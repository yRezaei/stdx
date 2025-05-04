#pragma once

#include <atomic>
#include <vector>
#include <future>
#include <thread>
#include <mutex>
#include <memory>
#include <condition_variable>
#include <stdexcept>
#include "stdx/concurrency/ring_buffer.hpp"
#include "stdx/light_callable.hpp"
#include "stdx/singleton.hpp"

#if defined(STDX_INCLUDE_EXPORT)
#include "stdx/stdx_export.hpp"
#else
#define STDX_API
#endif

namespace stdx
{
    namespace threading
    {
        //=========================
        // Exceptions
        //=========================
        class STDX_API ThreadPoolStopped : public std::exception
        {
        public:
            const char *what() const noexcept override;
        };

        class STDX_API BufferFull : public std::exception
        {
        public:
            const char *what() const noexcept override;
        };

        class STDX_API InvalidPriorityIndex : public std::exception
        {
        public:
            const char *what() const noexcept override;
        };

        enum class ShutdownMode
        {
            Immediate,
            Graceful
        };

        enum class PoolState
        {
            idle,
            Running,
            Stopping
        };

        /**
         * @brief Statistics collected by the ThreadPool
         */
        struct STDX_API ThreadPoolStats
        {
            // ------------------------
            // 1) The Atomic Members
            // ------------------------
            std::atomic<size_t> tasks_submitted{0};
            std::atomic<size_t> tasks_completed{0};
            std::atomic<size_t> submission_failures{0};

            std::vector<std::unique_ptr<std::atomic<size_t>>> tasks_by_priority;
            std::vector<std::unique_ptr<std::atomic<size_t>>> buffer_near_full_events;

            std::atomic<size_t> wait_count{0};
            std::atomic<size_t> task_execution_total_us{0};
            std::atomic<size_t> exception_count{0};

            // ------------------------
            // 2) Non-atomic Data
            // ------------------------
            std::vector<size_t> tasks_per_thread;
            std::vector<size_t> thread_wakeup_count;

            std::atomic<size_t> thread_sleep_count{0};

            /**
             * @brief Construct ThreadPoolStats with specified dimensions
             */
            ThreadPoolStats(std::uint32_t num_threads, std::uint32_t nr_of_priorities);

            /**
             * @brief Reset all statistics to zero
             */
            void reset();
        };

        /**
         * @brief A snapshot of ThreadPool stats at a point in time
         */
        struct STDX_API ThreadPoolSnapshot
        {
            size_t tasks_submitted;
            size_t tasks_completed;
            size_t submission_failures;

            std::vector<size_t> tasks_by_priority;
            std::vector<size_t> buffer_near_full_events;

            size_t wait_count;
            double avg_execution_time_us;
            size_t total_execution_time_us;
            size_t exception_count;
            std::vector<size_t> tasks_per_thread;
            std::vector<size_t> thread_wakeup_count;
            size_t thread_sleep_count;

            std::vector<double> buffer_usage_pct;
        };

        /**
         * @brief Encapsulates a user-provided callable and a std::promise<R>.
         */
        template <typename R, typename F>
        class STDX_API TaskPackage
        {
        public:
            TaskPackage(std::promise<R> &&promise, F &&func)
                : promise_(std::move(promise)), func_(std::forward<F>(func))
            {
            }

            TaskPackage(const TaskPackage &) = delete;
            TaskPackage &operator=(const TaskPackage &) = delete;
            TaskPackage(TaskPackage &&) noexcept = default;
            TaskPackage &operator=(TaskPackage &&) noexcept = default;

            void operator()()
            {
                try
                {
                    if constexpr (std::is_void_v<R>)
                    {
                        func_();
                        promise_.set_value();
                    }
                    else
                    {
                        R result = func_();
                        promise_.set_value(std::move(result));
                    }
                }
                catch (...)
                {
                    promise_.set_exception(std::current_exception());
                    throw;
                }
            }

        private:
            std::promise<R> promise_;
            F func_;
        };

        /**
         * @brief A lock-free thread pool with fixed threads and multiple ring buffers.
         */
        class STDX_API ThreadPool : public stdx::Singleton<ThreadPool>
        {
            friend class stdx::Singleton<ThreadPool>;

        private:
            explicit ThreadPool(std::uint32_t num_threads = std::thread::hardware_concurrency(),
                              std::uint32_t buffer_capacity = 1024,
                              std::uint32_t nr_of_priorities = 3);

        public:
            ~ThreadPool();
            
            std::size_t get_thread_count() const;
            void start();
            void stop(ShutdownMode mode = ShutdownMode::Graceful);
            
            /**
             * @brief Submit a task with the given priority_index.
             */
            template <typename F>
            auto submit(std::uint32_t priority_index, F &&func)
                -> std::future<std::invoke_result_t<F>>
            {
                using R = std::invoke_result_t<F>;

                std::promise<R> promise;
                std::future<R> future = promise.get_future();

                if (pool_state_.load(std::memory_order_acquire) != PoolState::Running)
                {
                    promise.set_exception(std::make_exception_ptr(ThreadPoolStopped()));
                    return future;
                }

                // Validate priority index
                if (priority_index >= nr_of_priorities_)
                {
                    promise.set_exception(std::make_exception_ptr(InvalidPriorityIndex()));
                    return future;
                }

                // Package the callable + promise
                using TaskT = TaskPackage<R, std::decay_t<F>>;
                TaskT task_pkg(std::move(promise), std::forward<F>(func));

                stdx::LightCallable<void()> wrapped([pkg = std::move(task_pkg)]() mutable
                                                    { pkg(); });

                // Attempt to push into ring buffer
                bool ok = ring_buffers_[priority_index]->push(wrapped);
                if (!ok)
                {
                    stats_.submission_failures.fetch_add(1, std::memory_order_relaxed);
                    std::promise<R> error_promise;
                    error_promise.set_exception(std::make_exception_ptr(BufferFull()));
                    return error_promise.get_future();
                }

                pending_tasks_.fetch_add(1, std::memory_order_relaxed);
                stats_.tasks_submitted.fetch_add(1, std::memory_order_relaxed);
                stats_.tasks_by_priority[priority_index]->fetch_add(1, std::memory_order_relaxed);

                notify_one_thread();
                return future;
            }

            void record_exception();
            ThreadPoolSnapshot get_stats() const;
            void reset_stats();
            void set_exception_handler(stdx::LightCallable<void(std::exception_ptr)> handler);

        private:
            void notify_one_thread();
            bool has_tasks() const;
            void worker_loop(std::uint32_t thread_id);

        private:
            std::uint32_t num_threads_;
            std::uint32_t buffer_capacity_;
            std::uint32_t nr_of_priorities_;

            std::vector<std::unique_ptr<stdx::concurrency::RingBuffer<stdx::LightCallable<void()>>>> ring_buffers_;

            // Worker threads
            std::vector<std::thread> threads_;
            std::mutex wakeup_mutex_;
            std::condition_variable wakeup_cv_;

            // Stats
            ThreadPoolStats stats_;

            std::atomic<size_t> pending_tasks_{0};
            std::atomic<PoolState> pool_state_{PoolState::idle};
            std::atomic<size_t> sleeping_threads_{0};

            stdx::LightCallable<void(std::exception_ptr)> exception_handler_;
        };

    } // namespace threading
} // namespace stdx