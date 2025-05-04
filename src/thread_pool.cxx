#include "stdx/threading/thread_pool.hpp"
#include <cassert>
#include <iostream>
#include <utility>
#include <type_traits>
#include <chrono>
#include <numeric>

namespace stdx
{
    namespace threading
    {
        const char* ThreadPoolStopped::what() const noexcept
        {
            return "ThreadPool is not running, submission rejected.";
        }

        const char* BufferFull::what() const noexcept
        {
            return "The ring buffer is full, submission failed.";
        }

        const char* InvalidPriorityIndex::what() const noexcept
        {
            return "Priority index is out of range for this ThreadPool.";
        }

        // ThreadPoolStats implementation
        ThreadPoolStats::ThreadPoolStats(std::uint32_t num_threads, std::uint32_t nr_of_priorities)
        {
            // Initialize priority-based vectors
            tasks_by_priority.reserve(nr_of_priorities);
            buffer_near_full_events.reserve(nr_of_priorities);

            for (std::size_t i = 0; i < nr_of_priorities; ++i)
            {
                tasks_by_priority.emplace_back(std::make_unique<std::atomic<size_t>>(0));
                buffer_near_full_events.emplace_back(std::make_unique<std::atomic<size_t>>(0));
            }

            // Initialize thread-based vectors
            tasks_per_thread.resize(num_threads, 0);
            thread_wakeup_count.resize(num_threads, 0);
        }

        void ThreadPoolStats::reset()
        {
            tasks_submitted.store(0, std::memory_order_relaxed);
            tasks_completed.store(0, std::memory_order_relaxed);
            submission_failures.store(0, std::memory_order_relaxed);
            wait_count.store(0, std::memory_order_relaxed);
            task_execution_total_us.store(0, std::memory_order_relaxed);
            exception_count.store(0, std::memory_order_relaxed);
            thread_sleep_count.store(0, std::memory_order_relaxed);

            for (auto& counter : tasks_by_priority)
            {
                counter->store(0, std::memory_order_relaxed);
            }
            for (auto& counter : buffer_near_full_events)
            {
                counter->store(0, std::memory_order_relaxed);
            }
            
            std::fill(tasks_per_thread.begin(), tasks_per_thread.end(), 0);
            std::fill(thread_wakeup_count.begin(), thread_wakeup_count.end(), 0);
        }

        // ThreadPool implementation
        ThreadPool::ThreadPool(std::uint32_t num_threads, std::uint32_t buffer_capacity, std::uint32_t nr_of_priorities)
            : num_threads_(std::max(1u, num_threads)), 
            buffer_capacity_(std::max(16u, buffer_capacity)), 
            nr_of_priorities_(std::max(1u, nr_of_priorities)),
            stats_(num_threads, nr_of_priorities)
        {
            // Build ring_buffers_:
            ring_buffers_.reserve(nr_of_priorities_);

            for (std::size_t p = 0; p < nr_of_priorities_; ++p)
            {
                ring_buffers_.emplace_back(std::make_unique<stdx::concurrency::RingBuffer<stdx::LightCallable<void()>>>(buffer_capacity_));
                ring_buffers_[p]->set_nonempty_callback([this]()
                                                        { notify_one_thread(); });
                ring_buffers_[p]->set_alarm((buffer_capacity_ * 9) / 10, [this, p](std::size_t)
                                            { stats_.buffer_near_full_events[p]->fetch_add(1, std::memory_order_relaxed); });
            }
        }

        ThreadPool::~ThreadPool()
        {
            stop();
        }

        std::size_t ThreadPool::get_thread_count() const 
        { 
            return threads_.size(); 
        }

        void ThreadPool::start()
        {
            PoolState expected = PoolState::idle;
            if (pool_state_.compare_exchange_strong(expected, PoolState::Running))
            {
                for (uint32_t i = 0u; i < num_threads_; ++i)
                {
                    threads_.emplace_back(&ThreadPool::worker_loop, this, i);
                }
            }
        }

        void ThreadPool::stop(ShutdownMode mode)
        {
            PoolState expected = PoolState::Running;
            if (pool_state_.compare_exchange_strong(expected, PoolState::Stopping))
            {
                if (mode == ShutdownMode::Immediate)
                {
                    // Clear all ring buffers
                    for (auto &rb : ring_buffers_)
                    {
                        rb->clear();
                    }
                }

                {
                    std::unique_lock<std::mutex> lock(wakeup_mutex_);
                    wakeup_cv_.notify_all();
                }

                for (auto &th : threads_)
                {
                    if (th.joinable())
                        th.join();
                }
                threads_.clear();

                pool_state_.store(PoolState::idle, std::memory_order_release);
            }
        }

        void ThreadPool::record_exception()
        {
            if (exception_handler_)
            {
                exception_handler_(std::current_exception());
            }
            else
            {
                stats_.exception_count.fetch_add(1, std::memory_order_relaxed);
            }
        }

