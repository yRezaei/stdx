#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <future>
#include <random>
#include "stdx/threading/thread_pool.hpp"

// If you want to keep your old "DefaultTaskPriority" enum in the test:
enum class DefaultTaskPriority : unsigned
{
    HIGH = 0,
    MEDIUM = 1,
    LOW = 2,
    NUM_PRIORITIES = 3
};

// A helper to convert that old enum to an integer index:
static std::size_t to_index(DefaultTaskPriority pri)
{
    // e.g., HIGH=0, MEDIUM=1, LOW=2
    return static_cast<std::size_t>(pri);
}

using namespace stdx::threading;
using namespace std::chrono_literals;

/**
 * @brief Test fixture. Each test can create a new ThreadPool with a chosen
 *        number of threads, capacity, and # of priorities.
 */
class ThreadPoolTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Usually empty. We'll create a new thread pool inside each test if needed.
    }

    void TearDown() override
    {
        // Possibly call ThreadPool::destroy() if your design expects that.
        // But each test here calls it at the end manually.
    }

    // Helper function to simulate a small "sleep" returning a value
    static int sleepTask(int duration, int return_val)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(duration));
        return return_val;
    }
};

//-----------------------------------------------------------
// 1) Basic functionality tests
//-----------------------------------------------------------
TEST_F(ThreadPoolTest, ConstructorTest)
{
    // (A) Default: 0 threads => we expect the pool to have 0 worker threads
    {
        // create( numThreads=0, bufferCapacity=1024, nr_of_priorities=3 )
        auto &pool = ThreadPool::create(0, 1024, 3);
        EXPECT_EQ(pool.get_thread_count(), 0);

        // Optionally destroy
        ThreadPool::destroy();
    }

    // (B) Specific thread count (4)
    {
        auto &pool = ThreadPool::create(4, 1024, 3);
        pool.start();
        EXPECT_EQ(pool.get_thread_count(), 4);
        pool.stop();
        ThreadPool::destroy();
    }

    // (C) Zero => defaults to 1 thread
    {
        auto &pool = ThreadPool::create(0, 1024, 3);
        pool.start();
        EXPECT_EQ(pool.get_thread_count(), 1);
        pool.stop();
        ThreadPool::destroy();
    }
}

TEST_F(ThreadPoolTest, SimpleTaskExecution)
{
    // create(2 threads, capacity=1024, 3 priorities)
    auto &pool = ThreadPool::create(2, 1024, 3);
    pool.start();

    std::atomic<int> counter{0};
    std::vector<std::future<int>> futures;

    // Submit 10 tasks (priority=MEDIUM => index=1)
    for (int i = 0; i < 10; i++)
    {
        auto fut = pool.submit(to_index(DefaultTaskPriority::MEDIUM), [&counter, i]() -> int
                               {
            counter.fetch_add(1);
            return i; });
        futures.push_back(std::move(fut));
    }

    for (auto &f : futures)
    {
        f.wait();
    }
    pool.stop();

    EXPECT_EQ(counter.load(), 10);

    // Reset the singleton if you want to allow a fresh instance in other tests
    ThreadPool::destroy();
}

TEST_F(ThreadPoolTest, TaskWithResult)
{
    // create(2 threads, capacity=1024, 3 priorities)
    auto &pool = ThreadPool::create(2, 1024, 3);
    pool.start();

    auto result_future = pool.submit(to_index(DefaultTaskPriority::HIGH), []
                                     { return 42; });
    EXPECT_EQ(result_future.get(), 42);

    pool.stop();
    ThreadPool::destroy();
}

TEST_F(ThreadPoolTest, MultipleTasksWithResults)
{
    auto &pool = ThreadPool::create(4, 1024, 3);
    pool.start();

    std::vector<std::future<int>> futures;
    const int NUM_TASKS = 100;

    for (int i = 0; i < NUM_TASKS; i++)
    {
        auto fut = pool.submit(to_index(DefaultTaskPriority::MEDIUM), [i]()
                               { return i * i; });
        futures.push_back(std::move(fut));
    }

    for (int i = 0; i < NUM_TASKS; i++)
    {
        EXPECT_EQ(futures[i].get(), i * i);
    }

    pool.stop();
    ThreadPool::destroy();
}

