#ifndef MPMC_RING_BUFFER_HPP
#define MPMC_RING_BUFFER_HPP

#include <atomic>
#include <array>
#include <cstddef>
#include <utility>
#include <cassert>

namespace stdx
{
    //
    // A minimal lock-free ring buffer supporting multiple producers and multiple consumers.
    // CAPACITY must be a power of two (e.g., 1024, 4096, etc.) for fast index masking.
    //
    template <typename T, std::size_t CAPACITY>
    class MpmcRingBuffer
    {
        static_assert((CAPACITY & (CAPACITY - 1)) == 0,
                      "CAPACITY must be a power of two");
        static_assert(CAPACITY >= 2,
                      "CAPACITY must be at least 2");

    public:
        MpmcRingBuffer()
            : head_(0),
              tail_(0)
        {
        }

        // Delete copy constructor and copy assignment operator
        MpmcRingBuffer(const MpmcRingBuffer&) = delete;
        MpmcRingBuffer& operator=(const MpmcRingBuffer&) = delete;

        // Delete move constructor and move assignment operator
        MpmcRingBuffer(MpmcRingBuffer&&) = delete;
        MpmcRingBuffer& operator=(MpmcRingBuffer&&) = delete;

        //--------------------------------------------------------------------------
        // push(...) : Attempt to enqueue an item (multiple producers can call this).
        // Returns true if successful, or false if the buffer is full.
        //--------------------------------------------------------------------------
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
                    return false; // full
                }
                if (head_.compare_exchange_weak(current_head, current_head + 1,
                                                std::memory_order_release,
                                                std::memory_order_relaxed))
                {
                    // Perfect-forward into the buffer
                    buffer_[current_head & (CAPACITY - 1)] = std::forward<U>(item);
                    return true;
                }
            }
        }

        //--------------------------------------------------------------------------
        // pop(...) : Attempt to dequeue an item (multiple consumers can call this).
        // Returns true if an item was popped, or false if the buffer is empty.
        //--------------------------------------------------------------------------
        bool pop(T &out)
        {
            // 'tail_' is the next item index for consumers to read from.
            size_t current_tail = tail_.load(std::memory_order_relaxed);

            for (;;)
            {
                // Acquire ensures we see the most up-to-date head_
                size_t current_head = head_.load(std::memory_order_acquire);

                // If empty
                if (current_tail == current_head)
                {
                    return false;
                }

                // Attempt to claim the item at current_tail
                if (tail_.compare_exchange_weak(
                        current_tail,
                        current_tail + 1,
                        std::memory_order_acquire,
                        std::memory_order_relaxed))
                {
                    // We have claimed slot [current_tail & (CAPACITY - 1)]
                    out = std::move(buffer_[current_tail & (CAPACITY - 1)]);
                    return true;
                }
                // If the compare_exchange_weak fails, current_tail is updated;
                // loop again.
            }
        }

        //--------------------------------------------------------------------------
        // is_empty() : for convenience, checks if the buffer is currently empty.
        //--------------------------------------------------------------------------
        bool is_empty() const
        {
            // Acquire the heads/tails in a consistent manner
            size_t tail = tail_.load(std::memory_order_relaxed);
            size_t head = head_.load(std::memory_order_acquire);
            return (tail == head);
        }

        //--------------------------------------------------------------------------
        // is_full() : for convenience, checks if the buffer is currently full.
        //--------------------------------------------------------------------------
        bool is_full() const
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

    private:
        std::array<T, CAPACITY> buffer_;

        // 'head_' is how far producers have written (or reserved).
        // 'tail_' is how far consumers have read.
        alignas(64) std::atomic<size_t> head_;
        char pad_[64 - sizeof(std::atomic<size_t>)];

        alignas(64) std::atomic<size_t> tail_;
    };
}

#endif // MPMC_RING_BUFFER_HPP
