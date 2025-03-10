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
#include <chrono>
#include <stdexcept>

/**
 * @file thread_pool.hpp
 * @brief Thread pool implementation that supports dynamic scaling based on the throughput ratio of a buffer,
 *        including optional batch-pop support.
 */

namespace stdx
{
    namespace threading
    {
        template <typename B, typename T>
        struct has_pop_batch_method
        {
        private:
            template <typename U>
            static auto test(int) -> decltype(std::declval<U>().pop_batch(std::declval<std::vector<T> &>(),
                                                                          std::declval<std::size_t>()),
                                              std::true_type());

            template <typename>
            static std::false_type test(...);

        public:
            static constexpr bool value = decltype(test<B>(0))::value;
        };

        struct ThreadContext
        {
            ThreadContext() = default;

            ThreadContext(const ThreadContext &) = delete;
            ThreadContext &operator=(const ThreadContext &) = delete;

            ThreadContext(ThreadContext &&other) noexcept
            {
                if (other.thread.joinable())
                {
                    if (other.exit_requested.load())
                    {
                        other.thread.join();
                    }
                    else
                    {
                        thread = std::move(other.thread);
                    }
                }
                exit_requested.store(other.exit_requested.exchange(false));
                active.store(other.active.exchange(false));
            }

            ThreadContext &operator=(ThreadContext &&other) noexcept
            {
                if (thread.joinable())
                {
                    thread.join();
                }
                thread = std::move(other.thread);
                exit_requested.store(other.exit_requested.exchange(false));
                active.store(other.active.exchange(false));
                return *this;
            }

            std::thread thread;
            std::atomic<bool> exit_requested{false};
            std::atomic<bool> active{false};
            bool exit_requested_cache = false;
            bool active_cache = false;
        };

        /**
         * @tparam BUFFER_T       A buffer type that supports push/pop, pop_batch, wait_for_item, throughput_ratio, etc.
         * @tparam BUFFER_ENTRY_T The item type stored in BUFFER_T (tasks or data to be processed).
         */
        template <typename BUFFER_T, typename BUFFER_ENTRY_T, bool POP_BATCH_ENABLED = false>
        class ThreadPool
        {
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
            static_assert(!POP_BATCH_ENABLED || has_pop_batch_method<BUFFER_T, BUFFER_ENTRY_T>::value,
                          "BUFFER_T must have pop_batch method if pop_batch is enabled");

        public:
            /**
             * @brief Constructs a ThreadPool expecting BUFFER_ENTRY_T to be callable (no custom task functor).
             */
            explicit ThreadPool(
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
                double batch_scaling_factor = 1.0,
                std::size_t batch_min_size = 10,
                std::size_t batch_max_count = 100,
                std::size_t batch_timeout_ms = 5000)
                : buffer_(buffer), task_([](BUFFER_ENTRY_T &item)
                                         { item(); }),
                  reserved_threads_(reserved_threads), min_threads_(min_threads),
                  spawn_ratio_threshold_(spawn_ratio_threshold), shrink_ratio_threshold_(shrink_ratio_threshold),
                  max_threads_(max_threads), monitor_interval_ms_(monitor_interval_ms),
                  spin_count_(spin_count), spawn_hysteresis_intervals_(spawn_hysteresis_intervals),
                  shrink_hysteresis_intervals_(shrink_hysteresis_intervals),
                  enable_batch_scaling_(enable_batch_scaling), batch_scaling_factor_(batch_scaling_factor),
                  running_(false), spawn_counter_(0), shrink_counter_(0),
                  batch_min_size_(batch_min_size), batch_max_count_(batch_max_count),
                  batch_timeout_ms_(batch_timeout_ms)
            {
                static_assert(std::is_invocable<BUFFER_ENTRY_T>::value,
                              "BUFFER_ENTRY_T must be callable if no custom task is provided.");
                validate_parameters_();
                threads_.reserve(max_threads_);
            }

