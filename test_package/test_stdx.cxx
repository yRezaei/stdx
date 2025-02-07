#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

#include "stdx/mpmc_ring_buffer.hpp"

namespace stdx {

static constexpr std::size_t BUFFER_SIZE = 1024; // must be power of two
using IntRingBuffer = MpmcRingBuffer<int, BUFFER_SIZE>;

//------------------------------------------------------------------------------
// Basic Single-Threaded Test
//------------------------------------------------------------------------------
TEST(MpmcRingBufferTest, SingleThreadedPushPop)
{
    IntRingBuffer buffer;
    EXPECT_TRUE(buffer.is_empty());
    EXPECT_FALSE(buffer.is_full());

    // Push an item
    EXPECT_TRUE(buffer.push(42));
    EXPECT_FALSE(buffer.is_empty());

    // Pop the item
    int value = 0;
    EXPECT_TRUE(buffer.pop(value));
    EXPECT_EQ(value, 42);
    EXPECT_TRUE(buffer.is_empty());
    EXPECT_FALSE(buffer.is_full());

    // Pop again should fail (empty)
    EXPECT_FALSE(buffer.pop(value));
}

//------------------------------------------------------------------------------
// Fill / Overfill Test
//------------------------------------------------------------------------------
TEST(MpmcRingBufferTest, FillAndOverfill)
{
    MpmcRingBuffer<int, 8> small_buffer; // capacity=8 (power of two)
    // Fill the buffer
    for (int i = 0; i < 8; ++i)
    {
        EXPECT_TRUE(small_buffer.push(i)) << "Push " << i << " failed unexpectedly.";
    }
    // Now it should be full
    EXPECT_TRUE(small_buffer.is_full());

    // Overfill attempt
    EXPECT_FALSE(small_buffer.push(99)) << "Expected push to fail when buffer is full.";

    // Pop 4 items
    for (int i = 0; i < 4; ++i)
    {
        int val = -1;
        EXPECT_TRUE(small_buffer.pop(val)) << "Pop " << i << " failed unexpectedly.";
        EXPECT_EQ(val, i) << "Popped unexpected value.";
    }
    EXPECT_FALSE(small_buffer.is_full());
    EXPECT_FALSE(small_buffer.is_empty());

    // Now push 4 more
    for (int i = 8; i < 12; ++i)
    {
        EXPECT_TRUE(small_buffer.push(i)) << "Push " << i << " failed unexpectedly.";
    }
    // Should be full again
    EXPECT_TRUE(small_buffer.is_full());
}

//------------------------------------------------------------------------------
// Single Producer, Single Consumer
//------------------------------------------------------------------------------
TEST(MpmcRingBufferTest, SingleProducerSingleConsumer)
{
    IntRingBuffer buffer;
    const int total_items = 1000;
    std::atomic<bool> producer_done{false};

    // Producer thread
    std::thread producer([&] {
        for (int i = 0; i < total_items; ++i)
        {
            // Wait until we can push
            while (!buffer.push(i))
            {
                // Busy-wait or sleep
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        }
        producer_done = true;
    });

    // Consumer thread
    int popped_count = 0;
    std::thread consumer([&] {
        int val;
        while (!producer_done.load() || !buffer.is_empty())
        {
            if (buffer.pop(val))
            {
                ++popped_count;
            }
            else
            {
                // Sleep to avoid excessive busy-wait
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        }
    });

    producer.join();
    consumer.join();

    // We expect all 1000 items to be popped
    EXPECT_EQ(popped_count, total_items);
}

//------------------------------------------------------------------------------
// Multiple Producers, Single Consumer
//------------------------------------------------------------------------------
TEST(MpmcRingBufferTest, MultiProducerSingleConsumer)
{
    IntRingBuffer buffer;
    const int total_items_per_thread = 500;
    const int num_producers = 4;
    std::atomic<int> global_count{0}; // total items produced
    std::atomic<bool> done_producers{false};

    // Launch producers
    std::vector<std::thread> producers;
    producers.reserve(num_producers);
    for (int t = 0; t < num_producers; ++t)
    {
        producers.emplace_back([&, t] {
            for (int i = 0; i < total_items_per_thread; ++i)
            {
                // Some data to push
                int data = t * 1000000 + i; // unique-ish
                while (!buffer.push(data))
                {
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                }
                global_count.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    // Single consumer
    std::thread consumer([&] {
        int popped = 0;
        int val = 0;
        // Keep popping until we've accounted for all produced items
        while (popped < (num_producers * total_items_per_thread))
        {
            if (buffer.pop(val))
            {
                popped++;
            }
            else
            {
                // Sleep a bit
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        }
    });

    // Join all
    for (auto &p : producers)
    {
        p.join();
    }
    done_producers = true;
    consumer.join();

    // total items produced should match total
    EXPECT_EQ(global_count.load(), num_producers * total_items_per_thread);
}

//------------------------------------------------------------------------------
// Multiple Producers, Multiple Consumers
//------------------------------------------------------------------------------
TEST(MpmcRingBufferTest, MultiProducerMultiConsumer)
{
    MpmcRingBuffer<int, 512> buffer;
    const int total_items_per_producer = 300;
    const int num_producers = 3;
    const int num_consumers = 2;
    const int total_items = num_producers * total_items_per_producer;

    std::atomic<int> produced_count{0};
    std::atomic<int> consumed_count{0};
    std::atomic<bool> done_producing{false};

    // Launch producers
    std::vector<std::thread> producers;
    producers.reserve(num_producers);
    for (int t = 0; t < num_producers; ++t)
    {
        producers.emplace_back([&, t] {
            for (int i = 0; i < total_items_per_producer; ++i)
            {
                int data = t * 1000000 + i;
                // Wait until we can push
                while (!buffer.push(data))
                {
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                }
                produced_count.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    // Launch consumers
    std::vector<std::thread> consumers;
    consumers.reserve(num_consumers);
    for (int c = 0; c < num_consumers; ++c)
    {
        consumers.emplace_back([&] {
            int val;
            for (;;)
            {
                if (buffer.pop(val))
                {
                    consumed_count.fetch_add(1, std::memory_order_relaxed);
                }
                else
                {
                    // Check if we are done
                    if (produced_count.load() >= total_items && buffer.is_empty())
                    {
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                }
            }
        });
    }

    // Join producers
    for (auto &p : producers)
    {
        p.join();
    }
    done_producing = true;

    // Join consumers
    for (auto &c : consumers)
    {
        c.join();
    }

    // Check final counts
    EXPECT_EQ(produced_count.load(), total_items);
    EXPECT_EQ(consumed_count.load(), total_items);
}

} // namespace stdx

//------------------------------------------------------------------------------
// main: typical Google Test entry
//------------------------------------------------------------------------------
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}