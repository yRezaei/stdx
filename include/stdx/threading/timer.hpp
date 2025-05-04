#pragma once

#include <cstdint>
#include <atomic>
#include <map>
#include <mutex>
#include <queue>
#include <thread>
#include <memory>
#include <condition_variable>
#include <chrono>
#include <iostream>
#include "stdx/threading/thread_pool.hpp"
#include "stdx/light_callable.hpp"

#if defined(STDX_INCLUDE_EXPORT)
#include "stdx/stdx_export.hpp"
#else
#define STDX_API
#endif

namespace stdx
{
    namespace threading
    {
        // Forward declaration
        namespace _impl
        {
            class TimerImpl;
        } // namespace _impl
        

        class STDX_API Timer
        {
        public:
            using TimerHandle = uint64_t;

            explicit Timer(ThreadPool &thread_pool = ThreadPool::instance());
            ~Timer();

            // Non-copyable, non-movable
            Timer(const Timer &) = delete;
            Timer &operator=(const Timer &) = delete;
            Timer(Timer &&) = delete;
            Timer &operator=(Timer &&) = delete;

            template <typename F>
            void set_task(F &&userTask,
                          std::chrono::milliseconds delay,
                          std::chrono::milliseconds interval = std::chrono::milliseconds{0},
                          std::size_t priority = 0)
            {
                stop();

                task_ = stdx::LightCallable<void()>(std::forward<F>(userTask));

                delay_ = delay;
                interval_ = interval;
                priority_ = priority;
                valid_task_ = true;
            }

            void start();
            void stop();

        private:
            stdx::LightCallable<void()> task_;
            std::chrono::milliseconds delay_{0};
            std::chrono::milliseconds interval_{0};
            std::size_t priority_{0};
            bool valid_task_{false};
            TimerHandle handle_{0};
            ThreadPool &thread_pool_;
            _impl::TimerImpl &impl_;
        };

        namespace _impl
        {

            // TimerImpl moved outside Timer and using stdx::Singleton
            class STDX_API TimerImpl : public stdx::Singleton<TimerImpl>
            {
                friend class stdx::Singleton<TimerImpl>;
                friend class Timer;

            public:
                using TimerHandle = uint64_t;
                using Clock = std::chrono::steady_clock;
                using TimePoint = Clock::time_point;

                TimerHandle schedule(std::chrono::milliseconds delay,
                                     std::chrono::milliseconds interval,
                                     std::size_t priority,
                                     ThreadPool &thread_pool,
                                     const stdx::LightCallable<void()> &task);

                bool cancel(TimerHandle handle);

                ~TimerImpl();

            private:
                // Private constructor for singleton pattern
                TimerImpl();

                void worker_loop();

                // Task representation
                struct STDX_API TimerTask
                {
                    TimerHandle handle;
                    TimePoint next_execution;
                    std::chrono::milliseconds interval;
                    stdx::LightCallable<void()> task;
                    std::size_t priority;
                    ThreadPool *thread_pool;
                    bool cancelled = false;

                    bool recurring() const { return interval.count() > 0; }
                };
                using TimerTaskPtr = std::shared_ptr<TimerTask>;

                struct CompareTask
                {
                    bool operator()(const TimerTaskPtr &a, const TimerTaskPtr &b) const
                    {
                        return a->next_execution > b->next_execution;
                    }
                };

                std::atomic<bool> running_{true};
                std::atomic<TimerHandle> next_handle_{1};
                std::thread worker_thread_;
                std::mutex mutex_;
                std::condition_variable cv_;
                std::priority_queue<TimerTaskPtr, std::vector<TimerTaskPtr>, CompareTask> task_queue_;
                std::map<TimerHandle, TimerTaskPtr> task_map_;
            };
        }

    } // namespace threading
} // namespace stdx