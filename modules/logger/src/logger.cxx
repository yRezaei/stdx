#include "stdx/logger.hpp"
#include <iomanip>
#include <sstream>
#include <chrono>
#include <ctime>
#include <stdexcept>
#include <utility>
#include <algorithm>
#include <iostream>

namespace stdx
{

    std::unordered_map<std::string, std::unique_ptr<Logger>> Logger::loggers_;
    std::mutex Logger::map_mutex_;

    void Logger::initialize(const std::string &name, const std::filesystem::path &file_path,
                            size_t max_file_size, size_t max_backup_files, RotationStrategy custom_strategy)
    {
        std::lock_guard<std::mutex> lock(map_mutex_);

        if (loggers_.find(name) != loggers_.end())
        {
            throw std::runtime_error("Logger with name '" + name + "' already exists.");
        }

        if (!std::filesystem::exists(file_path.parent_path()))
        {
            std::filesystem::create_directories(file_path.parent_path());
        }

        loggers_[name] = std::unique_ptr<Logger>(
            new Logger(file_path, max_file_size, max_backup_files, custom_strategy));
    }

    Logger &Logger::get_instance(const std::string &name)
    {
        std::lock_guard<std::mutex> lock(map_mutex_);

        auto it = loggers_.find(name);
        if (it == loggers_.end())
        {
            throw std::runtime_error("Logger with name '" + name + "' not initialized.");
        }

        return *(it->second);
    }

    void Logger::shutdown(const std::string &name)
    {
        std::lock_guard<std::mutex> lock(map_mutex_);
        if (name.empty())
        {
            // Shutdown all loggers
            for (auto &[name, logger] : loggers_)
            {
                logger->stop_worker(); // Stop worker thread for each logger
            }
            loggers_.clear(); // Clear the logger map
        }
        else
        {
            // Shutdown a specific logger
            auto it = loggers_.find(name);
            if (it != loggers_.end())
            {
                it->second->stop_worker();
                loggers_.erase(it);
            }
        }
    }

    Logger::Logger(const std::filesystem::path &file_path, size_t max_file_size,
                   size_t max_backup_files, RotationStrategy custom_strategy)
        : file_path_(file_path), file_(file_path, std::ios::app), max_file_size_(max_file_size),
          max_backup_files_(max_backup_files), custom_rotation_strategy_(std::move(custom_strategy)),
          is_running_(true), worker_thread_(&Logger::worker_thread_function, this),
          last_flush_time_(std::chrono::steady_clock::now())
    {
        if (!file_.is_open())
        {
            throw std::runtime_error("Failed to open log file: " + file_path_.string());
        }
    }

    Logger::~Logger()
    {
        stop_worker();
    }

    void Logger::log(Severity severity, const std::string &class_name, const std::string &message)
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        message_queue_.push({severity, class_name, message, get_time_stamp()});
        queue_cv_.notify_one();
    }

    void Logger::flush()
    {
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            force_flush_ = true;
        }

        queue_cv_.notify_all(); // Notify the worker thread

        // Wait until the queue is empty and the flush is complete
        while (true)
        {
            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                if (message_queue_.empty() && !force_flush_)
                {
                    break; // Exit if the queue is empty and force_flush_ is reset
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Minimal sleep to avoid busy waiting
        }
    }

    //  // Update timestamps
    // if (first_timestamp_.empty())
    // {
    //     first_timestamp_ = log_message.timestamp; // Set first timestamp for the log file
    // }
    // last_timestamp_ = log_message.timestamp; // Continuously update the last timestamp

    void Logger::worker_thread_function()
    {
        while (is_running_)
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);

            queue_cv_.wait(lock, [this]()
                           { return !message_queue_.empty() || !is_running_ || force_flush_; });

            while (!message_queue_.empty())
            {
                const auto log_message = std::move(message_queue_.front());
                message_queue_.pop();
                lock.unlock();

                {
                    // Lock rotation_mutex_ once for both writing and rotation
                    std::unique_lock<std::mutex> rotation_lock(rotation_mutex_);
                    std::lock_guard<std::mutex> file_lock(file_mutex_);

                    file_ << log_message.timestamp << " | "
                          << std::setw(10) << log_message.class_name << " | "
                          << std::setw(8) << severity_to_string(log_message.severity) << " | "
                          << log_message.message << std::endl;

                    // Update timestamps
                    if (first_timestamp_.empty())
                    {
                        first_timestamp_ = log_message.timestamp; // Set first timestamp for the log file
                    }
                    last_timestamp_ = log_message.timestamp; // Continuously update the last timestamp

                    if (std::filesystem::file_size(file_path_) >= max_file_size_)
                    {

                        rotate_file(); // rotation_mutex_ already locked
                    }
                }

                lock.lock();
            }

            if (force_flush_)
            {
                force_flush_ = false;
                queue_cv_.notify_all();
            }
        }
    }

    void Logger::rotate_file()
    {
        file_.close();

        std::string timestamped_name = file_path_.stem().string() + "-" +
                                       first_timestamp_ + "-" + last_timestamp_ + file_path_.extension().string();
        if (!std::filesystem::exists(file_path_.parent_path() / "history"))
        {
            std::filesystem::create_directories(file_path_.parent_path() / "history");
        }
        std::filesystem::path rotated_file = file_path_.parent_path() / "history" / timestamped_name;

        std::filesystem::rename(file_path_, rotated_file);

        first_timestamp_.clear();
        last_timestamp_.clear();

        file_.open(file_path_, std::ios::trunc);
        if (!file_.is_open())
        {
            throw std::runtime_error("Failed to reopen log file: " + file_path_.string());
        }
    }

    void Logger::stop_worker()
    {
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            is_running_ = false;
        }
        queue_cv_.notify_all();
        if (worker_thread_.joinable())
        {
            worker_thread_.join();
        }
    }

    std::string Logger::severity_to_string(Severity severity)
    {
        switch (severity)
        {
        case Severity::INFO:
            return "INFO";
        case Severity::DEB:
            return "DEBUG";
        case Severity::WARN:
            return "WARNING";
        case Severity::ERR:
            return "ERROR";
        default:
            return "UNKNOWN";
        }
    }

    std::string Logger::get_time_stamp()
    {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        auto duration_since_epoch = now.time_since_epoch();
        auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(duration_since_epoch).count() % 1'000'000;

        std::tm local_time;
        if (localtime_s(&local_time, &time_t_now) != 0)
        {
            throw std::runtime_error("Failed to get local time");
        }

        std::ostringstream oss;
        oss << std::put_time(&local_time, "%Y_%m_%d-%H_%M_%S") << "."
            << std::setw(6) << std::setfill('0') << microseconds; // Add microseconds
        return oss.str();
    }

    void Logger::clear_logger(const std::string &name)
    {
        std::lock_guard<std::mutex> lock(map_mutex_);
        loggers_.erase(name);
    }

} // namespace stdx
