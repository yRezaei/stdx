#pragma once

#include <thread>
#include <vector>
#include <atomic>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <algorithm>
#include <cassert>
#include <utility>
#include <type_traits>

/**
 * @file thread_pool.hpp
 * @brief Thread pool implementation that supports dynamic scaling based on the throughput ratio of a buffer.
 */

namespace stdx
{
    namespace threading
    {

        /**
         * @brief Per-thread context information used by the ThreadPool.
         *
         * Stores:
         * - The worker thread itself (`std::thread`).
         * - Two atomic flags:
         *   - `exit_requested`: Signals the thread to stop execution.
         *   - `active`: Indicates whether the thread is actively processing tasks or temporarily inactive.
         *
         * This struct disallows copying but supports move semantics to handle ownership of the `std::thread`.
         */
        struct ThreadContext
        {
            ThreadContext() = default;

            /// Deleted copy constructor
            ThreadContext(const ThreadContext &) = delete;
            /// Deleted copy assignment
            ThreadContext &operator=(const ThreadContext &) = delete;

            /**
             * @brief Move constructor.
             * @param other Another ThreadContext to move from.
             */
            ThreadContext(ThreadContext &&other) noexcept
                : thread(std::move(other.thread)), exit_requested(other.exit_requested.exchange(false, std::memory_order_relaxed)), active(other.active.exchange(false, std::memory_order_relaxed))
            {
            }

            /**
             * @brief Move assignment operator.
             * @param other Another ThreadContext to move from.
             * @return Reference to `this`.
             *
             * Joins the existing thread if `this` already owns one before overwriting.
             */
            ThreadContext &operator=(ThreadContext &&other) noexcept
            {
                if (thread.joinable())
                {
                    thread.join();
                }
                thread = std::move(other.thread);

                exit_requested.store(other.exit_requested.exchange(false, std::memory_order_relaxed),
                                     std::memory_order_relaxed);
                active.store(other.active.exchange(false, std::memory_order_relaxed),
                             std::memory_order_relaxed);
                return *this;
            }

            std::thread thread;                      ///< The worker thread handle.
            std::atomic<bool> exit_requested{false}; ///< Signals the thread to stop.
            std::atomic<bool> active{false};         ///< Indicates whether the thread is active.
        };

        /**
         * @brief A thread pool that manages a variable number of worker threads.
         *
         * @tparam BUFFER_T A buffer type with specific methods for push/pop, waiting, and throughput calculation.
         * @tparam BUFFER_ENTRY_T The type of items/tasks stored in the buffer. Must be callable if no custom task is provided.
         *
         * The pool spawns a separate monitor thread that periodically calls `throughput_ratio()`
         * on the buffer. Based on `spawn_ratio_threshold_` and `shrink_ratio_threshold_`,
         * it will activate or deactivate worker threads to match workload demands.
         */
        template <typename BUFFER_T, typename BUFFER_ENTRY_T>
        class ThreadPool
        {
            // These static_asserts confirm that BUFFER_T supports all required methods.
            static_assert(std::is_same_v<bool, decltype(std::declval<BUFFER_T>().empty())>,
                          "BUFFER_T must have bool empty() const");
            static_assert(std::is_same_v<std::size_t, decltype(std::declval<BUFFER_T>().size())>,
                          "BUFFER_T must have size_t size() const");
            static_assert(std::is_same_v<std::size_t, decltype(std::declval<BUFFER_T>().capacity())>,
                          "BUFFER_T must have size_t capacity() const");
            static_assert(std::is_same_v<bool, decltype(std::declval<BUFFER_T>().pop(std::declval<BUFFER_ENTRY_T &>()))>,
                          "BUFFER_T must have bool pop(BUFFER_ENTRY_T&)");
            static_assert(std::is_same_v<double, decltype(std::declval<BUFFER_T>().throughput_ratio())>,
                          "BUFFER_T must have double throughput_ratio()");
            static_assert(std::is_same_v<void, decltype(std::declval<BUFFER_T>().wait_for_item(std::declval<std::size_t>()))>,
                          "BUFFER_T must have void wait_for_item(size_t)");
            static_assert(std::is_same_v<void, decltype(std::declval<BUFFER_T>().signal_shutdown())>,
                          "BUFFER_T must have void signal_shutdown()");
            static_assert(std::is_same_v<bool, decltype(std::declval<BUFFER_T>().is_shutdown())>,
                          "BUFFER_T must have bool is_shutdown() const");

