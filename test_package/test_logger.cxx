#include <gtest/gtest.h>
#include <thread>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <stdx/logger/log_manager.hpp> // <-- Includes LogManager & Logger
#include <stdx/logger/logger.hpp>

using namespace stdx;

// Helper function to validate log file content
void validateLogFile(const std::string &file_path, const std::string &expected_content)
{
    std::ifstream file(file_path);
    ASSERT_TRUE(file.is_open()) << "Unable to open log file: " << file_path;

    std::string line;
    bool found = false;
    while (std::getline(file, line))
    {
        if (line.find(expected_content) != std::string::npos)
        {
            found = true;
            break;
        }
    }

    ASSERT_TRUE(found) << "Expected content not found in log file: " << expected_content;
}

// Test fixture for Logger tests
class LoggerTests : public ::testing::Test
{
protected:
    std::string log_file_name_;

    void SetUp() override
    {
        // Generate a log file name based on the current test case name
        const ::testing::TestInfo *test_info = ::testing::UnitTest::GetInstance()->current_test_info();
        log_file_name_ = "logs/" + std::string(test_info->name()) + ".log";

        // Initialize the LogManager with the generated log file name
        // (here we use 100 * 1024 ≈ 100KB as the max file size, 3 backups)
        LogManager::initialize(log_file_name_, 10 * 1024, 3);
    }

    void TearDown() override
    {
        // Shut down the logging system
        // (Make sure you have implemented LogManager::shutdown())
        LogManager::shutdown();

        // Clean up all logs after the test
        try
        {
            std::filesystem::remove_all("logs");
        }
        catch (const std::exception &ex)
        {
            std::cerr << "Log cleanup failed: " << ex.what() << "\n";
        }
    }
};

// Test logging basic messages
TEST_F(LoggerTests, LogBasicMessages)
{
    // Create a Logger for "BasicTest"
    auto logger = LogManager::create_logger("BasicTest");

    // Log messages
    logger.log(SEVERITY::INFO, "Logging INFO message.");
    logger.log(SEVERITY::ERR, "Logging ERROR message.");

    // Flush logs to ensure all messages are written
    logger.flush();

    // Validate log file content
    validateLogFile(log_file_name_, "Logging INFO message");
    validateLogFile(log_file_name_, "Logging ERROR message");
}

// Test logging under high load
TEST_F(LoggerTests, HighLoadLogging)
{
    // Create a Logger for "HighLoadTest"
    auto logger = LogManager::create_logger("HighLoadTest");

    // Log a large number of messages
    for (int i = 0; i < 1000; ++i)
    {
        logger.log(SEVERITY::DEB, "Logging message #" + std::to_string(i));
    }

    // Flush logs to ensure all messages are written
    logger.flush();

    // Check for log messages in the active log file first
    bool found_message_0 = false;
    bool found_message_999 = false;

    std::filesystem::path active_log_file = log_file_name_; // e.g. "logs/HighLoadLogging.log"
    if (std::filesystem::exists(active_log_file))
    {
        std::ifstream file(active_log_file);
        ASSERT_TRUE(file.is_open()) << "Unable to open active log file: " << active_log_file;

        std::string line;
        while (std::getline(file, line))
        {
            if (line.find("Logging message #0") != std::string::npos)
            {
                found_message_0 = true;
            }
            if (line.find("Logging message #999") != std::string::npos)
            {
                found_message_999 = true;
            }
        }
    }

    // If not found in the active log file, check the rotated files in the history folder
    if (!found_message_0 || !found_message_999)
    {
        std::filesystem::path history_dir = "logs/history";
        ASSERT_TRUE(std::filesystem::exists(history_dir)) << "History folder does not exist";
        ASSERT_TRUE(std::filesystem::is_directory(history_dir)) << "History folder is not a directory";

        for (const auto &entry : std::filesystem::directory_iterator(history_dir))
        {
            if (entry.is_regular_file())
            {
                std::ifstream file(entry.path());
                ASSERT_TRUE(file.is_open()) << "Unable to open rotated log file: " << entry.path();

                std::string line;
                while (std::getline(file, line))
                {
                    if (line.find("Logging message #0") != std::string::npos)
                    {
                        found_message_0 = true;
                    }
                    if (line.find("Logging message #999") != std::string::npos)
                    {
                        found_message_999 = true;
                    }
                }
            }
        }
    }

    ASSERT_TRUE(found_message_0) << "Logging message #0 not found in any log file (active or rotated)";
    ASSERT_TRUE(found_message_999) << "Logging message #999 not found in any log file (active or rotated)";
}

// Test log file rotation
TEST_F(LoggerTests, LogFileRotation)
{
    auto logger = LogManager::create_logger("RotationTest");

    // Log enough messages to trigger file rotation
    for (int i = 0; i < 3000; ++i)
    {
        logger.log(SEVERITY::INFO, "Message #" + std::to_string(i));
    }

    // Wait briefly for the worker thread to process messages
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Flush logs to ensure rotation is complete
    logger.flush();

    // Check if any file exists in the history folder
    std::filesystem::path history_dir = "logs/history";
    bool history_exists = std::filesystem::exists(history_dir) && std::filesystem::is_directory(history_dir);

    ASSERT_TRUE(history_exists) << "History folder does not exist";

    bool file_found = false;
    for (const auto &entry : std::filesystem::directory_iterator(history_dir))
    {
        if (entry.is_regular_file())
        {
            file_found = true;
            break;
        }
    }

    ASSERT_TRUE(file_found) << "No rotated log file found in history folder";
}

// Test buffered writing with time threshold
TEST_F(LoggerTests, BufferedWritingTimeThreshold)
{
    auto logger = LogManager::create_logger("BufferTest");

    // Log fewer than the flush threshold
    for (int i = 0; i < 5; ++i)
    {
        logger.log(SEVERITY::DEB, "Buffered message #" + std::to_string(i));
    }

    // Wait for the time threshold to elapse (assuming 500ms or so in your impl)
    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    // Flush logs to ensure all buffered messages are written
    logger.flush();

    // Validate that the messages were written despite not meeting the count threshold
    for (int i = 0; i < 5; ++i)
    {
        validateLogFile(log_file_name_, "Buffered message #" + std::to_string(i));
    }
}

// Test buffered writing with message threshold
TEST_F(LoggerTests, BufferedWritingMessageThreshold)
{
    auto logger = LogManager::create_logger("BufferTest");

    // Log exactly the flush threshold number of messages (assuming flush_threshold = 10)
    for (int i = 0; i < 10; ++i)
    {
        logger.log(SEVERITY::DEB, "Buffered message #" + std::to_string(i));
    }

    // Wait briefly for the worker thread to process
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Validate that all messages were written
    for (int i = 0; i < 10; ++i)
    {
        validateLogFile(log_file_name_, "Buffered message #" + std::to_string(i));
    }
}

// Main entry point for Google Test
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
