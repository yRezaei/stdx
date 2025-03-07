#include "stdx/logger/log_manager.hpp"
#include "stdx/logger/impl/logger_impl.hpp"
#include <stdexcept>

namespace stdx
{

std::unique_ptr<detail::LoggerImpl> LogManager::impl_ = nullptr;
std::filesystem::path LogManager::file_path_;
std::atomic<bool> LogManager::initialized_{false};
std::mutex LogManager::init_mutex_;

void LogManager::initialize(const std::filesystem::path& file_path,
                            std::size_t max_file_size,
                            std::size_t max_backup_files,
                            std::size_t batch_size,
                            std::chrono::seconds flush_interval,
                            RotationStrategy custom_strategy)
{
    std::lock_guard<std::mutex> lock(init_mutex_);
    if (initialized_)
    {
        throw std::runtime_error("LogManager::initialize: Already initialized.");
    }

    impl_ = std::make_unique<detail::LoggerImpl>(file_path,
                                         max_file_size,
                                         max_backup_files,
                                         batch_size,
                                         flush_interval,
                                         std::move(custom_strategy));
    file_path_ = file_path;
    initialized_ = true;
}

bool LogManager::is_initialized()
{
    return initialized_.load();
}

Logger LogManager::create_logger(const std::string &class_name)
    {
        // If not initialized, either auto-initialize with defaults or throw
        if (!is_initialized())
        {
            throw std::runtime_error("LogManager not initialized before create_logger().");
        }
        // Return a Logger handle that references our single global impl
        return Logger{class_name, get_impl()};
    }

detail::LoggerImpl* LogManager::get_impl()
{
    if (!initialized_)
    {
        throw std::runtime_error("LogManager::get_impl: Not initialized yet.");
    }
    return impl_.get();
}

const std::filesystem::path& LogManager::get_file_path()
{
    return file_path_;
}

void LogManager::shutdown()
    {
        std::lock_guard<std::mutex> lock(init_mutex_);
        if (!initialized_)
        {
            return; // Already shut down or never initialized
        }

        if (impl_)
        {
            // Optionally flush right before destroying (in case user hasn't already).
            impl_->flush();
            // Destroying the LoggerImpl will stop the thread (via its destructor).
            impl_.reset();
        }

        initialized_ = false;
    }

} // namespace stdx
