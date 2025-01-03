#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include "stdx/logger.hpp"

// Global counter to track test failures
static int g_testFailures = 0;

// Helper to compare equality with descriptive output
void checkTrue(bool condition, const std::string& testName) {
    if (!condition) {
        std::cerr << "TEST FAILED: " << testName << " -> expected TRUE, got FALSE\n";
        g_testFailures++;
    }
}

// Helper to check if exception is thrown
template <typename Callable>
void checkThrows(Callable func, const std::string& testName) {
    try {
        func();
        std::cerr << "TEST FAILED: " << testName << " -> expected exception, but none thrown\n";
        g_testFailures++;
    } catch (...) {
        std::cout << "TEST PASSED: " << testName << "\n";
    }
}

// Test function
void test_logger() {
    using namespace stdx;

    // Initialize two loggers
    try {
        Logger::initialize("test_logger_1", "logs/test1.log");
        Logger::initialize("test_logger_2", "logs/test2.log");
    } catch (const std::exception& ex) {
        std::cerr << "Logger initialization failed: " << ex.what() << "\n";
        g_testFailures++;
        return;
    }

    // Retrieve the loggers
    const Logger& logger1 = Logger::get_instance("test_logger_1");
    const Logger& logger2 = Logger::get_instance("test_logger_2");

    // Perform logging
    logger1.log(Severity::INFO, "TestLogger1", "Logging INFO message.");
    logger1.log(Severity::ERROR, "TestLogger1", "Logging ERROR message.");
    logger2.log(Severity::DEBUG, "TestLogger2", "Logging DEBUG message.");

    // Ensure exception is thrown for uninitialized logger
    checkThrows(
        []() { Logger::get_instance("uninitialized_logger"); },
        "Accessing uninitialized logger"
    );

    // Ensure exception is thrown for duplicate initialization
    checkThrows(
        []() { Logger::initialize("test_logger_1", "logs/test3.log"); },
        "Duplicate logger initialization"
    );
}

// Multithreaded logging
void thread_logging(const std::string& logger_name) {
    const auto& logger = stdx::Logger::get_instance(logger_name);

    for (int i = 0; i < 5; ++i) {
        logger.log(stdx::Severity::DEBUG, "Thread", "Thread-safe logging test.");
    }
}

// Test thread safety
void test_thread_safety() {
    try {
        const stdx::Logger& logger = stdx::Logger::get_instance("test_logger_1");
        std::thread t1(thread_logging, "test_logger_1");
        std::thread t2(thread_logging, "test_logger_1");
        t1.join();
        t2.join();
    } catch (const std::exception& ex) {
        std::cerr << "Thread-safety test failed: " << ex.what() << "\n";
        g_testFailures++;
    }
}

int main() {
    std::cout << "Running Logger Tests...\n";

    test_logger();
    test_thread_safety();

    if (g_testFailures == 0) {
        std::cout << "All Logger tests passed!\n";
        return 0; // success
    } else {
        std::cerr << g_testFailures << " test(s) failed.\n";
        return 1; // failure
    }
}
