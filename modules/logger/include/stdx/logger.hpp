#ifndef E1042EF3_758D_43B8_931C_46321B09EFD8
#define E1042EF3_758D_43B8_931C_46321B09EFD8

#include <string>
#include <fstream>
#include <mutex>
#include <unordered_map>
#include <memory>
#include <filesystem>

#ifdef _WIN32
  #if defined(STDX_SHARED)
    #ifdef BUILDING_STDX_LOGGER
      #define STDX_LOGGER_API __declspec(dllexport)
    #else
      #define STDX_LOGGER_API __declspec(dllimport)
    #endif
  #elif defined(STDX_STATIC)
    #define STDX_LOGGER_API
  #else
    #error "Either STDX_SHARED or STDX_STATIC must be defined."
  #endif
#elif defined(__GNUC__) || defined(__clang__)
  #if defined(STDX_SHARED)
    #ifdef BUILDING_STDX_LOGGER
      #define STDX_LOGGER_API __attribute__((visibility("default")))
    #else
      #define STDX_LOGGER_API
    #endif
  #elif defined(STDX_STATIC)
    #define STDX_LOGGER_API
  #else
    #error "Either STDX_SHARED or STDX_STATIC must be defined."
  #endif
#else
  // Fallback for other platforms (no visibility attributes)
  #define STDX_LOGGER_API
#endif

namespace stdx {

enum class Severity {
    INFO,
    DEBUG,
    WARNING,
    ERROR
};

class STDX_LOGGER_API Logger {
public:
    // Initialize a logger with a unique name and file path
    static void initialize(const std::string& name, const std::filesystem::path& file_path);

    // Get the logger instance by name
    static const Logger& get_instance(const std::string& name);

    // Log a message with the given severity and class name
    void log(Severity severity, const std::string& class_name, const std::string& message) const;

    // Deleted methods to prevent copying and assignment
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    explicit Logger(const std::filesystem::path& file_path);

    mutable std::ofstream file_;
    mutable std::mutex file_mutex_;

    static std::unordered_map<std::string, std::unique_ptr<Logger>> loggers_;
    static std::mutex map_mutex_;

    static std::string severity_to_string(Severity severity);
    static std::string get_time_stamp();
};

} // namespace stdx

#endif /* E1042EF3_758D_43B8_931C_46321B09EFD8 */
