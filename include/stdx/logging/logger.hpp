#pragma once

#include <chrono>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>
#include "stdx/concurrency/ring_buffer.hpp"
#include "stdx/threading/thread_pool.hpp"
#include "stdx/threading/timer.hpp"
#include "stdx/light_callable.hpp"
#include "stdx/singleton.hpp"

#if defined(STDX_INCLUDE_EXPORT)
#include "stdx/stdx_export.hpp"
#else
#define STDX_API
#endif

namespace stdx
{
    namespace logging
    {
        namespace _impl
        {
            class LogImpl;
        } // namespace _impl

        // ### Severity class EnumLogImpl;
        enum class Severity : std::uint8_t
        {
            INFO,
            DEB,
            WARN,
            ERR
        };

        // ### LogEntry Struct
        struct STDX_API LogEntry
        {
            std::chrono::system_clock::time_point timestamp;
            std::thread::id thread_id;
            std::string component;
            Severity severity;
            std::string message;
        };

        class STDX_API Logger
        {
        public:
            explicit Logger(const std::string &component);
            ~Logger();

            void log(Severity sev, const std::string &message, std::chrono::system_clock::time_point timestamp);

            static void initialize(stdx::threading::ThreadPool &pool,
                                   const std::string &log_path,
                                   const std::string &log_file_name,
                                   std::size_t batch_number,
                                   std::chrono::milliseconds write_threshold);

            static void shutdown();

        private:
            std::string component_;
        };

        namespace _impl
        {
            // ### LogImpl Singleton (Private Implementation)
            class STDX_API LogImpl : public stdx::Singleton<LogImpl>
            {
                friend class stdx::Singleton<LogImpl>;
                friend class Logger; // Allow Logger to access submit_log

            public:
                ~LogImpl();

            private:
                LogImpl(stdx::threading::ThreadPool &pool,
                        const std::string &log_path,
                        const std::string &log_file_name,
                        std::size_t batch_number,
                        std::chrono::milliseconds write_threshold);

                void submit_log(LogEntry entry);
                void flush_logs();
                void write_logs(const std::vector<LogEntry> &batch);
                std::string format_log_entry(const LogEntry &entry);
                std::string severity_to_string(Severity sev);
                void shutdown();

            private:
                std::string log_path_;
                std::string log_file_name_;
                std::size_t batch_number_ = 0;
                std::chrono::milliseconds write_threshold_{0};
                stdx::concurrency::RingBuffer<LogEntry> ring_buffer_;
                stdx::threading::ThreadPool &pool_;
                stdx::threading::Timer timer_;
                std::mutex file_mutex_;
                std::atomic<bool> is_shutdown_{false};
                static std::mutex shutdown_mutex_;
            };
        }
    } // namespace logging

// ### Logging Macros
#define LOG_INFO(logger, message) \
    (logger).log(stdx::logging::Severity::INFO, (message), std::chrono::system_clock::now())
#define LOG_DEBUG(logger, message) \
    (logger).log(stdx::logging::Severity::DEB, (message), std::chrono::system_clock::now())
#define LOG_WARN(logger, message) \
    (logger).log(stdx::logging::Severity::WARN, (message), std::chrono::system_clock::now())
#define LOG_ERROR(logger, message) \
    (logger).log(stdx::logging::Severity::ERR, (message), std::chrono::system_clock::now())
}