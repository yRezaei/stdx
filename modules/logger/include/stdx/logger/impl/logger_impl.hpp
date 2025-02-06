#ifndef STDX_IMPL_LOGGER_IMPL_HPP
#define STDX_IMPL_LOGGER_IMPL_HPP

#include <string>
#include <queue>
#include <fstream>
#include <filesystem>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>

#include "stdx/logger/log_manager.hpp"

namespace stdx
{
namespace detail
{

    // The actual implementation that does the queue, the background thread,
    // file rotation, etc. This is "private" to the library.
    class LoggerImpl
    {
    public:
        LoggerImpl(const std::filesystem::path& file_path,
                   std::size_t max_file_size,
                   std::size_t max_backup_files,
                   RotationStrategy custom_strategy);

        ~LoggerImpl();

        void log(SEVERITY severity, const std::string& class_name, const std::string& message);
        void flush();

    private:
        // A struct for queued messages
        struct log_message
        {
            SEVERITY severity;
            std::string class_name;
            std::string message;
            std::string timestamp;
        };

        // Worker thread function
        void worker_thread_function();
        void stop_worker();

        // Rotation logic
        void rotate_file();

        // Utility
        static std::string severity_to_string(SEVERITY sev);
        static std::string get_time_stamp();

    private:
        std::filesystem::path file_path_;
        std::ofstream file_;
        std::mutex file_mutex_;

        std::queue<log_message> message_queue_;
        std::mutex queue_mutex_;
        std::condition_variable queue_cv_;

        std::mutex rotation_mutex_;
        std::size_t max_file_size_;
        std::size_t max_backup_files_;
        RotationStrategy custom_rotation_strategy_;

        std::atomic<bool> is_running_;
        std::thread worker_thread_;

        std::string first_timestamp_;
        std::string last_timestamp_;

        std::atomic<bool> force_flush_;
    };

} // namespace detail
} // namespace stdx

#endif // STDX_IMPL_LOGGER_IMPL_HPP
