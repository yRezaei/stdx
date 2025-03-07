#pragma once

#include <atomic>
#include <array>
#include <cstddef>
#include <utility>
#include <cassert>
#include <thread>
#include <vector>
#include <chrono>
#include <algorithm>
#include <type_traits>

/**
 * @file ring_buffer.hpp
 * @brief A lock-free ring buffer designed for multiple producers/consumers with optional batch-pop support.
 *
 * CAPACITY must be a power of two to allow efficient masking in place of modulo operations.
 */

namespace stdx
{
    namespace concurrency
    {

        /**
         * @brief A lock-free ring buffer with fixed capacity.
         *
         * @tparam T The stored item type.
         * @tparam CAPACITY The maximum capacity, which must be a power of two.
         *
         * This ring buffer uses two atomic counters, `head_` and `tail_`, to manage
         * storage. It allows for multi-producer / multi-consumer usage, assuming
         * each producer calls `push()` and each consumer calls `pop()` or `pop_batch()`.
         */
        template <typename T, std::size_t CAPACITY>
        class RingBuffer
        {
            static_assert((CAPACITY & (CAPACITY - 1)) == 0,
                          "CAPACITY must be a power of two");
            static_assert(CAPACITY >= 2,
                          "CAPACITY must be at least 2");

        public:
            /// Default constructor initializes all counters to zero and `shutdown_` to false.
            RingBuffer()
                : head_(0), tail_(0), item_count_(0), shutdown_(false), push_count_(0), pop_count_(0), last_push_(0), last_pop_(0)
            {
            }

            // Deleting copy/move semantics for simplicity (could be supported if needed).
            RingBuffer(const RingBuffer &) = delete;
            RingBuffer &operator=(const RingBuffer &) = delete;
            RingBuffer(RingBuffer &&) = delete;
            RingBuffer &operator=(RingBuffer &&) = delete;

            /**
             * @brief Pushes a single item into the ring buffer.
             *
             * @tparam U A type convertible to T (e.g. T or derived).
             * @param item The item to push (may be moved).
             * @return True if pushed successfully, false if the buffer is full.
             */
            template <typename U>
            bool push(U &&item)
            {
                static_assert(std::is_same_v<std::decay_t<U>, T> || std::is_convertible_v<U, T>,
                              "Pushed item must be convertible to T");

                // Load head pointer once in a loop. If CAS fails, 'current_head' is updated.
                size_t current_head = head_.load(std::memory_order_relaxed);
                for (;;)
                {
                    size_t current_tail = tail_.load(std::memory_order_acquire);

                    // If the buffer is full, return false.
                    if (current_head - current_tail >= CAPACITY)
                    {
                        return false;
                    }

                    // Attempt to reserve one slot by incrementing head_.
                    if (head_.compare_exchange_weak(
                            current_head, current_head + 1,
                            std::memory_order_release,
                            std::memory_order_relaxed))
                    {
                        // We successfully got a slot, so place the item.
                        buffer_[current_head & (CAPACITY - 1)] = std::forward<U>(item);

                        // Update counters
                        push_count_.fetch_add(1, std::memory_order_relaxed);
                        item_count_.fetch_add(1, std::memory_order_release); // Let consumers see it
                        return true;
                    }
                }
            }

            /**
             * @brief Pops a single item from the ring buffer.
             *
             * @param out Reference to store the popped item.
             * @return True if an item was popped, false if the buffer was empty.
             */
            bool pop(T &out)
            {
                size_t current_tail = tail_.load(std::memory_order_relaxed);
                for (;;)
                {
                    size_t current_head = head_.load(std::memory_order_acquire);

                    // If empty, return false immediately.
                    if (current_tail == current_head)
                    {
                        return false;
                    }

                    // Attempt to claim one item by incrementing tail_.
                    if (tail_.compare_exchange_weak(
                            current_tail, current_tail + 1,
                            std::memory_order_acquire,
                            std::memory_order_relaxed))
                    {
                        out = std::move(buffer_[current_tail & (CAPACITY - 1)]);

                        pop_count_.fetch_add(1, std::memory_order_relaxed);
                        item_count_.fetch_sub(1, std::memory_order_release);
                        return true;
                    }
                }
            }

            /**
             * @brief Pops up to `max_count` items in one operation, storing them in a caller-provided buffer.
             *
             * @param out Pointer to a buffer that can hold at least `max_count` items.
             * @param max_count Maximum number of items to pop in this call.
             * @return The number of items actually popped (<= max_count).
             *
             * This uses a single atomic compare-exchange to claim multiple items,
             * reducing overhead when popping items in batches.
             */
            std::size_t pop_batch(T *out, std::size_t max_count)
            {
                for (;;)
                {
                    // Snapshot the tail pointer
                    size_t current_tail = tail_.load(std::memory_order_relaxed);

                    // Snapshot the head pointer with acquire order to see up-to-date items
                    size_t current_head = head_.load(std::memory_order_acquire);
                    size_t available = current_head - current_tail;

                    if (available == 0)
                    {
                        // Buffer is empty
                        return 0;
                    }

                    // We can pop at most 'available' items, limited by 'max_count'
                    std::size_t to_pop = (available > max_count) ? max_count : available;

                    // Attempt one CAS to reserve 'to_pop' items
                    if (tail_.compare_exchange_weak(
                            current_tail, current_tail + to_pop,
                            std::memory_order_acquire,
                            std::memory_order_relaxed))
                    {
                        // Move items into the caller's buffer
                        for (std::size_t i = 0; i < to_pop; ++i)
                        {
                            out[i] = std::move(buffer_[(current_tail + i) & (CAPACITY - 1)]);
                        }
                        pop_count_.fetch_add(to_pop, std::memory_order_relaxed);
                        item_count_.fetch_sub(to_pop, std::memory_order_release);

                        return to_pop;
                    }
                    // If CAS fails, another consumer took items; retry
                }
            }

