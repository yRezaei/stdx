#include "stdx/logger.hpp"
#include <iomanip>
#include <sstream>
#include <chrono>
#include <ctime>
#include <stdexcept>

namespace stdx {

std::unordered_map<std::string, std::unique_ptr<Logger>> Logger::loggers_;
std::mutex Logger::map_mutex_;

void Logger::initialize(const std::string& name, const std::filesystem::path& file_path) {
    std::lock_guard<std::mutex> lock(map_mutex_); // Thread-safe map access

    if (loggers_.find(name) != loggers_.end()) {
        throw std::runtime_error("Logger with name '" + name + "' already exists.");
    }

    if (!std::filesystem::exists(file_path.parent_path())) {
        std::filesystem::create_directories(file_path.parent_path());
    }

    loggers_[name] = std::unique_ptr<Logger>(new Logger(file_path));
}

const Logger& Logger::get_instance(const std::string& name) {
    std::lock_guard<std::mutex> lock(map_mutex_); // Thread-safe map access

    auto it = loggers_.find(name);
    if (it == loggers_.end()) {
        throw std::runtime_error("Logger with name '" + name + "' not initialized.");
    }

    return *(it->second);
}

Logger::Logger(const std::filesystem::path& file_path)
    : file_(file_path, std::ios::app) {
    if (!file_.is_open()) {
        throw std::runtime_error("Failed to open log file: " + file_path.string());
    }
}

void Logger::log(Severity severity, const std::string& class_name, const std::string& message) const {
    std::lock_guard<std::mutex> lock(file_mutex_); // Thread-safe file access

    file_ << get_time_stamp() << " | "
          << std::setw(10) << class_name << " | "
          << std::setw(8) << severity_to_string(severity) << " | "
          << message << std::endl;
}

std::string Logger::severity_to_string(Severity severity) {
    switch (severity) {
        case Severity::INFO: return "INFO";
        case Severity::DEBUG: return "DEBUG";
        case Severity::WARNING: return "WARNING";
        case Severity::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

std::string Logger::get_time_stamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm local_time;

    // Use localtime_s for thread safety
    if (localtime_s(&local_time, &time_t_now) != 0) {
        throw std::runtime_error("Failed to get local time");
    }

    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count() % 1000;

    std::ostringstream oss;
    oss << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S") << "."
        << std::setw(3) << std::setfill('0') << milliseconds;
    return oss.str();
}

} // namespace stdx
