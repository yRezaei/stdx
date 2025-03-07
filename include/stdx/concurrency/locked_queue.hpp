#pragma once

#include <queue>
#include <mutex>

namespace stdx
{
    namespace concurrency
    {
        template <typename T>
        struct LockedQueue
        {
            bool pop(T &out)
            {
                std::unique_lock<std::mutex> lock(mutex_);
                if (queue_.empty())
                    return false;
                out = queue_.front();
                queue_.pop();
                ++pop_count_;
                return true;
            }

            bool empty() const
            {
                std::unique_lock<std::mutex> lock(mutex_);
                return queue_.empty();
            }

            std::size_t size() const
            {
                std::unique_lock<std::mutex> lock(mutex_);
                return queue_.size();
            }

            std::size_t capacity() const
            {
                return 0; // no fixed limit
            }

            template <typename U>
            void push(U &&value)
            {
                std::unique_lock<std::mutex> lock(mutex_);
                queue_.push(std::forward<U>(value));
                ++push_count_;
            }

            double throughput_ratio()
            {
                std::unique_lock<std::mutex> lock(mutex_);

                // If we want to do a "delta" approach, we see how many pushes/pops
                // occurred since last time we called throughput_ratio().

                std::size_t delta_push = push_count_ - last_push_;
                std::size_t delta_pop = pop_count_ - last_pop_;

                last_push_ = push_count_;
                last_pop_ = pop_count_;

                // Avoid divide by zero
                if (delta_pop == 0 && delta_push == 0)
                {
                    // No activity, ratio = 1.0 => stable, or 0 => idle, your choice
                    return 1.0;
                }
                if (delta_pop == 0)
                {
                    // All push, no pop => ratio is large
                    return 9999.0; // or some sentinel
                }

                // Example: ratio = (pushes) / (pops)
                return static_cast<double>(delta_push) / static_cast<double>(delta_pop);
            }

        private:
            std::queue<T> queue_;
            std::mutex &mutex_;
            std::size_t push_count_;
            std::size_t pop_count_;
            std::size_t last_push_ = 0;
            std::size_t last_pop_ = 0;
        };

    } // namespace concurrency
} // namespace stdx