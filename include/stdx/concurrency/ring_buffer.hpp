#pragma once

#include <atomic>
#include <cstddef>
#include <cassert>
#include <memory>
#include <utility>
#include <vector>
#include <algorithm>
#include <type_traits>
#include "stdx/light_callable.hpp"

namespace stdx
{
    namespace concurrency
    {
        template <typename T>
        class RingBuffer
        {
            using slot_type = typename std::aligned_storage<sizeof(T), alignof(T)>::type;

        public:
            explicit RingBuffer(std::size_t capacity = 1024)
                : capacity_(next_power_of_two(capacity))
                , storage_(new slot_type[capacity_])
                , head_(0)
                , tail_(0)
                , item_count_(0)
                , push_count_(0)
                , pop_count_(0)
                , last_push_(0)
                , last_pop_(0)
                , on_nonempty_enabled_(false)
                , alarm_threshold_(0)
                , alarm_enabled_(false)
            {
            }

            RingBuffer(const RingBuffer&) = delete;
            RingBuffer& operator=(const RingBuffer&) = delete;
            RingBuffer(RingBuffer&&) = delete;
            RingBuffer& operator=(RingBuffer&&) = delete;

            ~RingBuffer()
            {
                // If you want to ensure leftover items are destructed,
                // you could call clear() here. But be aware of concurrency:
                // if other threads might still push/pop after ~RingBuffer(),
                // you have a bigger design problem. Typically you stop all threads first.
                clear();
            }

            /// Push a single item (MPMC lock-free).
            template <typename U>
            bool push(U&& item)
            {
                static_assert(std::is_same_v<std::decay_t<U>, T> || 
                              std::is_convertible_v<U, T>,
                              "Pushed item must be convertible to T");

                size_t current_head = head_.load(std::memory_order_relaxed);
                for (;;)
                {
                    size_t current_tail = tail_.load(std::memory_order_acquire);

                    // Buffer full?
                    if (current_head - current_tail >= capacity_)
                    {
                        return false;
                    }

                    // Try to reserve one slot by incrementing head.
                    if (head_.compare_exchange_weak(
                            current_head, current_head + 1,
                            std::memory_order_release,
                            std::memory_order_relaxed))
                    {
                        // We now own the slot at 'current_head'.
                        // Construct the T object in-place via placement-new:
                        void* slotPtr = static_cast<void*>(&storage_[current_head & (capacity_ - 1)]);
                        ::new (slotPtr) T(std::forward<U>(item));

                        push_count_.fetch_add(1, std::memory_order_relaxed);

                        // If oldSize was 0, we transitioned from empty -> non-empty
                        size_t oldSize = item_count_.fetch_add(1, std::memory_order_release);
                        if (oldSize == 0 && on_nonempty_enabled_.load(std::memory_order_relaxed))
                        {
                            if (on_nonempty_callback_)
                            {
                                on_nonempty_callback_();
                            }
                        }

                        if (alarm_enabled_.load(std::memory_order_relaxed))
                        {
                            size_t newSize = oldSize + 1;
                            if (newSize >= alarm_threshold_ && on_alarm_callback_)
                            {
                                on_alarm_callback_(newSize);
                            }
                        }

                        return true;
                    }
                    // else CAS failed => loop again
                }
            }

            /// Pop a single item (MPMC lock-free).
            bool pop(T& out)
            {
                size_t current_tail = tail_.load(std::memory_order_relaxed);
                for (;;)
                {
                    size_t current_head = head_.load(std::memory_order_acquire);

                    if (current_tail == current_head)
                    {
                        // Empty
                        return false;
                    }

                    if (tail_.compare_exchange_weak(
                            current_tail, current_tail + 1,
                            std::memory_order_acquire,
                            std::memory_order_relaxed))
                    {
                        // We now own the slot at 'current_tail'.
                        T* slotPtr = reinterpret_cast<T*>(
                            &storage_[current_tail & (capacity_ - 1)]
                        );

                        // Move out:
                        out = std::move(*slotPtr);
                        // Destruct the old object:
                        slotPtr->~T();

                        pop_count_.fetch_add(1, std::memory_order_relaxed);
                        item_count_.fetch_sub(1, std::memory_order_release);
                        return true;
                    }
                }
            }

            /// Pop up to `max_count` items, storing into caller's buffer `out`.
            std::size_t pop_batch(T* out, std::size_t max_count)
            {
                size_t popped = 0;
                while (popped < max_count)
                {
                    if (!pop(out[popped]))
                        break;
                    popped++;
                }
                return popped;
            }

