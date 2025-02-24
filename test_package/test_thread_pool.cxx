#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <functional>
#include "stdx/concurrency/thread_pool.hpp"
#include "stdx/concurrency/ring_buffer.hpp"

using namespace stdx;
using namespace stdx::concurrency;

// Test fixture for ThreadPool tests
class ThreadPoolTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        counter_.store(0, std::memory_order_relaxed);
    }

    void TearDown() override
    {
        // Ensure both pools are stopped and reset after each test
        if (pool_callable_16_)
        {
            pool_callable_16_->stop();
            pool_callable_16_.reset();
            buffer_callable_16_.reset();
        }
        if (pool_16_)
        {
            pool_16_->stop();
            pool_16_.reset();
            buffer_int_16_.reset();
        }
    }

    // Helper to create a ThreadPool with a callable buffer entry
    void SetupCallablePool16(std::size_t reserved_threads = 2, std::size_t min_threads = 1,
                             std::size_t max_threads = 4, std::size_t monitor_interval_ms = 50)
    {
        buffer_callable_16_ = std::make_unique<RingBuffer<std::function<void()>, 16>>();
        pool_callable_16_ = std::make_unique<ThreadPool<RingBuffer<std::function<void()>, 16>, std::function<void()>>>(
            *buffer_callable_16_, reserved_threads, min_threads, 1.5 /* spawn_ratio */, 0.5 /* shrink_ratio */,
            max_threads, monitor_interval_ms);
    }

    // Helper to create a ThreadPool with a callable buffer entry
    void SetupCallablePool256(std::size_t reserved_threads = 2, std::size_t min_threads = 1,
                              std::size_t max_threads = 4, std::size_t monitor_interval_ms = 50)
    {
        buffer_callable_256_ = std::make_unique<RingBuffer<std::function<void()>, 256>>();
        pool_callable_256_ = std::make_unique<ThreadPool<RingBuffer<std::function<void()>, 256>, std::function<void()>>>(
            *buffer_callable_256_, reserved_threads, min_threads, 1.5 /* spawn_ratio */, 0.5 /* shrink_ratio */,
            max_threads, monitor_interval_ms);
    }

    // Helper to create a ThreadPool with an external task
    void SetupTaskPool(std::size_t reserved_threads = 2, std::size_t min_threads = 1,
                       std::size_t max_threads = 4, std::size_t monitor_interval_ms = 50)
    {
        buffer_int_16_ = std::make_unique<RingBuffer<int, 16>>();
        pool_16_ = std::make_unique<ThreadPool<RingBuffer<int, 16>, int>>(
            *buffer_int_16_, [this](int &item)
            { counter_.fetch_add(item, std::memory_order_relaxed); },
            reserved_threads, min_threads, 1.5 /* spawn_ratio */, 0.5 /* shrink_ratio */,
            max_threads, monitor_interval_ms);
    }

    // Helper to create a ThreadPool with an external task
    void SetupTaskPool256(std::size_t reserved_threads = 2, std::size_t min_threads = 1,
                       std::size_t max_threads = 10, std::size_t monitor_interval_ms = 10)
    {
        buffer_int_256_ = std::make_unique<RingBuffer<int, 256>>();
        pool_256_ = std::make_unique<ThreadPool<RingBuffer<int, 256>, int>>(
            *buffer_int_256_, [this](int &item)
            { std::this_thread::sleep_for(std::chrono::milliseconds(50));
                counter_.fetch_add(item, std::memory_order_relaxed); },
            reserved_threads, min_threads, 1.5 /* spawn_ratio */, 0.5 /* shrink_ratio */,
            max_threads, monitor_interval_ms, 100, 1);
    }

    std::atomic<int> counter_{0};
    std::unique_ptr<RingBuffer<std::function<void()>, 16>> buffer_callable_16_;
    std::unique_ptr<RingBuffer<int, 16>> buffer_int_16_;
    std::unique_ptr<RingBuffer<std::function<void()>, 256>> buffer_callable_256_;
    std::unique_ptr<RingBuffer<int, 256>> buffer_int_256_;
    std::unique_ptr<ThreadPool<RingBuffer<std::function<void()>, 16>, std::function<void()>>> pool_callable_16_;
    std::unique_ptr<ThreadPool<RingBuffer<int, 16>, int>> pool_16_;
    std::unique_ptr<ThreadPool<RingBuffer<std::function<void()>, 256>, std::function<void()>>> pool_callable_256_;
    std::unique_ptr<ThreadPool<RingBuffer<int, 256>, int>> pool_256_;
};