        public:
            /**
             * @brief Constructs a ThreadPool that processes callable items directly (no custom task function).
             *
             * Expects BUFFER_ENTRY_T to be callable. Uses hardware concurrency heuristics
             * for initial and maximum thread counts.
             */
            ThreadPool(
                BUFFER_T &buffer,
                std::size_t reserved_threads = std::max<std::size_t>(1, static_cast<std::size_t>(std::thread::hardware_concurrency()) / 2),
                std::size_t min_threads = 1,
                double spawn_ratio_threshold = 1.2,
                double shrink_ratio_threshold = 0.8,
                std::size_t max_threads = std::max<std::size_t>(1, static_cast<std::size_t>(std::thread::hardware_concurrency())),
                std::size_t monitor_interval_ms = 200,
                std::size_t spin_count = 100,
                std::size_t spawn_hysteresis_intervals = 2,
                std::size_t shrink_hysteresis_intervals = 2,
                bool enable_batch_scaling = false,
                double batch_scaling_factor = 1.0)
                : buffer_(buffer), task_([](BUFFER_ENTRY_T &item)
                                         {
                                             item(); // Because BUFFER_ENTRY_T must be callable if no task is provided
                                         }),
                  reserved_threads_(reserved_threads), min_threads_(min_threads), spawn_ratio_threshold_(spawn_ratio_threshold), shrink_ratio_threshold_(shrink_ratio_threshold), max_threads_(max_threads), monitor_interval_ms_(monitor_interval_ms), spin_count_(spin_count), spawn_hysteresis_intervals_(spawn_hysteresis_intervals), shrink_hysteresis_intervals_(shrink_hysteresis_intervals), enable_batch_scaling_(enable_batch_scaling), batch_scaling_factor_(batch_scaling_factor), running_(false), active_threads_(0), spawn_counter_(0), shrink_counter_(0)
            {
                static_assert(std::is_invocable<BUFFER_ENTRY_T>::value,
                              "BUFFER_ENTRY_T must be callable if no custom task is provided.");
                validate_parameters_();
                threads_.reserve(max_threads_);
            }

            /**
             * @brief Constructs a ThreadPool with a custom task function.
             *
             * @param buffer Reference to the buffer from which items are popped.
             * @param task   A callable that processes a popped item (`BUFFER_ENTRY_T&`).
             */
            ThreadPool(
                BUFFER_T &buffer,
                std::function<void(BUFFER_ENTRY_T &)> task,
                std::size_t reserved_threads = std::max<std::size_t>(1, static_cast<std::size_t>(std::thread::hardware_concurrency()) / 2),
                std::size_t min_threads = 1,
                double spawn_ratio_threshold = 1.2,
                double shrink_ratio_threshold = 0.8,
                std::size_t max_threads = std::max<std::size_t>(1, static_cast<std::size_t>(std::thread::hardware_concurrency())),
                std::size_t monitor_interval_ms = 200,
                std::size_t spin_count = 100,
                std::size_t spawn_hysteresis_intervals = 2,
                std::size_t shrink_hysteresis_intervals = 2,
                bool enable_batch_scaling = false,
                double batch_scaling_factor = 1.0)
                : buffer_(buffer), task_(std::move(task)), reserved_threads_(reserved_threads), min_threads_(min_threads), spawn_ratio_threshold_(spawn_ratio_threshold), shrink_ratio_threshold_(shrink_ratio_threshold), max_threads_(max_threads), monitor_interval_ms_(monitor_interval_ms), spin_count_(spin_count), spawn_hysteresis_intervals_(spawn_hysteresis_intervals), shrink_hysteresis_intervals_(shrink_hysteresis_intervals), enable_batch_scaling_(enable_batch_scaling), batch_scaling_factor_(batch_scaling_factor), running_(false), active_threads_(0), spawn_counter_(0), shrink_counter_(0)
            {
                validate_parameters_();
                threads_.reserve(max_threads_);
            }

            /// Destructor that stops the thread pool if still running.
            ~ThreadPool()
            {
                stop();
            }

            /// Deleted copy constructor
            ThreadPool(const ThreadPool &) = delete;
            /// Deleted copy assignment
            ThreadPool &operator=(const ThreadPool &) = delete;
            /// Deleted move constructor
            ThreadPool(ThreadPool &&) = delete;
            /// Deleted move assignment
            ThreadPool &operator=(ThreadPool &&) = delete;

            /**
             * @brief Starts the thread pool (if not already running).
             *
             * Launches the reserved threads immediately, and starts a monitor thread to handle
             * dynamic scaling (activating or deactivating workers based on throughput).
             */
            void start()
            {
                bool expected = false;
                if (running_.compare_exchange_strong(expected, true))
                {
                    std::unique_lock<std::mutex> lock(mutex_);

                    // Spawn the reserved number of threads
                    for (std::size_t i = 0; i < reserved_threads_; ++i)
                    {
                        launch_thread_();
                    }

                    // Immediately mark min_threads_ as active
                    active_threads_ = min_threads_;
                    for (std::size_t i = 0; i < min_threads_; ++i)
                    {
                        threads_[i].active.store(true, std::memory_order_relaxed);
                    }

                    // Start the monitor thread
                    monitor_thread_ = std::thread(&ThreadPool::monitor_loop_, this);
                }
            }