            /// Pop up to `max_count` items, appending to `out_vec`.
            std::size_t pop_batch(std::vector<T>& out_vec, std::size_t max_count)
            {
                // Optional micro-optimization: reserve
                out_vec.reserve(out_vec.size() + max_count);

                std::size_t popped = 0;
                T temp;
                while (popped < max_count)
                {
                    if (!pop(temp))
                        break;
                    out_vec.push_back(std::move(temp));
                    popped++;
                }
                return popped;
            }

            bool empty() const
            {
                size_t t = tail_.load(std::memory_order_relaxed);
                size_t h = head_.load(std::memory_order_acquire);
                return (t == h);
            }

            bool full() const
            {
                size_t h = head_.load(std::memory_order_relaxed);
                size_t t = tail_.load(std::memory_order_acquire);
                return (h - t >= capacity_);
            }

            size_t size() const
            {
                // This could be slightly out of date in an MPMC scenario
                size_t h = head_.load(std::memory_order_acquire);
                size_t t = tail_.load(std::memory_order_relaxed);
                return h - t;
            }

            size_t capacity() const
            {
                return capacity_;
            }

            /// Clear all items by popping them. This calls destructors on all.
            void clear()
            {
                // We repeatedly pop until the buffer is empty.
                // This is safe if no other thread is pushing or popping concurrently.
                // If multiple threads are still active, you effectively remove items
                // "out from under them." Usually you'd only call clear() when no other
                // producers/consumers are running (e.g. on shutdown).
                T dummy;
                while (pop(dummy))
                {
                    // destructor was called in `pop()`
                }
            }

            /// A rough "throughput ratio" example: (pushes / pops) since last call.
            double throughput_ratio()
            {
                std::size_t cur_push = push_count_.load(std::memory_order_relaxed);
                std::size_t cur_pop  = pop_count_.load(std::memory_order_relaxed);

                std::size_t delta_push = cur_push - last_push_;
                std::size_t delta_pop  = cur_pop  - last_pop_;

                last_push_ = cur_push;
                last_pop_  = cur_pop;

                if (delta_push == 0 && delta_pop == 0)
                    return 1.0;
                if (delta_pop == 0)
                    return 9999.0;
                return static_cast<double>(delta_push) / static_cast<double>(delta_pop);
            }

            //--------------------------------------------------------
            // Optional Callbacks
            //--------------------------------------------------------
            void set_nonempty_callback(stdx::LightCallable<void()> cb)
            {
                on_nonempty_callback_ = cb;
                on_nonempty_enabled_.store(bool(on_nonempty_callback_), std::memory_order_release);
            }

            void set_alarm(std::size_t threshold, stdx::LightCallable<void(size_t)> alarm_cb)
            {
                alarm_threshold_ = threshold;
                on_alarm_callback_ = alarm_cb;
                alarm_enabled_.store(bool(on_alarm_callback_) && threshold > 0, std::memory_order_release);
            }

        private:
            static std::size_t next_power_of_two(std::size_t n)
            {
                if (n < 2) return 2;
                n--;
                n |= n >> 1;
                n |= n >> 2;
                n |= n >> 4;
                n |= n >> 8;
                n |= n >> 16;
                if constexpr (sizeof(std::size_t) == 8)
                {
                    n |= n >> 32;
                }
                return n + 1;
            }

            // Converts an index to a pointer into our allocated storage.
            // We mask with (capacity_ - 1) because capacity_ is a power of two.
            T* slot_ptr(size_t idx) const
            {
                return reinterpret_cast<T*>(&storage_[idx & (capacity_ - 1)]);
            }

        private:
            const std::size_t capacity_;

            // Our raw storage for T objects, uninitialized until push() constructs them.
            std::unique_ptr<slot_type[]> storage_;

            // Head/Tail pointers for a lock-free MPMC ring.
            // This is the same logic you had, just adapted to placement-new.
            alignas(64) std::atomic<size_t> head_;
            alignas(64) std::atomic<size_t> tail_;

            // Current item count (for optional checks/callbacks).
            std::atomic<size_t> item_count_;

            // For throughput analysis
            std::atomic<size_t> push_count_;
            std::atomic<size_t> pop_count_;
            size_t last_push_;
            size_t last_pop_;

            // Non-empty callback
            stdx::LightCallable<void()> on_nonempty_callback_;
            std::atomic<bool> on_nonempty_enabled_;

            // Alarm threshold & callback
            std::atomic<std::size_t> alarm_threshold_;
            stdx::LightCallable<void(size_t)> on_alarm_callback_;
            std::atomic<bool> alarm_enabled_;
        };

    } // namespace concurrency
} // namespace stdx