// Test cases using callable pool
TEST_F(ThreadPoolTest, CallableBasicStartStop)
{
    SetupCallablePool16();
    pool_callable_16_->start();

    for (int i = 0; i < 5; ++i)
    {
        buffer_callable_16_->push([this]()
                               { counter_.fetch_add(1, std::memory_order_relaxed); });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    pool_callable_16_->stop();

    EXPECT_EQ(counter_.load(), 5);
    EXPECT_EQ(pool_callable_16_->get_total_threads(), 0);
}

TEST_F(ThreadPoolTest, CallableConcurrencySafety)
{
    SetupCallablePool256(2 /* reserved */, 2 /* min */, 4 /* max */);
    pool_callable_256_->start();

    std::vector<std::thread> producers;
    const int items_per_thread = 50;
    for (int t = 0; t < 3; ++t)
    {
        producers.emplace_back([this, items_per_thread]()
                               {
            for (int i = 0; i < items_per_thread; ++i) {
                buffer_callable_256_->push([this]() { counter_.fetch_add(1, std::memory_order_relaxed); });
            } });
    }

    for (auto &t : producers)
    {
        t.join();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    pool_callable_256_->stop();

    EXPECT_EQ(counter_.load(), 3 * items_per_thread);
}

// Test cases using task pool
TEST_F(ThreadPoolTest, TaskWorkerProcessing)
{
    SetupTaskPool();
    pool_16_->start();

    for (int i = 1; i <= 10; ++i)
    {
        buffer_int_16_->push(i);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    pool_16_->stop();

    EXPECT_EQ(counter_.load(), 55); // Sum of 1 to 10
    EXPECT_TRUE(buffer_int_16_->empty());
}

TEST_F(ThreadPoolTest, TaskScalingUp)
{
    SetupTaskPool256(1 /* reserved */, 1 /* min */, 10 /* max */, 10 /* monitor interval */);
    pool_256_->start();

    for (int i = 0; i < 350; ++i)
    {
        while (!buffer_int_256_->push(1)) {
            // sleep or yield
            std::this_thread::yield();
        }
        
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_GE(pool_256_->get_active_threads(), 4);
    
    pool_256_->stop();
    EXPECT_GT(counter_.load(), 4);
}

TEST_F(ThreadPoolTest, TaskScalingDown)
{
    SetupTaskPool(3 /* reserved */, 1 /* min */, 3 /* max */, 50 /* monitor interval */);
    pool_16_->start();

    for (int i = 0; i < 10; ++i)
    {
        buffer_int_16_->push(1);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::size_t active_before_stop = pool_16_->get_active_threads();
    pool_16_->stop();

    EXPECT_LT(active_before_stop, 3);
    EXPECT_GE(active_before_stop, 1);
    EXPECT_EQ(counter_.load(), 10);
}

TEST_F(ThreadPoolTest, TaskShutdownDuringOperation)
{
    SetupTaskPool();
    pool_16_->start();

    for (int i = 0; i < 100; ++i)
    {
        buffer_int_16_->push(1);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    pool_16_->stop();

    EXPECT_LE(counter_.load(), 100);
    EXPECT_GE(counter_.load(), 0);
}

TEST_F(ThreadPoolTest, TaskFullBuffer)
{
    SetupTaskPool();
    pool_16_->start();

    for (int i = 0; i < 16; ++i)
    {
        EXPECT_TRUE(buffer_int_16_->push(1));
    }
    EXPECT_FALSE(buffer_int_16_->push(1));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    pool_16_->stop();

    EXPECT_EQ(counter_.load(), 16);
}

//------------------------------------------------------------------------------
// main: typical Google Test entry
//------------------------------------------------------------------------------
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}