//-----------------------------------------------------------
// 2) Priority testing
//-----------------------------------------------------------
TEST_F(ThreadPoolTest, PriorityOrder)
{
    // One thread => it checks ring buffers in ascending index => 0 first, 1 next, 2 last
    auto &pool = ThreadPool::create(1, 1024, 3);
    pool.start();

    std::atomic<int> executionCounter{0};
    std::array<int, 6> executionOrder{0};
    std::vector<std::future<int>> futures;

    // We'll submit tasks at indexes: 2(Low), 1(Med), 0(High) to see if index=0 tasks run first
    //  Low=2
    for (int i = 0; i < 2; i++)
    {
        auto fut = pool.submit(to_index(DefaultTaskPriority::LOW), [&executionCounter, &executionOrder]() -> int
                               {
            std::this_thread::sleep_for(10ms);
            int pos = executionCounter.fetch_add(1);
            executionOrder[pos] = 2; 
            return 0; });
        futures.push_back(std::move(fut));
    }
    //  Medium=1
    for (int i = 0; i < 2; i++)
    {
        auto fut = pool.submit(to_index(DefaultTaskPriority::MEDIUM), [&executionCounter, &executionOrder]() -> int
                               {
            std::this_thread::sleep_for(10ms);
            int pos = executionCounter.fetch_add(1);
            executionOrder[pos] = 1;
            return 0; });
        futures.push_back(std::move(fut));
    }
    //  High=0
    for (int i = 0; i < 2; i++)
    {
        auto fut = pool.submit(to_index(DefaultTaskPriority::HIGH), [&executionCounter, &executionOrder]() -> int
                               {
            std::this_thread::sleep_for(10ms);
            int pos = executionCounter.fetch_add(1);
            executionOrder[pos] = 0;
            return 0; });
        futures.push_back(std::move(fut));
    }

    for (auto &f : futures)
        f.wait();

    pool.stop();
    ThreadPool::destroy();

    // We expect: tasks at index=0 (HIGH) run first, then index=1 (MEDIUM), then 2 (LOW)
    EXPECT_EQ(executionCounter.load(), 6);
    EXPECT_EQ(executionOrder[0], 0);
    EXPECT_EQ(executionOrder[1], 0);
    EXPECT_EQ(executionOrder[2], 1);
    EXPECT_EQ(executionOrder[3], 1);
    EXPECT_EQ(executionOrder[4], 2);
    EXPECT_EQ(executionOrder[5], 2);
}

//-----------------------------------------------------------
// 3) Statistics tests
//-----------------------------------------------------------
TEST_F(ThreadPoolTest, StatisticsTracking)
{
    {
        auto &pool = ThreadPool::create(2, 1024, 3);
        pool.start();

        // Submit a mix of tasks
        for (int i = 0; i < 10; i++)
        {
            pool.submit(to_index(DefaultTaskPriority::HIGH), []
                        { std::this_thread::sleep_for(5ms); });
        }
        for (int i = 0; i < 5; i++)
        {
            pool.submit(to_index(DefaultTaskPriority::MEDIUM), []
                        { std::this_thread::sleep_for(5ms); });
        }

        std::this_thread::sleep_for(3s);
        pool.stop(ShutdownMode::Graceful);

        auto stats = pool.get_stats();
        EXPECT_EQ(stats.tasks_submitted, 15ul);
        EXPECT_EQ(stats.tasks_completed, 15ul);
        EXPECT_EQ(stats.tasks_by_priority[to_index(DefaultTaskPriority::HIGH)], 10ul);
        EXPECT_EQ(stats.tasks_by_priority[to_index(DefaultTaskPriority::MEDIUM)], 5ul);
        EXPECT_EQ(stats.tasks_by_priority[to_index(DefaultTaskPriority::LOW)], 0ul);

        ThreadPool::destroy();
    }

    {
        auto &pool2 = ThreadPool::create(2, 1024, 3);
        pool2.start();

        pool2.reset_stats();
        auto stats2 = pool2.get_stats();
        EXPECT_EQ(stats2.tasks_submitted, 0ul);
        EXPECT_EQ(stats2.tasks_completed, 0ul);

        // Submit a single task
        pool2.submit(to_index(DefaultTaskPriority::LOW), []() {});
        pool2.stop(ShutdownMode::Graceful);

        stats2 = pool2.get_stats();
        EXPECT_EQ(stats2.tasks_submitted, 1ul);
        EXPECT_EQ(stats2.tasks_completed, 1ul);
        EXPECT_EQ(stats2.tasks_by_priority[to_index(DefaultTaskPriority::LOW)], 1ul);

        ThreadPool::destroy();
    }
}