            /**
             * @brief Pops up to `max_count` items in one operation, appending them to `out_vec`.
             *
             * @param out_vec A vector to which popped items will be appended.
             * @param max_count Maximum number of items to pop in this call.
             * @return Number of items actually popped (<= max_count).
             */
            std::size_t pop_batch(std::vector<T> &out_vec, std::size_t max_count)
            {
                // Reserve space to reduce potential re-allocations
                out_vec.reserve(out_vec.size() + max_count);

                // For demonstration, using a small local buffer. Adjust if max_count can be large.
                T temp[1024];
                std::size_t local_max = std::min<std::size_t>(max_count, 1024);

                std::size_t popped = pop_batch(temp, local_max);
                if (popped > 0)
                {
                    // Move from the temp array into out_vec
                    out_vec.insert(out_vec.end(),
                                   std::make_move_iterator(temp),
                                   std::make_move_iterator(temp + popped));
                }
                return popped;
            }

            /**
             * @brief Lock-free wait for an item to appear or a shutdown signal.
             *
             * @param spin_count Number of yields before sleeping briefly.
             *
             * This method repeatedly checks `item_count_` to see if there are items,
             * and yields the CPU up to `spin_count` times before sleeping for a short duration.
             * It exits if the ring buffer is signaled to shut down.
             */
            void wait_for_item(std::size_t spin_count = 100) const
            {
                while (item_count_.load(std::memory_order_acquire) == 0 &&
                       !shutdown_.load(std::memory_order_acquire))
                {
                    // Perform several yields
                    for (std::size_t i = 0;
                         i < spin_count && item_count_.load(std::memory_order_relaxed) == 0;
                         ++i)
                    {
                        std::this_thread::yield();
                    }

                    // If still empty, do a short sleep
                    if (item_count_.load(std::memory_order_acquire) == 0)
                    {
                        std::this_thread::sleep_for(std::chrono::microseconds(10));
                    }
                }
            }

            /**
             * @brief Signals a shutdown to wake consumers so they can exit.
             */
            void signal_shutdown()
            {
                shutdown_.store(true, std::memory_order_release);
            }

            /**
             * @brief Checks if the buffer is in shutdown state.
             * @return True if shutdown has been signaled.
             */
            bool is_shutdown() const
            {
                return shutdown_.load(std::memory_order_acquire);
            }

            /**
             * @brief Checks if the ring buffer is empty.
             * @return True if empty, false otherwise.
             */
            bool empty() const
            {
                size_t tail = tail_.load(std::memory_order_relaxed);
                size_t head = head_.load(std::memory_order_acquire);
                return (tail == head);
            }

            /**
             * @brief Checks if the ring buffer is full.
             * @return True if full, false otherwise.
             */
            bool full() const
            {
                size_t head = head_.load(std::memory_order_relaxed);
                size_t tail = tail_.load(std::memory_order_acquire);
                return (head - tail >= CAPACITY);
            }

            /**
             * @brief Returns the current number of items in the buffer.
             */
            size_t size() const
            {
                size_t head = head_.load(std::memory_order_acquire);
                size_t tail = tail_.load(std::memory_order_relaxed);
                return head - tail;
            }

            /**
             * @brief Returns the fixed capacity of the buffer (i.e., CAPACITY).
             */
            size_t capacity() const
            {
                return buffer_.size();
            }

            /**
             * @brief Computes a "throughput ratio": (pushes / pops) since last call.
             *
             * @return A ratio used to determine load:
             *  - If `delta_pop == 0` and `delta_push == 0`, returns 1.0.
             *  - If `delta_pop == 0` but `delta_push > 0`, returns a large number (9999.0).
             *  - Otherwise, returns `(delta_push / delta_pop)`.
             *
             * The ThreadPool monitor thread reads this value to decide if more threads
             * should be activated or deactivated.
             */
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
            std::array<T, CAPACITY> buffer_; ///< Underlying storage array

            alignas(64) std::atomic<size_t> head_;       ///< Next write location
            char pad_[64 - sizeof(std::atomic<size_t>)]; // optional padding to avoid false sharing

            alignas(64) std::atomic<size_t> tail_;       ///< Next read location
            alignas(64) std::atomic<size_t> item_count_; ///< Number of items in the buffer
            std::atomic<bool> shutdown_;                 ///< Shutdown flag

            // Counters for throughput analysis
            std::atomic<std::size_t> push_count_;
            std::atomic<std::size_t> pop_count_;
            std::size_t last_push_;
            std::size_t last_pop_;
        };

    } // namespace concurrency
} // namespace stdx