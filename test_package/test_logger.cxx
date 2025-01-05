#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <fstream>
#include <chrono>
#include <filesystem>
#include <stdx/logger.hpp>

static int g_testFailures = 0;

void checkTrue(bool condition, const std::string& testName) {
    if (!condition) {
        std::cerr << "TEST FAILED: " << testName << " -> expected TRUE, got FALSE\n";
        g_testFailures++;
    }
}

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

void validateLogFile(const std::string& file_path, const std::string& expected_content, const std::string& testName) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        std::cerr << "TEST FAILED: " << testName << " -> unable to open log file\n";
        g_testFailures++;
        return;
    }

    std::string line;
    bool found = false;
    while (std::getline(file, line)) {
        if (line.find(expected_content) != std::string::npos) {
            found = true;
            break;
        }
    }

    if (!found) {
        std::cerr << "TEST FAILED: " << testName << " -> expected content not found in log file\n";
        g_testFailures++;
    } else {
        std::cout << "TEST PASSED: " << testName << "\n";
    }
}

void test_logger() {
    using namespace stdx;

    // Initialize loggers
    try {
        Logger::initialize("test_logger_async", "logs/async_test.log");
    } catch (const std::exception& ex) {
        std::cerr << "Logger initialization failed: " << ex.what() << "\n";
        g_testFailures++;
        return;
    }

    // Retrieve the logger
    Logger& logger = Logger::get_instance("test_logger_async");

    // Perform logging
    logger.log(Severity::INFO, "AsyncTest", "Logging INFO message.");
    logger.log(Severity::ERROR, "AsyncTest", "Logging ERROR message.");

    // Wait briefly to allow the worker thread to process the messages
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Validate log file content
    validateLogFile("logs/async_test.log", "Logging INFO message", "Log content validation for INFO");
    validateLogFile("logs/async_test.log", "Logging ERROR message", "Log content validation for ERROR");
}

void test_high_load_logging() {
    using namespace stdx;

    // Retrieve the logger
    Logger& logger = Logger::get_instance("test_logger_async");

    // Log a large number of messages
    for (int i = 0; i < 1000; ++i) {
        logger.log(Severity::DEBUG, "HighLoadTest", "Logging message #" + std::to_string(i));
    }

    // Wait to ensure all messages are processed
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Validate that the log file contains some of the expected messages
    validateLogFile("logs/async_test.log", "Logging message #0", "High-load log validation for first message");
    validateLogFile("logs/async_test.log", "Logging message #999", "High-load log validation for last message");
}

void cleanup_logs() {
    try {
        std::filesystem::remove_all("logs");
    } catch (const std::exception& ex) {
        std::cerr << "Log cleanup failed: " << ex.what() << "\n";
    }
}

int main() {
    std::cout << "Running Logger Tests...\n";

    test_logger();
    test_high_load_logging();

    if (g_testFailures == 0) {
        std::cout << "All Logger tests passed!\n";
    } else {
        std::cerr << g_testFailures << " test(s) failed.\n";
    }

    // cleanup_logs();

    return g_testFailures == 0 ? 0 : 1;
}