        ThreadPoolSnapshot ThreadPool::get_stats() const
        {
            ThreadPoolSnapshot snap;
            snap.tasks_submitted = stats_.tasks_submitted.load(std::memory_order_relaxed);
            snap.tasks_completed = stats_.tasks_completed.load(std::memory_order_relaxed);
            snap.submission_failures = stats_.submission_failures.load(std::memory_order_relaxed);
            snap.wait_count = stats_.wait_count.load(std::memory_order_relaxed);
            snap.total_execution_time_us = stats_.task_execution_total_us.load(std::memory_order_relaxed);
            snap.exception_count = stats_.exception_count.load(std::memory_order_relaxed);
            snap.thread_sleep_count = stats_.thread_sleep_count.load(std::memory_order_relaxed);

            snap.avg_execution_time_us = (snap.tasks_completed > 0)
                                             ? static_cast<double>(snap.total_execution_time_us) / snap.tasks_completed
                                             : 0.0;

            // tasks_by_priority
            snap.tasks_by_priority.resize(nr_of_priorities_);
            snap.buffer_near_full_events.resize(nr_of_priorities_);
            snap.buffer_usage_pct.resize(nr_of_priorities_);

            for (std::size_t p = 0; p < nr_of_priorities_; ++p)
            {
                snap.tasks_by_priority[p] = stats_.tasks_by_priority[p]->load(std::memory_order_relaxed);
                snap.buffer_near_full_events[p] = stats_.buffer_near_full_events[p]->load(std::memory_order_relaxed);

                auto capacity = ring_buffers_[p]->capacity();
                snap.buffer_usage_pct[p] = (capacity > 0)
                                               ? (static_cast<double>(ring_buffers_[p]->size()) / capacity * 100.0)
                                               : 0.0;
            }

            // tasks_per_thread
            auto thread_count = stats_.tasks_per_thread.size();
            snap.tasks_per_thread.resize(thread_count);
            snap.thread_wakeup_count.resize(thread_count);
            for (std::size_t i = 0; i < thread_count; ++i)
            {
                snap.tasks_per_thread[i] = stats_.tasks_per_thread[i];
                snap.thread_wakeup_count[i] = stats_.thread_wakeup_count[i];
            }

            return snap;
        }

        void ThreadPool::reset_stats()
        {
            stats_.reset();
        }

        void ThreadPool::set_exception_handler(stdx::LightCallable<void(std::exception_ptr)> handler)
        {
            exception_handler_ = handler;
        }

        void ThreadPool::notify_one_thread()
        {
            // if there's at least one sleeping thread, wake one up
            if (sleeping_threads_.load(std::memory_order_acquire) > 0)
            {
                std::unique_lock<std::mutex> lock(wakeup_mutex_);
                wakeup_cv_.notify_one();
            }
        }

        bool ThreadPool::has_tasks() const
        {
            for (auto &rb : ring_buffers_)
            {
                if (rb->size() > 0)
                    return true;
            }
            return false;
        }

        void ThreadPool::worker_loop(std::uint32_t thread_id)
        {
            bool sleep_triggered = false;
            int sleep_attempt = 1;
            constexpr int base_interval = 5;
            constexpr int max_sleep = 3000; // 3 seconds

            while (true)
            {
                stdx::LightCallable<void()> task;
                bool found_task = false;

                // Attempt to pop from ring buffers in ascending priority index
                for (std::size_t p = 0; p < nr_of_priorities_; ++p)
                {
                    if (ring_buffers_[p]->pop(task))
                    {
                        found_task = true;
                        sleep_triggered = false;
                        sleep_attempt = 1;
                        break;
                    }
                }

                if (found_task)
                {
                    stats_.tasks_per_thread[thread_id]++;
                    auto start_time = std::chrono::high_resolution_clock::now();
                    try
                    {
                        task(); // run user code
                    }
                    catch (...)
                    {
                        record_exception();
                    }
                    auto end_time = std::chrono::high_resolution_clock::now();
                    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();

                    stats_.tasks_completed.fetch_add(1, std::memory_order_relaxed);
                    stats_.task_execution_total_us.fetch_add(duration_us, std::memory_order_relaxed);
                }
                else
                {
                    if (pool_state_.load(std::memory_order_acquire) == PoolState::Stopping)
                    {
                        // no new tasks, can exit
                        break;
                    }

                    if (!sleep_triggered)
                        sleep_triggered = true;

                    int sleep_amount = ((num_threads_ - sleeping_threads_.load(std::memory_order_relaxed) + 1) * sleep_attempt) * base_interval;

                    if (sleep_amount > max_sleep)
                    {
                        sleep_triggered = false;
                        sleep_amount = 1;
                        sleeping_threads_.fetch_add(1, std::memory_order_release);
                        stats_.thread_sleep_count.fetch_add(1, std::memory_order_release);

                        std::unique_lock<std::mutex> lock(wakeup_mutex_);
                        wakeup_cv_.wait(lock, [this]
                                        { return (pool_state_.load(std::memory_order_acquire) != PoolState::Running) || has_tasks(); });

                        sleeping_threads_.fetch_sub(1, std::memory_order_release);
                        stats_.thread_wakeup_count[thread_id]++;
                    }
                    else
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_amount));
                        sleep_attempt++;
                    }
                }
            }
        }

    } // namespace threading
} // namespace stdx