            /**
             * @brief Constructs a ThreadPool with a custom task function that processes items.
             */
            explicit ThreadPool(
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
                double batch_scaling_factor = 1.0,
                std::size_t batch_min_size = 10,
                std::size_t batch_max_count = 100,
                std::size_t batch_timeout_ms = 5000)
                : buffer_(buffer), task_(std::move(task)),
                  reserved_threads_(reserved_threads), min_threads_(min_threads),
                  spawn_ratio_threshold_(spawn_ratio_threshold), shrink_ratio_threshold_(shrink_ratio_threshold),
                  max_threads_(max_threads), monitor_interval_ms_(monitor_interval_ms),
                  spin_count_(spin_count), spawn_hysteresis_intervals_(spawn_hysteresis_intervals),
                  shrink_hysteresis_intervals_(shrink_hysteresis_intervals),
                  enable_batch_scaling_(enable_batch_scaling), batch_scaling_factor_(batch_scaling_factor),
                  running_(false), spawn_counter_(0), shrink_counter_(0),
                  batch_min_size_(batch_min_size), batch_max_count_(batch_max_count),
                  batch_timeout_ms_(batch_timeout_ms)
            {
                validate_parameters_();
                threads_.reserve(max_threads_);
            }

            ~ThreadPool()
            {
                stop();
            }

            ThreadPool(const ThreadPool &) = delete;
            ThreadPool &operator=(const ThreadPool &) = delete;

            /**
             * @brief Move constructor.
             *
             * If the other pool is still running, it will be stopped. Then we move its internal state.
             * We also require that both pools reference the exact same buffer.
             */
            ThreadPool(ThreadPool &&other) noexcept
                : buffer_(other.buffer_), // It's a reference; must match
                  task_(std::move(other.task_)),
                  reserved_threads_(other.reserved_threads_),
                  min_threads_(other.min_threads_),
                  spawn_ratio_threshold_(other.spawn_ratio_threshold_),
                  shrink_ratio_threshold_(other.shrink_ratio_threshold_),
                  max_threads_(other.max_threads_),
                  monitor_interval_ms_(other.monitor_interval_ms_),
                  spin_count_(other.spin_count_),
                  spawn_hysteresis_intervals_(other.spawn_hysteresis_intervals_),
                  shrink_hysteresis_intervals_(other.shrink_hysteresis_intervals_),
                  enable_batch_scaling_(other.enable_batch_scaling_),
                  batch_scaling_factor_(other.batch_scaling_factor_),
                  running_(false), // We'll start fresh
                  spawn_counter_(other.spawn_counter_),
                  shrink_counter_(other.shrink_counter_),
                  batch_min_size_(other.batch_min_size_),
                  batch_max_count_(other.batch_max_count_),
                  batch_timeout_ms_(other.batch_timeout_ms_)
            {
                static_assert(&buffer_ == &other.buffer_, "Cannot move between ThreadPools that reference different buffers.");

                if (other.running_.load())
                {
                    other.stop();
                }
                // Move the vector of ThreadContext
                threads_ = std::move(other.threads_);
                // The old monitor thread
                if (other.monitor_thread_.joinable())
                {
                    // Should already be joined by stop(), but just in case:
                    other.monitor_thread_.join();
                }

                // Now that we've moved everything, the other pool is effectively inert:
                other.spawn_counter_ = 0;
                other.shrink_counter_ = 0;
            }

