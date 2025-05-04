#include "stdx/logging/logger.hpp"

namespace stdx
{
    namespace logging
    {
        namespace _impl
        {
            std::mutex LogImpl::shutdown_mutex_;

            LogImpl::~LogImpl()
            {
                timer_.stop();
            }

            LogImpl::LogImpl(stdx::threading::ThreadPool &pool,
                             const std::string &log_path,
                             const std::string &log_file_name,
                             std::size_t batch_number,
                             std::chrono::milliseconds write_threshold) : log_path_(log_path),
                                                                          log_file_name_(log_file_name),
                                                                          batch_number_(batch_number),
                                                                          write_threshold_(write_threshold),
                                                                          ring_buffer_(1024),
                                                                          pool_(pool),
                                                                          timer_(pool_)
            {
                // Set up periodic flush
                timer_.set_task([this]
                                { flush_logs(); }, write_threshold_, write_threshold_, 0);
                timer_.start();
            }

            void LogImpl::submit_log(LogEntry entry)
            {
                if (is_shutdown_.load(std::memory_order_acquire) || !ring_buffer_.push(std::move(entry)))
                {
                    return;
                }

                // Check batch size and submit if reached
                if (ring_buffer_.size() >= batch_number_)
                {
                    std::vector<LogEntry> batch;
                    ring_buffer_.pop_batch(batch, batch_number_);
                    if (!batch.empty())
                    {
                        pool_.submit(0, [batch = std::move(batch), this]
                                     { write_logs(batch); });
                    }
                }
            }

            void LogImpl::flush_logs()
            {
                std::vector<LogEntry> batch;
                ring_buffer_.pop_batch(batch, ring_buffer_.capacity());
                if (!batch.empty())
                {
                    pool_.submit(0, [batch = std::move(batch), this]
                                 { write_logs(batch); });
                }
            }

            void LogImpl::write_logs(const std::vector<LogEntry> &batch)
            {
                std::lock_guard<std::mutex> lock(file_mutex_);
                std::ofstream log_file(log_path_ + "/" + log_file_name_, std::ios::app);
                if (!log_file.is_open())
                {
                    return;
                }

                for (const auto &entry : batch)
                {
                    log_file << format_log_entry(entry) << std::endl;
                }
            }

            std::string LogImpl::format_log_entry(const LogEntry &entry)
            {
                std::ostringstream oss;
                auto time_t_timestamp = std::chrono::system_clock::to_time_t(entry.timestamp);
                auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                              entry.timestamp.time_since_epoch()) %
                          1000000;

                oss << "[" << std::put_time(std::localtime(&time_t_timestamp), "%Y-%m-%d %H:%M:%S")
                    << "." << std::setfill('0') << std::setw(6) << us.count() << "] "
                    << "[" << entry.thread_id << "] "
                    << "[" << entry.component << "] "
                    << "[" << severity_to_string(entry.severity) << "] "
                    << entry.message;
                return oss.str();
            }

            std::string LogImpl::severity_to_string(Severity sev)
            {
                switch (sev)
                {
                case Severity::INFO:
                    return "INFO";
                case Severity::DEB:
                    return "DEBUG";
                case Severity::WARN:
                    return "WARN";
                case Severity::ERR:
                    return "ERROR";
                default:
                    return "UNKNOWN";
                }
            }

            void LogImpl::shutdown()
            {
                bool expected = false;
                if (is_shutdown_.compare_exchange_strong(expected, true))
                {
                    timer_.stop();

                    // Flush remaining logs synchronously
                    std::vector<LogEntry> batch;
                    ring_buffer_.pop_batch(batch, ring_buffer_.capacity());

                    if (!batch.empty())
                    {
                        write_logs(batch);
                    }

                    // Wait for in-flight tasks to complete
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
            }
        }

        Logger::Logger(const std::string &component) : component_(component) {}

        Logger::~Logger() {}

        void Logger::log(Severity sev, const std::string &message, std::chrono::system_clock::time_point timestamp)
        {
            try
            {
                LogEntry entry{timestamp, std::this_thread::get_id(), component_, sev, message};
                _impl::LogImpl::instance().submit_log(std::move(entry));
            }
            catch (const std::runtime_error &)
            {
                // Logger is already destroyed, silently drop the log
            }
        }

        void Logger::initialize(stdx::threading::ThreadPool &pool,
                                const std::string &log_path,
                                const std::string &log_file_name,
                                std::size_t batch_number,
                                std::chrono::milliseconds write_threshold)
        {
            _impl::LogImpl::create(pool, log_path, log_file_name, batch_number, write_threshold);
        }

        void Logger::shutdown()
        {
            std::lock_guard<std::mutex> lock(_impl::LogImpl::shutdown_mutex_);
            try
            {
                _impl::LogImpl &impl = _impl::LogImpl::instance();
                impl.shutdown();
                _impl::LogImpl::destroy();
            }
            catch (const std::runtime_error &)
            {
                // Already destroyed
            }
        }
    } // namespace logging
} // namespace stdx