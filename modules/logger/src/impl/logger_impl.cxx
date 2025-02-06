#include "stdx/logger/impl/logger_impl.hpp"

#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace stdx
{
namespace detail
{

LoggerImpl::LoggerImpl(const std::filesystem::path& file_path,
                       std::size_t max_file_size,
                       std::size_t max_backup_files,
                       RotationStrategy custom_strategy)
    : file_path_(file_path),
      max_file_size_(max_file_size),
      max_backup_files_(max_backup_files),
      custom_rotation_strategy_(std::move(custom_strategy)),
      is_running_(true),
      worker_thread_(),
      force_flush_(false)
{
    // Ensure directories exist
    if (!file_path_.parent_path().empty() && !std::filesystem::exists(file_path_.parent_path()))
    {
        std::filesystem::create_directories(file_path_.parent_path());
    }

    // Open for append
    file_.open(file_path_, std::ios::app);
    if (!file_.is_open())
    {
        throw std::runtime_error("LoggerImpl: Cannot open file " + file_path_.string());
    }

    // Start thread
    worker_thread_ = std::thread(&LoggerImpl::worker_thread_function, this);
}

LoggerImpl::~LoggerImpl()
{
    stop_worker();
}

void LoggerImpl::log(SEVERITY severity, const std::string& class_name, const std::string& message)
{
    log_message entry;
    entry.severity = severity;
    entry.class_name = class_name;
    entry.message = message;
    entry.timestamp = get_time_stamp();

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        message_queue_.push(std::move(entry));
    }
    queue_cv_.notify_one();
}

void LoggerImpl::flush()
{
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        force_flush_ = true;
    }
    queue_cv_.notify_all();

    // Optionally block until everything is flushed
    bool done = false;
    while (!done)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (message_queue_.empty() && !force_flush_)
        {
            done = true;
        }
    }
}

void LoggerImpl::worker_thread_function()
{
    while (is_running_)
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        queue_cv_.wait(lock, [this] {
            return !message_queue_.empty() || !is_running_ || force_flush_;
        });

        while (!message_queue_.empty())
        {
            auto msg = std::move(message_queue_.front());
            message_queue_.pop();
            lock.unlock();

            {
                std::unique_lock<std::mutex> rotation_lock(rotation_mutex_);
                std::lock_guard<std::mutex> file_lock(file_mutex_);

                file_ << msg.timestamp << " | "
                      << msg.class_name << " | "
                      << severity_to_string(msg.severity) << " | "
                      << msg.message << std::endl;

                // Update timestamps
                if (first_timestamp_.empty())
                {
                    first_timestamp_ = msg.timestamp;
                }
                last_timestamp_ = msg.timestamp;

                // Check size
                if (std::filesystem::file_size(file_path_) >= max_file_size_)
                {
                    rotate_file();
                }
            }

            lock.lock();
        }

        // If force_flush_ is set, flush the file
        if (force_flush_)
        {
            {
                std::lock_guard<std::mutex> file_lock(file_mutex_);
                file_.flush();
            }
            force_flush_ = false;
        }
    }
}

void LoggerImpl::stop_worker()
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

    // Final flush & close
    if (file_.is_open())
    {
        file_.flush();
        file_.close();
    }
}

void LoggerImpl::rotate_file()
{
    file_.close();
    // E.g. logs/history/logfile-2025_02_06-10_05_02.123456-2025_02_06-10_30_00.654321.log
    auto history_dir = file_path_.parent_path() / "history";
    if (!std::filesystem::exists(history_dir))
    {
        std::filesystem::create_directories(history_dir);
    }
    auto rotated_name = file_path_.stem().string() + "-"
                        + first_timestamp_ + "-"
                        + last_timestamp_
                        + file_path_.extension().string();
    auto rotated_path = history_dir / rotated_name;
    std::filesystem::rename(file_path_, rotated_path);

    first_timestamp_.clear();
    last_timestamp_.clear();

    // Reopen fresh
    file_.open(file_path_, std::ios::trunc);
    if (!file_.is_open())
    {
        throw std::runtime_error("LoggerImpl::rotate_file: Cannot reopen file " + file_path_.string());
    }
}

std::string LoggerImpl::severity_to_string(SEVERITY sev)
{
    switch (sev)
    {
        case SEVERITY::INFO: return "INFO";
        case SEVERITY::DEB:  return "DEBUG";
        case SEVERITY::WARN: return "WARNING";
        case SEVERITY::ERR:  return "ERROR";
        default:             return "UNKNOWN";
    }
}

std::string LoggerImpl::get_time_stamp()
{
    using clock = std::chrono::system_clock;

    auto now = clock::now();
    auto t_c = clock::to_time_t(now);
    auto micros = std::chrono::duration_cast<std::chrono::microseconds>(
                      now.time_since_epoch()).count() % 1000000;

    std::tm local_tm{};
#if defined(_WIN32) || defined(_MSC_VER)
    localtime_s(&local_tm, &t_c);
#else
    localtime_r(&t_c, &local_tm);
#endif

    std::ostringstream oss;
    oss << std::put_time(&local_tm, "%Y_%m_%d-%H_%M_%S") << "."
        << std::setw(6) << std::setfill('0') << micros;
    return oss.str();
}

} // namespace detail
} // namespace stdx
