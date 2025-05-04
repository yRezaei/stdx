#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <fstream>
#include <filesystem>
#include <vector>
#include <atomic>
#include "stdx/threading/thread_pool.hpp"
#include "stdx/logging/logger.hpp"

using namespace std::chrono_literals;

// Test fixture to set up and tear down the logging environment
class LoggerTest : public ::testing::Test {
protected:
    std::string log_path = "./test_logs";
    std::string log_file_name = "test.log";
    std::size_t batch_number = 2; // Small batch size for testing
    std::chrono::milliseconds write_threshold = 100ms;
    stdx::threading::ThreadPool& pool{stdx::threading::ThreadPool::create(2, 1024, 1)}; // Thread pool with 2 threads

    void SetUp() override {
        // Create a temporary directory for logs
        std::filesystem::create_directory(log_path);
        pool.start();
    }

    void TearDown() override {
        pool.stop();
        stdx::logging::Logger::shutdown();
        // Clean up the temporary directory
        // std::filesystem::remove_all(log_path);
    }

    // Helper function to read the log file contents
    std::string read_log_file() {
        std::ifstream log_file(log_path + "/" + log_file_name);
        std::stringstream buffer;
        buffer << log_file.rdbuf();
        return buffer.str();
    }
};

// Test 1: Verify initialization works and is idempotent
TEST_F(LoggerTest, Initialization) {
    stdx::logging::Logger::initialize(pool, log_path, log_file_name, batch_number, write_threshold);
    // Second call should have no effect (idempotency)
    stdx::logging::Logger::initialize(pool, "different_path", "different_file.log", 10, 500ms);

    stdx::logging::Logger logger("TestComponent");
    LOG_INFO(logger, "Initialization test");

    // Wait for asynchronous log write
    std::this_thread::sleep_for(200ms);
    stdx::logging::Logger::shutdown();

    std::string log_content = read_log_file();
    EXPECT_TRUE(log_content.find("[TestComponent] [INFO] Initialization test") != std::string::npos);
}

// Test 2: Verify log submission and batching
TEST_F(LoggerTest, LogSubmissionAndBatching) {
    stdx::logging::Logger::initialize(pool, log_path, log_file_name, batch_number, write_threshold);
    stdx::logging::Logger logger("BatchTest");

    // Submit logs to reach batch size
    LOG_INFO(logger, "Log 1");
    LOG_INFO(logger, "Log 2"); // Should trigger a write since batch_number = 2

    std::this_thread::sleep_for(200ms);
    stdx::logging::Logger::shutdown();
    std::string log_content = read_log_file();
    EXPECT_TRUE(log_content.find("Log 1") != std::string::npos);
    EXPECT_TRUE(log_content.find("Log 2") != std::string::npos);
}

// Test 3: Verify periodic flushing
TEST_F(LoggerTest, PeriodicFlushing) {
    stdx::logging::Logger::initialize(pool, log_path, log_file_name, batch_number, write_threshold);
    stdx::logging::Logger logger("FlushTest");

    // Submit a single log (less than batch size)
    LOG_INFO(logger, "Single log");

    // Wait longer than the flush interval
    std::this_thread::sleep_for(150ms); // write_threshold is 100ms
    stdx::logging::Logger::shutdown();
    std::string log_content = read_log_file();
    EXPECT_TRUE(log_content.find("Single log") != std::string::npos);
}

// Test 4: Verify log formatting
TEST_F(LoggerTest, LogFormatting) {
    stdx::logging::Logger::initialize(pool, log_path, log_file_name, batch_number, write_threshold);
    stdx::logging::Logger logger("FormatTest");

    LOG_INFO(logger, "Formatted log");

    std::this_thread::sleep_for(200ms);
    stdx::logging::Logger::shutdown();
    std::string log_content = read_log_file();
    // Check for expected format: timestamp with microseconds, component, severity, message
    EXPECT_TRUE(log_content.find("[FormatTest] [INFO] Formatted log") != std::string::npos);
    EXPECT_TRUE(log_content.find(".") != std::string::npos); // Microseconds in timestamp
}

// Test 5: Verify thread safety
TEST_F(LoggerTest, ThreadSafety) {
    stdx::logging::Logger::initialize(pool, log_path, log_file_name, batch_number, write_threshold);
    stdx::logging::Logger logger("ThreadTest");

    std::atomic<int> counter{0};
    std::vector<std::thread> threads;

    // Launch 5 threads to submit logs
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&logger, &counter] {
            LOG_INFO(logger, "Log from thread " + std::to_string(counter.fetch_add(1)));
        });
    }

    // Wait for all threads to finish
    for (auto& th : threads) {
        th.join();
    }

    std::this_thread::sleep_for(200ms);
    stdx::logging::Logger::shutdown();
    std::string log_content = read_log_file();
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(log_content.find("Log from thread " + std::to_string(i)) != std::string::npos);
    }
}

// Test 6: HighLoad of log
TEST_F(LoggerTest, HighLoad) {
    stdx::logging::Logger::initialize(pool, log_path, log_file_name, batch_number, write_threshold);
    stdx::logging::Logger logger("HighLoadTest");

    // Submit more logs than the ring buffer can hold (assuming capacity ~1024)
    for (int i = 0; i < 2000; ++i) {
        LOG_INFO(logger, "Log " + std::to_string(i));
    }

    std::this_thread::sleep_for(200ms);
    stdx::logging::Logger::shutdown();
    std::string log_content = read_log_file();
    // Check that some logs are written, but later ones may be dropped
    EXPECT_TRUE(log_content.find("Log 0") != std::string::npos);
    EXPECT_TRUE(log_content.find("Log 1999") != std::string::npos);
}

// Main function to run the tests
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}