//-----------------------------------------------------------
// 4) Performance tests
//-----------------------------------------------------------
TEST_F(ThreadPoolTest, ConcurrentSubmissions)
{
    auto &pool = ThreadPool::create(4, 1024, 3);
    pool.start();

    std::atomic<int> counter{0};
    const int THREADS = 4;
    const int TASKS_PER_THREAD = 100;

    std::vector<std::thread> submitters;
    submitters.reserve(THREADS);

    for (int t = 0; t < THREADS; t++)
    {
        submitters.emplace_back([&pool, &counter]()
                                {
            for (int i = 0; i < TASKS_PER_THREAD; i++)
            {
                // cycle priority among 0..2
                std::size_t prio_index = (i % 3);
                try
                {
                    pool.submit(prio_index, [&counter] {
                        counter.fetch_add(1);
                        std::this_thread::sleep_for(1ms);
                    });
                }
                catch (const BufferFull&)
                {
                    // If ring buffer is full, back off
                    std::this_thread::sleep_for(5ms);
                }
            } });
    }

    for (auto &th : submitters)
        th.join();

    // Wait up to 5s for tasks
    auto start = std::chrono::steady_clock::now();
    const int totalTasks = THREADS * TASKS_PER_THREAD;
    while (counter < totalTasks)
    {
        std::this_thread::sleep_for(100ms);
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed > 5s)
            break;
    }

    pool.stop();
    ThreadPool::destroy();

    // Might not get all tasks if ring buffers got full
    EXPECT_GE(counter.load(), totalTasks * 0.9);
}

TEST_F(ThreadPoolTest, LongRunningTasks)
{
    auto &pool = ThreadPool::create(4, 1024, 3);
    pool.start();

    const int NUM_TASKS = 8;
    std::vector<std::future<int>> futures;
    futures.reserve(NUM_TASKS);

    // Submit 8 tasks that each sleep ~100ms
    for (int i = 0; i < NUM_TASKS; i++)
    {
        auto fut = pool.submit(to_index(DefaultTaskPriority::MEDIUM), [i]()
                               {
            std::this_thread::sleep_for(100ms);
            return i; });
        futures.push_back(std::move(fut));
    }

    for (int i = 0; i < NUM_TASKS; i++)
    {
        EXPECT_EQ(futures[i].get(), i);
    }

    auto stats = pool.get_stats();
    // average ~100ms => 100000 microseconds
    EXPECT_GE(stats.avg_execution_time_us, 100000ul);

    pool.stop();
    ThreadPool::destroy();
}

//-----------------------------------------------------------
// 5) Exception handling tests
//-----------------------------------------------------------
TEST_F(ThreadPoolTest, TaskExceptions)
{
    {
        auto &pool = ThreadPool::create(2, 1024, 3);
        pool.start();

        for (int i = 0; i < 5; i++)
        {
            pool.submit(to_index(DefaultTaskPriority::MEDIUM), []
                        { throw std::runtime_error("Void task exception"); });
        }

        pool.stop(ShutdownMode::Graceful);

        auto stats = pool.get_stats();
        EXPECT_EQ(stats.exception_count, 5ul);

        ThreadPool::destroy();
    }

    {
        auto &pool = ThreadPool::create(2, 1024, 3);
        std::atomic<int> exceptionCounter{0};

        pool.set_exception_handler([&exceptionCounter](std::exception_ptr)
                                   { exceptionCounter.fetch_add(1, std::memory_order_relaxed); });
        pool.start();

        for (int i = 0; i < 5; i++)
        {
            pool.submit(to_index(DefaultTaskPriority::MEDIUM), []
                        { throw std::runtime_error("Void task exception"); });
        }

        pool.stop(ShutdownMode::Graceful);

        // Our custom handler got them all
        EXPECT_EQ(exceptionCounter.load(std::memory_order_relaxed), 5);

        // Built-in stats for exceptions remain 0, since we used a custom handler
        auto stats = pool.get_stats();
        EXPECT_EQ(stats.exception_count, 0ul);

        ThreadPool::destroy();
    }
}

