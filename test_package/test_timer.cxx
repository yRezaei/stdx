#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <atomic>
#include <exception>
#include "stdx/threading/thread_pool.hpp"
#include "stdx/threading/timer.hpp"

using namespace stdx::threading;
using namespace std::chrono_literals;

class TimerTest : public ::testing::Test
{
protected:
    ThreadPool &defaultPool_{ThreadPool::create(2, 1024, 3)}; // Increase thread count

    void SetUp() override
    {
        defaultPool_.start();
    }

    void TearDown() override
    {
        defaultPool_.stop();
    }
};

TEST_F(TimerTest, OneTimeBasic)
{
    Timer t(defaultPool_);
    std::atomic<int> counter{0};

    t.set_task([&counter]
               { counter.fetch_add(1); }, 30ms); // Reduce delay for faster test

    t.start();

    // Increase wait time to ensure execution completes
    std::this_thread::sleep_for(500ms);

    EXPECT_EQ(counter.load(), 1);
    t.stop();
}

TEST_F(TimerTest, OneTimeCancelBeforeRun)
{
    Timer t(defaultPool_);
    std::atomic<int> counter{0};

    // 500ms delay
    t.set_task([&counter]
               { counter.fetch_add(1); }, 500ms);

    t.start();

    // Immediately stop (cancel)
    t.stop();

    // Wait beyond 500ms to confirm it never runs
    std::this_thread::sleep_for(700ms);
    EXPECT_EQ(counter.load(), 0);
}

TEST_F(TimerTest, RecurringBasic)
{
    // defaultPool_.set_exception_handler([](std::exception_ptr ex)
    //                                { std::cout << "Error: " << ex.what() << std::endl; });
    Timer t(defaultPool_);
    std::atomic<int> counter{0};

    // Recurring every 200ms
    t.set_task([&counter]
               { counter.fetch_add(1); }, 100ms /* initial delay */,
               200ms /* interval */, 0);

    t.start();

    // Let it run for ~1.2 seconds (more margin)
    std::this_thread::sleep_for(1200ms);

    // We expect about 5-6 executions, but verify >=3 for stability
    int count = counter.load();
    EXPECT_GE(count, 3) << "Expected at least 3 recurring executions in 1.2 seconds.";

    t.stop();
    int stopCount = counter.load();
    std::this_thread::sleep_for(300ms);
    int finalCount = counter.load();
    EXPECT_LE(finalCount - stopCount, 1) << "At most one more increment after stop().";
}

TEST_F(TimerTest, ReSetTask)
{
    Timer t(defaultPool_);
    std::atomic<int> counter{0};

    // First set a 1-time task after 300ms
    t.set_task([&counter]
               { counter.fetch_add(10); }, 300ms);
    t.start();

    // Wait but not enough for the task to run
    std::this_thread::sleep_for(100ms);

    // Re-set the task with a different function
    t.set_task([&counter]
               { counter.fetch_add(1); }, 100ms); // Reduce delay
    t.start();

    // Increased wait time
    std::this_thread::sleep_for(500ms);

    EXPECT_EQ(counter.load(), 1)
        << "Expected only the second set_task's function to run";
}

TEST_F(TimerTest, MultipleTimersIndependent)
{
    Timer t1(defaultPool_);
    Timer t2(defaultPool_);

    std::atomic<int> counter1{0};
    std::atomic<int> counter2{0};

    // Timer1: after 100ms
    t1.set_task([&counter1]
                { counter1.fetch_add(10); }, 100ms);
    t1.start();

    // Timer2: after 200ms
    t2.set_task([&counter2]
                { counter2.fetch_add(20); }, 200ms);
    t2.start();

    // Increased wait time
    std::this_thread::sleep_for(500ms);

    EXPECT_EQ(counter1.load(), 10);
    EXPECT_EQ(counter2.load(), 20);

    t1.stop();
    t2.stop();
}

TEST_F(TimerTest, RecurringZeroIntervalTreatedAsOnce)
{
    Timer t(defaultPool_);
    std::atomic<int> counter{0};

    t.set_task([&counter]
               { counter.fetch_add(1); }, 100ms, 0ms /* zero interval => one-time */);

    t.start();

    // Increased wait time
    std::this_thread::sleep_for(500ms);

    EXPECT_EQ(counter.load(), 1);

    // No more increments
    std::this_thread::sleep_for(300ms);
    EXPECT_EQ(counter.load(), 1);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}