#ifndef AE72ABF9_E120_4308_820D_72E5A0E7595E
#define AE72ABF9_E120_4308_820D_72E5A0E7595E

#include <atomic>
#include <array>
#include <cstddef>
#include <utility>
#include <cassert>
#include <thread>

namespace stdx
{
    namespace concurrency
    {
        template <typename T, std::size_t CAPACITY>
        class RingBuffer
        {
            static_assert((CAPACITY & (CAPACITY - 1)) == 0,
                          "CAPACITY must be a power of two");
            static_assert(CAPACITY >= 2,
                          "CAPACITY must be at least 2");

        public:
            RingBuffer()
                : head_(0),
                  tail_(0),
                  item_count_(0),
                  shutdown_(false),
                  push_count_(0),
                  pop_count_(0)
            {
            }

            RingBuffer(const RingBuffer &) = delete;
            RingBuffer &operator=(const RingBuffer &) = delete;

            RingBuffer(RingBuffer &&) = delete;
            RingBuffer &operator=(RingBuffer &&) = delete;

            template <typename U>
            bool push(U &&item)
            {
                static_assert(std::is_same_v<std::decay_t<U>, T> || std::is_convertible_v<U, T>,
                              "Pushed item must be convertible to T");

                size_t current_head = head_.load(std::memory_order_relaxed);
                for (;;)
                {
                    size_t current_tail = tail_.load(std::memory_order_acquire);
                    if (current_head - current_tail >= CAPACITY)
                    {
                        return false; // Buffer is full
                    }
                    if (head_.compare_exchange_weak(current_head, current_head + 1,
                                                    std::memory_order_release,
                                                    std::memory_order_relaxed))
                    {
                        buffer_[current_head & (CAPACITY - 1)] = std::forward<U>(item);
                        push_count_.fetch_add(1, std::memory_order_relaxed);
                        item_count_.fetch_add(1, std::memory_order_release); // Notify consumers
                        return true;
                    }
                }
            }

            bool pop(T &out)
            {
                size_t current_tail = tail_.load(std::memory_order_relaxed);
                for (;;)
                {
                    size_t current_head = head_.load(std::memory_order_acquire);
                    if (current_tail == current_head)
                    {
                        return false; // Buffer is empty
                    }
                    if (tail_.compare_exchange_weak(current_tail, current_tail + 1,
                                                    std::memory_order_acquire,
                                                    std::memory_order_relaxed))
                    {
                        out = std::move(buffer_[current_tail & (CAPACITY - 1)]);
                        pop_count_.fetch_add(1, std::memory_order_relaxed);
                        item_count_.fetch_sub(1, std::memory_order_release); // Update item count
                        return true;
                    }
                }
            }

            // Lock-free wait for an item or shutdown
            void wait_for_item(std::size_t spin_count = 100) const
            {
                while (item_count_.load(std::memory_order_acquire) == 0 && !shutdown_.load(std::memory_order_acquire))
                {
                    for (std::size_t i = 0; i < spin_count && item_count_.load(std::memory_order_relaxed) == 0; ++i)
                    {
                        std::this_thread::yield();
                    }
                    if (item_count_.load(std::memory_order_acquire) == 0)
                    {
                        std::this_thread::sleep_for(std::chrono::microseconds(10)); // Lightweight sleep
                    }
                }
            }

            // Signal shutdown to wake up consumers
            void signal_shutdown()
            {
                shutdown_.store(true, std::memory_order_release);
            }

            bool is_shutdown() const
            {
                return shutdown_.load(std::memory_order_acquire);
            }

            bool empty() const
            {
                size_t tail = tail_.load(std::memory_order_relaxed);
                size_t head = head_.load(std::memory_order_acquire);
                return (tail == head);
            }

            bool full() const
            {
                size_t head = head_.load(std::memory_order_relaxed);
                size_t tail = tail_.load(std::memory_order_acquire);
                return (head - tail >= CAPACITY);
            }

            size_t size() const
            {
                size_t head = head_.load(std::memory_order_acquire);
                size_t tail = tail_.load(std::memory_order_relaxed);
                return head - tail;
            }

            size_t capacity() const
            {
                return buffer_.size();
            }

            double throughput_ratio()
            {
                std::size_t cur_push = push_count_.load(std::memory_order_relaxed);
                std::size_t cur_pop = pop_count_.load(std::memory_order_relaxed);
                std::size_t delta_push = cur_push - last_push_;
                std::size_t delta_pop = cur_pop - last_pop_;
                last_push_ = cur_push;
                last_pop_ = cur_pop;
                if (delta_pop == 0 && delta_push == 0)
                {
                    return 1.0;
                }
                if (delta_pop == 0)
                {
                    return 9999.0;
                }
                return static_cast<double>(delta_push) / static_cast<double>(delta_pop);
            }

        private:
            std::array<T, CAPACITY> buffer_;
            alignas(64) std::atomic<size_t> head_;
            char pad_[64 - sizeof(std::atomic<size_t>)];
            alignas(64) std::atomic<size_t> tail_;
            alignas(64) std::atomic<size_t> item_count_; // Lock-free notification counter
            std::atomic<bool> shutdown_;                 // Shutdown flag
            std::atomic<std::size_t> push_count_;
            std::atomic<std::size_t> pop_count_;
            std::size_t last_push_ = 0;
            std::size_t last_pop_ = 0;
        };

    } // namespace concurrency
} // namespace stdx

#endif /* AE72ABF9_E120_4308_820D_72E5A0E7595E */