            /**
             * @brief Move assignment operator.
             */
            ThreadPool &operator=(ThreadPool &&other) noexcept
            {
                if (this != &other)
                {
                    // We cannot change the buffer reference after construction
                    // so we require that both pools reference the same buffer.
                    static_assert(&buffer_ == &other.buffer_, "Cannot move between ThreadPools that reference different buffers.");

                    stop(); // Ensure this is not running
                    if (other.running_.load())
                    {
                        other.stop();
                    }

                    // Move simple fields
                    task_ = std::move(other.task_);
                    reserved_threads_ = other.reserved_threads_;
                    min_threads_ = other.min_threads_;
                    spawn_ratio_threshold_ = other.spawn_ratio_threshold_;
                    shrink_ratio_threshold_ = other.shrink_ratio_threshold_;
                    max_threads_ = other.max_threads_;
                    monitor_interval_ms_ = other.monitor_interval_ms_;
                    spin_count_ = other.spin_count_;
                    spawn_hysteresis_intervals_ = other.spawn_hysteresis_intervals_;
                    shrink_hysteresis_intervals_ = other.shrink_hysteresis_intervals_;
                    enable_batch_scaling_ = other.enable_batch_scaling_;
                    batch_scaling_factor_ = other.batch_scaling_factor_;
                    running_.store(false);
                    spawn_counter_ = other.spawn_counter_;
                    shrink_counter_ = other.shrink_counter_;
                    batch_min_size_ = other.batch_min_size_;
                    batch_max_count_ = other.batch_max_count_;
                    batch_timeout_ms_ = other.batch_timeout_ms_;

                    // Move the threads
                    threads_ = std::move(other.threads_);
                    if (other.monitor_thread_.joinable())
                    {
                        other.monitor_thread_.join();
                    }
                    other.spawn_counter_ = 0;
                    other.shrink_counter_ = 0;
                }
                return *this;
            }

            void start()
            {
                bool expected = false;
                if (running_.compare_exchange_strong(expected, true))
                {
                    std::unique_lock<std::mutex> lock(mutex_);

                    for (std::size_t i = 0; i < reserved_threads_; ++i)
                    {
                        launch_thread_();
                    }

                    // Mark min_threads_ as active (or all if reserved_threads < min_threads)
                    active_threads_.store(std::max(min_threads_, reserved_threads_));
                    for (std::size_t i = 0; i < get_active_threads() && i < threads_.size(); ++i)
                    {
                        threads_[i].active.store(true);
                    }

                    monitor_thread_ = std::thread(&ThreadPool::monitor_loop_, this);
                }
            }

            void stop()
            {
                bool expected = true;
                if (running_.compare_exchange_strong(expected, false))
                {
                    {
                        std::unique_lock<std::mutex> lock(mutex_);
                        for (auto &ctx : threads_)
                        {
                            ctx.exit_requested.store(true);
                        }
                        buffer_.signal_shutdown();
                    }
                    {
                        std::unique_lock<std::mutex> lock(idle_mutex_);
                        idle_cv_.notify_all(); // Wake up all idle threads
                    }
                    {
                        std::unique_lock<std::mutex> lock(mutex_);
                        if (monitor_thread_.joinable())
                            monitor_thread_.join();
                    }
                    {
                        std::unique_lock<std::mutex> lock(mutex_);
                        for (auto &ctx : threads_)
                        {
                            if (ctx.thread.joinable())
                                ctx.thread.join();
                        }
                        threads_.clear();
                    }
                }
            }

