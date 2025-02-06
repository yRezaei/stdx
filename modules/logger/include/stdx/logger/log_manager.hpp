#ifndef STDX_IMPL_LOG_MANAGER_HPP
#define STDX_IMPL_LOG_MANAGER_HPP

#include <filesystem>
#include <memory>
#include <mutex>
#include <atomic>
#include <functional>
#include "stdx/logger/logger.hpp"

namespace stdx
{

    using RotationStrategy = std::function<void(const std::filesystem::path &, std::ofstream &)>;

    namespace detail
    {
        class LoggerImpl;
    }

    // Private class (not part of the public API):
    // A single "manager" that owns one LoggerImpl instance and mediates its lifecycle.
    class STDX_API LogManager
    {
    public:
        static void initialize(const std::filesystem::path &file_path,
                               size_t max_file_size = 10 * 1024 * 1024, // 10 MB
                               size_t max_backup_files = 5,
                               RotationStrategy custom_strategy = nullptr);

        static bool is_initialized();

        // Create a Logger handle for a given class/module name
        // (returns by value, so user can store it however they want).
        static Logger create_logger(const std::string &class_name);

        // Optionally let us retrieve the path used, to check for conflicts
        static const std::filesystem::path &get_file_path();

        static void shutdown();

    private:
        // Disallow instantiation
        LogManager() = delete;

        // Internal accessor to the single LoggerImpl
        static detail::LoggerImpl *get_impl();

    private:
        // Once created, these remain valid until the user kills the program
        // or we decide to allow a "shutdown" method.
        static std::unique_ptr<detail::LoggerImpl> impl_;
        static std::filesystem::path file_path_;
        static std::atomic<bool> initialized_;
        static std::mutex init_mutex_;
    };

} // namespace stdx

#endif // STDX_IMPL_LOG_MANAGER_HPP