            /**
             * @brief Stops the thread pool (if it is running).
             *
             * Signals all threads to exit, joins them, clears the thread list, and joins the
             * monitor thread.
             */
            void stop()
            {
                bool expected = true;
                if (running_.compare_exchange_strong(expected, false))
                {
                    {
                        std::unique_lock<std::mutex> lock(mutex_);
                        for (auto &ctx : threads_)
                        {
                            ctx.exit_requested.store(true, std::memory_order_relaxed);
                        }
                        buffer_.signal_shutdown(); // Wake all waiting workers
                    }
                    {
                        std::unique_lock<std::mutex> lock(mutex_);
                        if (monitor_thread_.joinable())
                        {
                            monitor_thread_.join();
                        }
                    }
                    {
                        std::unique_lock<std::mutex> lock(mutex_);
                        for (auto &ctx : threads_)
                        {
                            if (ctx.thread.joinable())
                            {
                                ctx.thread.join();
                            }
                        }
                        threads_.clear();
                    }
                }
            }

            /**
             * @brief Returns how many threads are currently marked active.
             * @return The current count of active threads.
             */
            std::size_t get_active_threads() const
            {
                std::unique_lock<std::mutex> lock(mutex_);
                return active_threads_;
            }

            /**
             * @brief Returns the total number of threads owned by this pool (both active and inactive).
             * @return The total thread count.
             */
            std::size_t get_total_threads() const
            {
                std::unique_lock<std::mutex> lock(mutex_);
                return threads_.size();
            }

        private:
            /**
             * @brief Validates the constructor parameters, making corrections if needed.
             *
             * For example, ensures that `min_threads_ >= 1`, that `reserved_threads_ >= min_threads_`,
             * and that `spawn_ratio_threshold_` is strictly greater than `shrink_ratio_threshold_`.
             */
            void validate_parameters_()
            {
                if (min_threads_ < 1)
                {
                    min_threads_ = 1;
                }
                if (reserved_threads_ < min_threads_)
                {
                    reserved_threads_ = min_threads_;
                }
                if (max_threads_ < reserved_threads_)
                {
                    max_threads_ = reserved_threads_;
                }
                if (spawn_ratio_threshold_ <= shrink_ratio_threshold_)
                {
                    throw std::invalid_argument("spawn_ratio_threshold must be greater than shrink_ratio_threshold");
                }
            }

            /**
             * @brief Creates a new ThreadContext and spawns a worker thread.
             *
             * The thread runs `worker_loop_` with a reference to its own ThreadContext.
             */
            void launch_thread_()
            {
                threads_.emplace_back(ThreadContext{});
                ThreadContext &ctx = threads_.back();
                ctx.thread = std::thread(&ThreadPool::worker_loop_, this, std::ref(ctx));
            }

            /**
             * @brief Activates up to 'count' additional workers, respecting max_threads_.
             * @param count Number of new workers to activate if possible.
             */
            void activate_workers_(std::size_t count)
            {
                for (std::size_t i = 0; i < count && active_threads_ < max_threads_; ++i)
                {
                    // If we don't have enough total threads, first launch a new one
                    if (threads_.size() <= active_threads_)
                    {
                        launch_thread_();
                    }
                    // Then find an inactive thread and activate it
                    for (auto &ctx : threads_)
                    {
                        if (!ctx.active.load(std::memory_order_relaxed))
                        {
                            ctx.active.store(true, std::memory_order_relaxed);
                            ++active_threads_;
                            break;
                        }
                    }
                }
            }

            /**
             * @brief Deactivates up to 'count' active workers, respecting min_threads_.
             * @param count Number of workers to deactivate if possible.
             */
            void deactivate_workers_(std::size_t count)
            {
                for (std::size_t i = 0; i < count && active_threads_ > min_threads_; ++i)
                {
                    for (auto &ctx : threads_)
                    {
                        if (ctx.active.load(std::memory_order_relaxed))
                        {
                            ctx.active.store(false, std::memory_order_relaxed);
                            --active_threads_;
                            break;
                        }
                    }
                }
            }