TEST_F(ThreadPoolTest, CustomExceptionHandler)
{
    auto &pool = ThreadPool::create(2, 1024, 3);

    std::atomic<int> exceptionCount{0};
    pool.set_exception_handler([&exceptionCount](std::exception_ptr)
                               { exceptionCount++; });
    pool.start();

    // Submit tasks that throw
    for (int i = 0; i < 5; i++)
    {
        pool.submit(to_index(DefaultTaskPriority::MEDIUM), []
                    { throw std::runtime_error("Test exception"); });
    }

    pool.stop(ShutdownMode::Graceful);

    // Verify custom handler caught all exceptions
    EXPECT_EQ(exceptionCount.load(), 5);

    // Built-in stats for exceptions remain 0, since we used a custom handler
    auto stats = pool.get_stats();
    EXPECT_EQ(stats.exception_count, 0ul);

    ThreadPool::destroy();
}

//-----------------------------------------------------------
// 6) Shutdown tests
//-----------------------------------------------------------
TEST_F(ThreadPoolTest, ImmediateShutdown)
{
    auto &pool = ThreadPool::create(2, 1024, 3);
    pool.start();

    std::atomic<int> counter{0};

    for (int i = 0; i < 10; i++)
    {
        pool.submit(to_index(DefaultTaskPriority::MEDIUM), [&counter, i]
                    {
            std::this_thread::sleep_for(500ms);
            counter++; });
    }

    std::this_thread::sleep_for(100ms);
    pool.stop(ShutdownMode::Immediate); // Clears tasks

    // Some tasks may have finished, but not all 10
    EXPECT_LT(counter.load(), 10);

    ThreadPool::destroy();
}

TEST_F(ThreadPoolTest, DrainShutdown)
{
    auto &pool = ThreadPool::create(4, 1024, 3);
    pool.start();

    std::atomic<int> counter{0};

    // Submit a few quick tasks
    for (int i = 0; i < 5; i++)
    {
        pool.submit(to_index(DefaultTaskPriority::HIGH), [&counter]()
                    { counter.fetch_add(1); });
    }

    std::this_thread::sleep_for(50ms);
    // "Graceful" => let tasks finish
    pool.stop(ShutdownMode::Graceful);

    // All tasks should have completed
    EXPECT_EQ(counter.load(), 5);

    ThreadPool::destroy();
}

TEST_F(ThreadPoolTest, FullRingBuffer)
{
    // Very small ring buffer (capacity=8)
    // with 3 priorities
    auto &pool = ThreadPool::create(1, 8, 3);
    pool.start();

    std::atomic<int> counter{0};
    std::atomic<int> rejections{0};
    std::vector<std::future<void>> futures;

    // Submit 20 tasks, likely to exceed capacity
    for (int i = 0; i < 20; i++)
    {
        auto fut = pool.submit(to_index(DefaultTaskPriority::MEDIUM), [&counter]
                               {
            std::this_thread::sleep_for(10ms);
            counter++; });
        futures.push_back(std::move(fut));
    }

    std::this_thread::sleep_for(100ms);

    // Check for BufferFull exceptions
    for (auto &f : futures)
    {
        try
        {
            f.get();
        }
        catch (const BufferFull &)
        {
            rejections++;
        }
        catch (...)
        {
            // ignore
        }
    }

    pool.stop(ShutdownMode::Graceful);

    // Some tasks were likely rejected
    EXPECT_GT(rejections.load(), 0);
    EXPECT_EQ(counter.load() + rejections.load(), 20);

    auto stats = pool.get_stats();
    EXPECT_EQ(stats.submission_failures, size_t(rejections.load()));

    ThreadPool::destroy();
}

//
// main test runner
//
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
