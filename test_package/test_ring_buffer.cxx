#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <functional>

#include "stdx/concurrency/ring_buffer.hpp"

namespace stdx
{

    static constexpr std::size_t BUFFER_SIZE = 1024; // must be a power of two
    using IntRingBuffer = stdx::concurrency::RingBuffer<int>;

    /**
     * @brief Test fixture for RingBuffer tests.
     */
    class RingBufferTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            buffer_ = std::make_unique<IntRingBuffer>(BUFFER_SIZE);
        }

        void TearDown() override
        {
            buffer_.reset();
        }

        std::unique_ptr<IntRingBuffer> buffer_;
    };

    /**
     * @brief Verify that the capacity is rounded up to the nearest power of two.
     */
    TEST(RingBufferCapacityTest, RoundUpToPowerOfTwo)
    {
        // If we pass something like 100, it should become 128
        {
            stdx::concurrency::RingBuffer<int> rb(100);
            EXPECT_EQ(rb.capacity(), 128u)
                << "Expected capacity to round up to next power-of-two.";
        }
        // If we pass 1, it should become at least 2
        {
            stdx::concurrency::RingBuffer<int> rb(1);
            EXPECT_EQ(rb.capacity(), 2u)
                << "Expected capacity to be forced to minimum 2.";
        }
        // If we pass an already-power-of-two, it remains the same
        {
            stdx::concurrency::RingBuffer<int> rb(1024);
            EXPECT_EQ(rb.capacity(), 1024u)
                << "Expected capacity to stay at 1024 for an already-power-of-two.";
        }
    }

    //------------------------------------------------------------------------------
    // 1) Basic Single-Threaded Test
    //------------------------------------------------------------------------------
    TEST_F(RingBufferTest, SingleThreadedPushPop)
    {
        EXPECT_TRUE(buffer_->empty());
        EXPECT_FALSE(buffer_->full());

        // Push an item
        EXPECT_TRUE(buffer_->push(42));
        EXPECT_FALSE(buffer_->empty());

        // Pop the item
        int value = 0;
        EXPECT_TRUE(buffer_->pop(value));
        EXPECT_EQ(value, 42);
        EXPECT_TRUE(buffer_->empty());
        EXPECT_FALSE(buffer_->full());

        // Pop again should fail (empty)
        EXPECT_FALSE(buffer_->pop(value));
    }

    //------------------------------------------------------------------------------
    // 2) Fill / Overfill Test
    //------------------------------------------------------------------------------
    TEST_F(RingBufferTest, FillAndOverfill)
    {
        // Use a small capacity to test full/overfull behavior
        stdx::concurrency::RingBuffer<int> small_buffer(8);

        // Fill the buffer
        for (int i = 0; i < 8; ++i)
        {
            EXPECT_TRUE(small_buffer.push(i)) << "Push " << i << " failed unexpectedly.";
        }
        // Now it should be full
        EXPECT_TRUE(small_buffer.full());

        // Overfill attempt
        EXPECT_FALSE(small_buffer.push(99)) << "Expected push to fail when buffer is full.";

        // Pop 4 items
        for (int i = 0; i < 4; ++i)
        {
            int val = -1;
            EXPECT_TRUE(small_buffer.pop(val)) << "Pop " << i << " failed unexpectedly.";
            EXPECT_EQ(val, i) << "Popped unexpected value.";
        }
        EXPECT_FALSE(small_buffer.full());
        EXPECT_FALSE(small_buffer.empty());

        // Now push 4 more
        for (int i = 8; i < 12; ++i)
        {
            EXPECT_TRUE(small_buffer.push(i)) << "Push " << i << " failed unexpectedly.";
        }
        // Should be full again
        EXPECT_TRUE(small_buffer.full());
    }

    //------------------------------------------------------------------------------
    // 3) Single Producer, Single Consumer
    //------------------------------------------------------------------------------
    TEST_F(RingBufferTest, SingleProducerSingleConsumer)
    {
        const int total_items = 1000;
        std::atomic<bool> producer_done{false};

        // Producer
        std::thread producer([this, total_items, &producer_done]
                             {
        for (int i = 0; i < total_items; ++i)
        {
            while (!buffer_->push(i))
            {
                // If the buffer is temporarily full, wait a little
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        }
        producer_done = true; });

        // Consumer
        int popped_count = 0;
        std::thread consumer([this, total_items, &producer_done, &popped_count]
                             {
        while (popped_count < total_items)
        {
            int value;
            if (buffer_->pop(value))
            {
                ++popped_count;
            }
            else
            {
                // if the producer is done and the buffer is empty, we're finished
                if (producer_done && buffer_->empty()) 
                    break;
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        } });

        producer.join();
        consumer.join();

        EXPECT_EQ(popped_count, total_items);
    }

    //------------------------------------------------------------------------------
    // 4) Multiple Producers, Single Consumer
    //------------------------------------------------------------------------------
    TEST_F(RingBufferTest, MultiProducerSingleConsumer)
    {
        const int total_items_per_thread = 500;
        const int num_producers = 4;
        std::atomic<int> global_count{0};

        // Launch producers
        std::vector<std::thread> producers;
        producers.reserve(num_producers);

        for (int t = 0; t < num_producers; ++t)
        {
            producers.emplace_back([this, t, total_items_per_thread, &global_count]
                                   {
            for (int i = 0; i < total_items_per_thread; ++i)
            {
                int data = t * 1000000 + i;
                while (!buffer_->push(data))
                {
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                }
                global_count.fetch_add(1, std::memory_order_relaxed);
            } });
        }

        // Single consumer
        std::thread consumer([this, &global_count, num_producers, total_items_per_thread]
                             {
        int popped = 0;
        int val;
        const int total_items = num_producers * total_items_per_thread;
        while (popped < total_items)
        {
            if (buffer_->pop(val))
            {
                popped++;
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        } });

        // Join all producers
        for (auto &p : producers)
            p.join();
        consumer.join();

        EXPECT_EQ(global_count.load(), num_producers * total_items_per_thread);
    }

    //------------------------------------------------------------------------------
    // 5) Multiple Producers, Multiple Consumers
    //------------------------------------------------------------------------------
    TEST_F(RingBufferTest, MultiProducerMultiConsumer)
    {
        // We create a separate buffer here for clarity
        stdx::concurrency::RingBuffer<int> buffer(512);

        const int total_items_per_producer = 300;
        const int num_producers = 3;
        const int num_consumers = 2;
        const int total_items = num_producers * total_items_per_producer;

        std::atomic<int> produced_count{0};
        std::atomic<int> consumed_count{0};

        // Launch producers
        std::vector<std::thread> producers;
        producers.reserve(num_producers);
        for (int t = 0; t < num_producers; ++t)
        {
            producers.emplace_back([&buffer, t, total_items_per_producer, &produced_count]
                                   {
            for (int i = 0; i < total_items_per_producer; ++i)
            {
                int data = t * 1000000 + i;
                while (!buffer.push(data))
                {
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                }
                produced_count.fetch_add(1, std::memory_order_relaxed);
            } });
        }

        // Launch consumers
        std::vector<std::thread> consumers;
        consumers.reserve(num_consumers);
        for (int c = 0; c < num_consumers; ++c)
        {
            consumers.emplace_back([&buffer, &consumed_count, total_items]
                                   {
            int val;
            while (consumed_count.load(std::memory_order_relaxed) < total_items)
            {
                if (buffer.pop(val))
                {
                    consumed_count.fetch_add(1, std::memory_order_relaxed);
                }
                else
                {
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                }
            } });
        }

        // Join producers
        for (auto &p : producers)
            p.join();
        // Join consumers
        for (auto &c : consumers)
            c.join();

        EXPECT_EQ(produced_count.load(), total_items);
        EXPECT_EQ(consumed_count.load(), total_items);
    }

    //------------------------------------------------------------------------------
    // 6) Callback Tests
    //------------------------------------------------------------------------------
    TEST_F(RingBufferTest, NonEmptyCallback)
    {
        std::atomic<int> callback_count{0};

        buffer_->set_nonempty_callback(
            stdx::LightCallable<void()>([&callback_count]()
                                         { callback_count.fetch_add(1, std::memory_order_relaxed); }));

        // Push one item
        EXPECT_TRUE(buffer_->push(1));
        EXPECT_EQ(callback_count.load(), 1);

        // Push another item (should not trigger callback again since was already non-empty)
        EXPECT_TRUE(buffer_->push(2));
        EXPECT_EQ(callback_count.load(), 1);

        // Pop both items
        int val;
        EXPECT_TRUE(buffer_->pop(val));
        EXPECT_TRUE(buffer_->pop(val));
        EXPECT_TRUE(buffer_->empty());

        // Push again (should trigger callback since buffer was empty)
        EXPECT_TRUE(buffer_->push(3));
        EXPECT_EQ(callback_count.load(), 2);
    }

    TEST_F(RingBufferTest, AlarmCallback)
    {
        std::atomic<int> alarm_count{0};

        // Use 80% of BUFFER_SIZE as a threshold
        std::size_t threshold = static_cast<std::size_t>(BUFFER_SIZE * 0.8);
        buffer_->set_alarm(threshold, [&alarm_count](std::size_t size)
                           { alarm_count.fetch_add(1, std::memory_order_relaxed); });

        // Push items below threshold
        for (int i = 0; i < static_cast<int>(threshold - 1); ++i)
        {
            EXPECT_TRUE(buffer_->push(i));
        }
        EXPECT_EQ(alarm_count.load(), 0);

        // Push one more to reach threshold
        EXPECT_TRUE(buffer_->push(static_cast<int>(threshold - 1)));
        EXPECT_EQ(alarm_count.load(), 1);

        // Push another to exceed threshold
        EXPECT_TRUE(buffer_->push(static_cast<int>(threshold)));
        EXPECT_EQ(alarm_count.load(), 2);
    }

    //------------------------------------------------------------------------------
    // main: typical Google Test entry
    //------------------------------------------------------------------------------
    int main(int argc, char **argv)
    {
        ::testing::InitGoogleTest(&argc, argv);
        return RUN_ALL_TESTS();
    }

} // namespace stdx
