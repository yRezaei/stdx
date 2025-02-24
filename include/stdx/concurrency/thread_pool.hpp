#ifndef B1D9354E_67C1_4440_BFF9_EE30213D0BB8
#define B1D9354E_67C1_4440_BFF9_EE30213D0BB8

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

namespace stdx
{
    namespace concurrency
    {
        struct ThreadContext
        {
            ThreadContext() = default;

            // Disallow copying
            ThreadContext(const ThreadContext &) = delete;
            ThreadContext &operator=(const ThreadContext &) = delete;

            // Provide a custom move constructor
            ThreadContext(ThreadContext &&other) noexcept
                : thread(std::move(other.thread))
                  // For atomics, decide if you want to read/clear or just copy the old bits:
                  ,
                  exit_requested(other.exit_requested.exchange(false, std::memory_order_relaxed)), active(other.active.exchange(false, std::memory_order_relaxed))
            {
            }

            // Provide a custom move assignment
            ThreadContext &operator=(ThreadContext &&other) noexcept
            {
                // If we already have a std::thread we need to join or detach it before overwriting
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

            std::thread thread;
            std::atomic<bool> exit_requested{false};
            std::atomic<bool> active{false};
        };

        template <typename BUFFER_T, typename BUFFER_ENTRY_T>
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

        public:
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
                : buffer_(buffer),
                  task_([](BUFFER_ENTRY_T &item)
                        {
                            item(); // Because we "static_assert" that BUFFER_ENTRY_T is invocable
                        }),
                  reserved_threads_(reserved_threads), min_threads_(min_threads), spawn_ratio_threshold_(spawn_ratio_threshold), shrink_ratio_threshold_(shrink_ratio_threshold), max_threads_(max_threads), monitor_interval_ms_(monitor_interval_ms), spin_count_(spin_count), spawn_hysteresis_intervals_(spawn_hysteresis_intervals), shrink_hysteresis_intervals_(shrink_hysteresis_intervals), enable_batch_scaling_(enable_batch_scaling), batch_scaling_factor_(batch_scaling_factor), running_(false), active_threads_(0), spawn_counter_(0), shrink_counter_(0)
            {
                static_assert(std::is_invocable<BUFFER_ENTRY_T>::value,
                              "BUFFER_ENTRY_T must be callable when no task is provided.");
                validate_parameters_();
                threads_.reserve(max_threads_);
            }

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
                : buffer_(buffer),
                  task_(std::move(task)),
                  reserved_threads_(reserved_threads),
                  min_threads_(min_threads),
                  spawn_ratio_threshold_(spawn_ratio_threshold),
                  shrink_ratio_threshold_(shrink_ratio_threshold),
                  max_threads_(max_threads),
                  monitor_interval_ms_(monitor_interval_ms),
                  spin_count_(spin_count),
                  spawn_hysteresis_intervals_(spawn_hysteresis_intervals),
                  shrink_hysteresis_intervals_(shrink_hysteresis_intervals),
                  enable_batch_scaling_(enable_batch_scaling),
                  batch_scaling_factor_(batch_scaling_factor),
                  running_(false),
                  active_threads_(0),
                  spawn_counter_(0),
                  shrink_counter_(0)
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

            ThreadPool(ThreadPool &&) = delete; // unless you truly want a movable pool
            ThreadPool &operator=(ThreadPool &&) = delete;

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
                    active_threads_ = min_threads_;
                    for (std::size_t i = 0; i < min_threads_; ++i)
                    {
                        threads_[i].active.store(true, std::memory_order_relaxed);
                    }
                    // No cv_.notify_all() needed; buffer notification handles waking workers
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

            std::size_t get_active_threads() const
            {
                std::unique_lock<std::mutex> lock(mutex_);
                return active_threads_;
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
                    min_threads_ = 1;
                if (reserved_threads_ < min_threads_)
                    reserved_threads_ = min_threads_;
                if (max_threads_ < reserved_threads_)
                    max_threads_ = reserved_threads_;
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
                for (std::size_t i = 0; i < count && active_threads_ < max_threads_; ++i)
                {
                    if (threads_.size() <= active_threads_)
                    {
                        launch_thread_();
                    }
                    for (auto &ctx : threads_)
                    {
                        if (!ctx.active.load(std::memory_order_relaxed))
                        {
                            ctx.active.store(true, std::memory_order_relaxed);
                            ++active_threads_;
                            // No cv_.notify_one() needed; buffer notification handles waking
                            break;
                        }
                    }
                }
            }

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

            void worker_loop_(ThreadContext &ctx)
            {
                while (!ctx.exit_requested.load(std::memory_order_relaxed))
                {
                    if (!ctx.active.load(std::memory_order_relaxed))
                    {
                        std::this_thread::yield(); // Inactive threads yield
                        continue;
                    }

                    // Use lock-free notification to wait for items
                    buffer_.wait_for_item(spin_count_);

                    // Check for shutdown or exit after waiting
                    if (ctx.exit_requested.load(std::memory_order_relaxed) || buffer_.is_shutdown())
                    {
                        break;
                    }

                    BUFFER_ENTRY_T item{};
                    if (buffer_.pop(item))
                    {
                        try
                        {
                            task_(item);
                        }
                        catch (const std::exception &e)
                        {
                            // Log or handle exception
                        }
                    }
                }
            }

            void monitor_loop_()
            {
                while (running_)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(monitor_interval_ms_));
                    if (!running_)
                        break;

                    double ratio = buffer_.throughput_ratio();
                    std::unique_lock<std::mutex> lock(mutex_);

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
                                    threads_to_add = std::max<std::size_t>(1, static_cast<std::size_t>(deviation / batch_scaling_factor_));
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
                                    threads_to_remove = std::max<std::size_t>(1, static_cast<std::size_t>(deviation / batch_scaling_factor_));
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
            BUFFER_T &buffer_;
            std::function<void(BUFFER_ENTRY_T &)> task_;
            bool use_task_;

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
            std::size_t active_threads_;
            std::size_t spawn_counter_;
            std::size_t shrink_counter_;

            std::vector<ThreadContext> threads_;
            std::thread monitor_thread_;
            mutable std::mutex mutex_;
            std::condition_variable cv_; // Retained but unused in this version
        };
    } // namespace concurrency
} // namespace stdx

#endif /* B1D9354E_67C1_4440_BFF9_EE30213D0BB8 */