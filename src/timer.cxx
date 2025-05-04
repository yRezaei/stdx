#include "stdx/threading/timer.hpp"
#include <map>
#include <queue>
#include <mutex>
#include <iostream>
#include <exception>

namespace stdx
{
    namespace threading
    {
        // Timer implementation
        Timer::Timer(ThreadPool &thread_pool)
            : thread_pool_(thread_pool),
              impl_(_impl::TimerImpl::create())
        {
        }

        Timer::~Timer()
        {
            stop();
        }

        void Timer::start()
        {
            if (!valid_task_ || handle_ != 0)
                return;

            try
            {
                handle_ = impl_.schedule(
                    delay_,
                    interval_,
                    priority_,
                    thread_pool_,
                    task_);

                valid_task_ = false;
            }
            catch (const std::exception &ex)
            {
                std::cerr << "Timer start error: " << ex.what() << std::endl;
            }
        }

        void Timer::stop()
        {
            if (handle_ != 0)
            {
                impl_.cancel(handle_);
                handle_ = 0;
            }
        }

        namespace _impl
        {

            TimerImpl::TimerImpl()
            {
                worker_thread_ = std::thread(&TimerImpl::worker_loop, this);
            }

            TimerImpl::~TimerImpl()
            {
                running_ = false;
                cv_.notify_one();
                if (worker_thread_.joinable())
                {
                    worker_thread_.join();
                }
            }

            TimerImpl::TimerHandle TimerImpl::schedule(
                std::chrono::milliseconds delay,
                std::chrono::milliseconds interval,
                std::size_t priority,
                ThreadPool &thread_pool,
                const stdx::LightCallable<void()> &task)
            {
                auto t = std::make_shared<TimerTask>();
                t->handle = next_handle_.fetch_add(1, std::memory_order_relaxed);
                t->next_execution = Clock::now() + delay;
                t->interval = interval;
                t->task = task;
                t->priority = priority;
                t->thread_pool = &thread_pool;
                t->cancelled = false;

                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    task_map_[t->handle] = t;
                    task_queue_.push(t);
                }
                cv_.notify_one();

                return t->handle;
            }

            bool TimerImpl::cancel(TimerHandle handle)
            {
                std::unique_lock<std::mutex> lock(mutex_);
                auto it = task_map_.find(handle);
                if (it != task_map_.end())
                {
                    it->second->cancelled = true;
                    task_map_.erase(it);
                    return true;
                }
                return false;
            }

            void TimerImpl::worker_loop()
            {
                while (true)
                {
                    std::unique_lock<std::mutex> lock(mutex_);

                    if (!running_ && task_queue_.empty())
                        break;

                    if (task_queue_.empty())
                    {
                        cv_.wait(lock, [this]
                                 { return !running_ || !task_queue_.empty(); });
                        if (!running_ && task_queue_.empty())
                            break;
                    }
                    else
                    {
                        auto top = task_queue_.top();
                        auto now = Clock::now();

                        if (top->next_execution <= now)
                        {
                            task_queue_.pop();
                            auto handle = top->handle;
                            bool cancelledOrMissing = top->cancelled ||
                                                      (task_map_.find(handle) == task_map_.end());

                            if (!cancelledOrMissing && top->task)
                            {
                                auto spTask = top;
                                bool isRecurring = spTask->recurring();

                                lock.unlock();

                                try
                                {
                                    if (spTask->thread_pool)
                                    {
                                        auto task_copy = spTask->task;
                                        spTask->thread_pool->submit(spTask->priority, std::move(task_copy));
                                    }
                                }
                                catch (const std::exception &e)
                                {
                                    std::cerr << "Error executing timer task: " << e.what() << '\n';
                                }

                                lock.lock();

                                if (isRecurring)
                                {
                                    spTask->next_execution = now + spTask->interval;
                                    task_queue_.push(spTask);
                                }
                                else
                                {
                                    task_map_.erase(handle);
                                }
                            }
                            else
                            {
                                task_map_.erase(handle);
                            }
                        }
                        else
                        {
                            auto wait_dur = top->next_execution - now;
                            cv_.wait_for(lock, wait_dur, [this]
                                         { return !running_; });
                        }
                    }
                }
            }
        } // namespace _impl

    } // namespace threading
} // namespace stdx