            /**
             * @brief The main function each worker thread runs.
             * @param ctx Reference to this thread's `ThreadContext`.
             *
             * Each worker repeatedly:
             *  - Waits for items via `buffer_.wait_for_item()`
             *  - Pops an item (if available) and executes `task_`
             *  - Yields if the thread is currently inactive
             *  - Stops if `exit_requested` or buffer shutdown is signaled.
             */
            void worker_loop_(ThreadContext &ctx)
            {
                while (!ctx.exit_requested.load(std::memory_order_relaxed))
                {
                    // If this thread is inactive, yield until it becomes active or must exit
                    if (!ctx.active.load(std::memory_order_relaxed))
                    {
                        std::this_thread::yield();
                        continue;
                    }

                    // Wait for an item with a spin-then-sleep strategy
                    buffer_.wait_for_item(spin_count_);

                    // Check for shutdown or exit after waiting
                    if (ctx.exit_requested.load(std::memory_order_relaxed) || buffer_.is_shutdown())
                    {
                        break;
                    }

                    // Attempt to pop and process an item
                    BUFFER_ENTRY_T item{};
                    if (buffer_.pop(item))
                    {
                        try
                        {
                            task_(item);
                        }
                        catch (const std::exception &e)
                        {
                            // Log or handle exceptions here.
                        }
                    }
                }
            }

            /**
             * @brief Monitors the buffer's throughput ratio and scales the pool accordingly.
             *
             * Runs in a dedicated monitor thread until `running_` is set to false.
             */
            void monitor_loop_()
            {
                while (running_)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(monitor_interval_ms_));
                    if (!running_)
                    {
                        break;
                    }

                    double ratio = buffer_.throughput_ratio();
                    std::unique_lock<std::mutex> lock(mutex_);

                    // Check if we should spawn more threads
                    if (ratio > spawn_ratio_threshold_)
                    {
                        if (active_threads_ < max_threads_)
                        {
                            spawn_counter_++;
                            if (spawn_counter_ >= spawn_hysteresis_intervals_)
                            {
                                std::size_t threads_to_add = 1;
                                if (enable_batch_scaling_)
                                {
                                    double deviation = ratio - spawn_ratio_threshold_;
                                    threads_to_add = std::max<std::size_t>(1,
                                                                           static_cast<std::size_t>(deviation / batch_scaling_factor_));
                                }
                                activate_workers_(threads_to_add);
                                spawn_counter_ = 0;
                            }
                        }
                    }
                    else
                    {
                        spawn_counter_ = 0;
                    }

                    // Check if we should shrink (deactivate) threads
                    if (ratio < shrink_ratio_threshold_)
                    {
                        if (active_threads_ > min_threads_)
                        {
                            shrink_counter_++;
                            if (shrink_counter_ >= shrink_hysteresis_intervals_)
                            {
                                std::size_t threads_to_remove = 1;
                                if (enable_batch_scaling_)
                                {
                                    double deviation = shrink_ratio_threshold_ - ratio;
                                    threads_to_remove = std::max<std::size_t>(1,
                                                                              static_cast<std::size_t>(deviation / batch_scaling_factor_));
                                }
                                deactivate_workers_(threads_to_remove);
                                shrink_counter_ = 0;
                            }
                        }
                    }
                    else
                    {
                        shrink_counter_ = 0;
                    }
                }
            }

        private:
            BUFFER_T &buffer_;                           ///< The buffer from which tasks are popped
            std::function<void(BUFFER_ENTRY_T &)> task_; ///< Function to process each popped task

            std::size_t reserved_threads_;            ///< Number of threads to spawn immediately on start
            std::size_t min_threads_;                 ///< Minimum number of active threads
            double spawn_ratio_threshold_;            ///< Threshold for deciding if we should spawn more threads
            double shrink_ratio_threshold_;           ///< Threshold for deciding if we should deactivate threads
            std::size_t max_threads_;                 ///< Hard upper limit on total worker threads
            std::size_t monitor_interval_ms_;         ///< Interval for monitor thread sleep
            std::size_t spin_count_;                  ///< Spin count used in wait_for_item
            std::size_t spawn_hysteresis_intervals_;  ///< Consecutive intervals needed to trigger spawning
            std::size_t shrink_hysteresis_intervals_; ///< Consecutive intervals needed to trigger shrinking
            bool enable_batch_scaling_;               ///< If true, scale up/down by more than 1 thread
            double batch_scaling_factor_;             ///< Used in batch scaling calculations

            std::atomic<bool> running_;  ///< Indicates if the thread pool is running
            std::size_t active_threads_; ///< Current count of actively processing workers
            std::size_t spawn_counter_;  ///< Tracks consecutive intervals above spawn ratio
            std::size_t shrink_counter_; ///< Tracks consecutive intervals below shrink ratio

            std::vector<ThreadContext> threads_; ///< All worker threads (both active and inactive)
            std::thread monitor_thread_;         ///< Dedicated thread for dynamic scaling
            mutable std::mutex mutex_;           ///< Protects internal data (thread list, counters)
            std::condition_variable cv_;         ///< Unused in this version, but left for future usage
        };

    } // namespace threading
} // namespace stdx