            void wait()
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this]()
                         { return buffer_.empty() && (get_active_threads() == 0 ||
                                                      std::all_of(threads_.begin(), threads_.end(),
                                                                  [](const ThreadContext &ctx)
                                                                  { return !ctx.active.load(); })); });
            }

            std::size_t get_active_threads() const
            {
                return active_threads_.load();
            }

            std::size_t get_total_threads() const
            {
                std::unique_lock<std::mutex> lock(mutex_);
                return threads_.size();
            }

        private:
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

            void launch_thread_()
            {
                threads_.emplace_back(ThreadContext{});
                ThreadContext &ctx = threads_.back();
                ctx.thread = std::thread(&ThreadPool::worker_loop_, this, std::ref(ctx));
            }

            void activate_workers_(std::size_t count)
            {
                // Called with mutex_ already locked by monitor_loop_
                for (std::size_t i = 0; i < count && get_active_threads() < max_threads_; ++i)
                {
                    if (threads_.size() <= get_active_threads())
                    {
                        launch_thread_();
                    }

                    for (auto &ctx : threads_)
                    {
                        if (!ctx.active.load())
                        {
                            ctx.active.store(true);
                            active_threads_.fetch_add(1); // Fixed from fetch_sub to fetch_add
                            idle_cv_.notify_one();        // Wake up one idle thread
                            break;
                        }
                    }
                }
            }

            void deactivate_workers_(std::size_t count)
            {
                // Called with mutex_ already locked by monitor_loop_
                for (std::size_t i = 0; i < count && get_active_threads() > min_threads_; ++i)
                {
                    for (auto &ctx : threads_)
                    {
                        if (ctx.active.load())
                        {
                            ctx.active.store(false);
                            active_threads_.fetch_sub(1); // Correctly decrement
                            break;
                        }
                    }
                }
            }

            /**
             * @brief Worker thread entry point.
             *
             * Splits out into smaller subroutines:
             *  - wait_for_work_() to spin/sleep until items appear or exit is requested
             *  - process_items_(ctx) which decides whether to pop single or batch
             *  - pop_and_process_one_(...) or pop_and_process_batch_(...) for actual consumption
             */
            void worker_loop_(ThreadContext &ctx)
            {
                using clock = std::chrono::steady_clock;
                auto last_batch_start = clock::now();

                while (!ctx.exit_requested.load())
                {
                    if (!ctx.active.load())
                    {
                        std::unique_lock<std::mutex> lock(idle_mutex_);
                        idle_cv_.wait(lock, [&ctx, this]()
                                      { return ctx.active.load() || ctx.exit_requested.load() || !running_.load(); });
                        if (ctx.exit_requested.load() || !running_.load())
                        {
                            break;
                        }
                    }
                    ctx.active_cache = true; // Update cache after waking up

                    // Wait for at least one item (or shutdown signal)
                    wait_for_work_(ctx);
                    if (ctx.exit_requested.load() || buffer_.is_shutdown())
                    {
                        break;
                    }

                    if constexpr (POP_BATCH_ENABLED)
                    {
                        auto now = clock::now();
                        auto ms_since_last = std::chrono::duration_cast<std::chrono::milliseconds>(
                            now - last_batch_start);
                        if (buffer_.size() < batch_min_size_ && ms_since_last.count() < (long)batch_timeout_ms_)
                        {
                            pop_and_process_one_(ctx);
                        }
                        else
                        {
                            pop_and_process_batch_(ctx);
                            last_batch_start = clock::now();
                        }
                    }
                    else
                    {
                        pop_and_process_one_(ctx);
                    }
                }
            }

            /**
             * @brief Waits for items to appear in the buffer or an exit signal.
             */
            void wait_for_work_(ThreadContext &ctx)
            {
                // The ring buffer's wait_for_item will spin/sleep until at least one item
                // is available OR until it notices shutdown_. We also check exit_requested
                // in the loop so we can break early if needed.
                buffer_.wait_for_item(spin_count_);
            }

            /**
             * @brief Pop a single item and run the task, if any item is available.
             */
            void pop_and_process_one_(ThreadContext &ctx)
            {
                BUFFER_ENTRY_T item{};
                if (buffer_.pop(item))
                {
                    try
                    {
                        task_(item);
                    }
                    catch (...)
                    {
                        // Handle/log exception
                    }
                }
            }

            /**
             * @brief Pop up to `batch_max_count_` items at once and process them.
             */
            void pop_and_process_batch_(ThreadContext &ctx)
            {
                std::vector<BUFFER_ENTRY_T> items;
                items.reserve(batch_max_count_);
                std::size_t popped = buffer_.pop_batch(items, batch_max_count_);
                if (popped != 0)
                {
                    for (auto &itm : items)
                    {
                        try
                        {
                            task_(itm);
                        }
                        catch (...)
                        {
                            // Handle/log exception
                        }
                    }
                }
            }

            /**
             * @brief Thread that periodically checks the buffer's throughput ratio
             *        to spawn or shrink workers.
             */
            void monitor_loop_()
            {
                std::size_t current_interval_ms = monitor_interval_ms_;
                double last_ratio = 1.0;
                std::size_t stable_count = 0;
                const std::size_t max_stable_count = 5;
                const double ratio_change_threshold = 0.1;
                const std::size_t min_interval_ms = 10;
                const std::size_t max_interval_ms = 1000;

                while (running_)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(current_interval_ms));
                    if (!running_)
                        break;

                    double ratio = buffer_.throughput_ratio();
                    bool ratio_stable = std::abs(ratio - last_ratio) < ratio_change_threshold;

                    // Dynamically adjust monitoring interval
                    if (ratio_stable)
                    {
                        stable_count++;
                        if (stable_count >= max_stable_count)
                        {
                            // Increase interval (slower monitoring) when conditions are stable
                            current_interval_ms = std::min(current_interval_ms * 2, max_interval_ms);
                            stable_count = 0;
                        }
                    }
                    else
                    {
                        // Reset stability counter and decrease interval (faster monitoring) when conditions change
                        stable_count = 0;
                        current_interval_ms = std::max(current_interval_ms / 2, min_interval_ms);
                    }

                    std::unique_lock<std::mutex> lock(mutex_);

                    // SPIN UP?
                    if (ratio > spawn_ratio_threshold_)
                    {
                        if (get_active_threads() < max_threads_)
                        {
                            spawn_counter_++;
                            if (spawn_counter_ >= spawn_hysteresis_intervals_)
                            {
                                std::size_t threads_to_add = 1;
                                if (enable_batch_scaling_)
                                {
                                    double diff = ratio - spawn_ratio_threshold_;
                                    threads_to_add = std::max<std::size_t>(
                                        1,
                                        static_cast<std::size_t>(diff / batch_scaling_factor_));
                                }
                                activate_workers_(threads_to_add);
                                spawn_counter_ = 0;
                                current_interval_ms = std::max(current_interval_ms / 2, min_interval_ms); // Monitor more frequently after scaling
                            }
                        }
                    }
                    else
                    {
                        spawn_counter_ = 0;
                    }

                    // SHRINK?
                    if (ratio < shrink_ratio_threshold_)
                    {
                        if (get_active_threads() > min_threads_)
                        {
                            shrink_counter_++;
                            if (shrink_counter_ >= shrink_hysteresis_intervals_)
                            {
                                std::size_t threads_to_remove = 1;
                                if (enable_batch_scaling_)
                                {
                                    double diff = shrink_ratio_threshold_ - ratio;
                                    threads_to_remove = std::max<std::size_t>(
                                        1,
                                        static_cast<std::size_t>(diff / batch_scaling_factor_));
                                }
                                deactivate_workers_(threads_to_remove);
                                shrink_counter_ = 0;
                                current_interval_ms = std::max(current_interval_ms / 2, min_interval_ms); // Monitor more frequently after scaling
                            }
                        }
                    }
                    else
                    {
                        shrink_counter_ = 0;
                    }

                    // Update last ratio
                    last_ratio = ratio;
                }
            }

        private:
            BUFFER_T &buffer_;
            std::function<void(BUFFER_ENTRY_T &)> task_;

            std::size_t reserved_threads_;
            std::size_t min_threads_;
            double spawn_ratio_threshold_;
            double shrink_ratio_threshold_;
            std::size_t max_threads_;
            std::size_t monitor_interval_ms_;
            std::size_t spin_count_;
            std::size_t spawn_hysteresis_intervals_;
            std::size_t shrink_hysteresis_intervals_;
            bool enable_batch_scaling_;
            double batch_scaling_factor_;

            std::atomic<bool> running_;
            std::atomic<std::size_t> active_threads_{0};
            std::size_t spawn_counter_;
            std::size_t shrink_counter_;

            std::vector<ThreadContext> threads_;
            std::thread monitor_thread_;
            mutable std::mutex mutex_;
            std::condition_variable cv_;

            // New batch-pop configuration:
            std::size_t batch_min_size_;
            std::size_t batch_max_count_;
            std::size_t batch_timeout_ms_;

            std::condition_variable idle_cv_; // Condition variable for idle threads
            std::mutex idle_mutex_;           // Mutex for idle_cv_
        };

        /**
         * @brief Enumerates different usage scenarios for a thread pool configuration.
         */
        enum class PoolScenario
        {
            BATCH_PROCESS_WITH_FEW_THREADS, ///< Logging-like scenario: batch pops, fewer threads initially, high spawn threshold.
            REALTIME_NO_BATCH               ///< Real-time-like scenario: no batch pop, spin up threads quickly for minimal delay.
        };

        /**
         * @brief Create a thread pool with preset parameters depending on the scenario.
         *
         * @tparam BUFFER_T      The buffer type.
         * @tparam BUFFER_ITEM_T The item type in the buffer.
         * @param buffer         Reference to the buffer instance.
         * @param scenario       Which preset scenario to configure for.
         *
         * @return A ThreadPool appropriately configured for the selected scenario.
         */
        template <typename BUFFER_T, typename BUFFER_ITEM_T>
        static auto create_thread_pool(BUFFER_T &buffer, PoolScenario scenario)
        {
            // If scenario #1 (BATCH_PROCESS_WITH_FEW_THREADS), we require the buffer to support pop_batch:
            if (scenario == PoolScenario::BATCH_PROCESS_WITH_FEW_THREADS)
            {
                static_assert(has_pop_batch_method<BUFFER_T, BUFFER_ITEM_T>::value,
                              "BATCH_PROCESS_WITH_FEW_THREADS scenario requires pop_batch support in the buffer");

                // Example parameters geared toward batching:
                //  - Start with a small number of threads.
                //  - Only scale up if throughput_ratio gets close to or above capacity.
                //  - Larger monitor interval, because we don't need super-fast scaling.
                using PoolType = ThreadPool<BUFFER_T, BUFFER_ITEM_T, true>;
                return PoolType(buffer,
                                /*reserved_threads*/ 1,
                                /*min_threads*/ 1,
                                /*spawn_ratio_threshold*/ 1.8, // Only spawn new threads if ratio > 1.8
                                /*shrink_ratio_threshold*/ 0.5,
                                /*max_threads*/ std::max<std::size_t>(2, std::thread::hardware_concurrency()),
                                /*monitor_interval_ms*/ 500,
                                /*spin_count*/ 100,
                                /*spawn_hysteresis_intervals*/ 3,
                                /*shrink_hysteresis_intervals*/ 2,
                                /*enable_batch_scaling*/ true,
                                /*batch_scaling_factor*/ 0.5, // adjust as desired
                                /*batch_min_size*/ 10,
                                /*batch_max_count*/ 200,
                                /*batch_timeout_ms*/ 3000);
            }
            else
            {
                // scenario #2 (REALTIME_NO_BATCH)
                //  - No batch pop.
                //  - Possibly start with more threads or spin up quickly.
                //  - Lower spawn threshold to scale up faster.
                using PoolType = ThreadPool<BUFFER_T, BUFFER_ITEM_T, false>;
                return PoolType(buffer,
                                /*reserved_threads*/ std::max<std::size_t>(2, std::thread::hardware_concurrency() / 2),
                                /*min_threads*/ 2,
                                /*spawn_ratio_threshold*/ 1.05, // scale quickly
                                /*shrink_ratio_threshold*/ 0.9,
                                /*max_threads*/ std::max<std::size_t>(2, std::thread::hardware_concurrency()),
                                /*monitor_interval_ms*/ 100,
                                /*spin_count*/ 100,
                                /*spawn_hysteresis_intervals*/ 1,
                                /*shrink_hysteresis_intervals*/ 1,
                                /*enable_batch_scaling*/ false,
                                /*batch_scaling_factor*/ 1.0, // unused if no batch
                                /*batch_min_size*/ 1,         // unused
                                /*batch_max_count*/ 1,        // unused
                                /*batch_timeout_ms*/ 1000);   // unused
            }
        }

    } // namespace threading
} // namespace stdx
