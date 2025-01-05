#ifndef E1042EF3_758D_43B8_931C_46321B09EFD8
#define E1042EF3_758D_43B8_931C_46321B09EFD8

#include <string>
#include <fstream>
#include <mutex>
#include <unordered_map>
#include <memory>
#include <filesystem>
#include <queue>
#include <condition_variable>

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

namespace stdx
{

  enum class Severity
  {
    INFO,
    DEBUG,
    WARNING,
    ERROR
  };

  class STDX_LOGGER_API Logger
  {
    struct LogMessage
    {
      Severity severity;
      std::string class_name;
      std::string message;
      std::string timestamp;
    };

  public:
    static void initialize(const std::string &name, const std::filesystem::path &file_path);
    static Logger &get_instance(const std::string &name);

    void log(Severity severity, const std::string &class_name, const std::string &message);

    ~Logger();

    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

  private:
    explicit Logger(const std::filesystem::path &file_path);
    void worker_thread_function();
    void stop_worker();

    mutable std::ofstream file_;
    mutable std::mutex file_mutex_;
    mutable std::queue<LogMessage> message_queue_;
    mutable std::mutex queue_mutex_;
    mutable std::condition_variable queue_cv_;
    std::atomic<bool> is_running_;
    std::thread worker_thread_;

    static std::unordered_map<std::string, std::unique_ptr<Logger>> loggers_;
    static std::mutex map_mutex_;

    static std::string severity_to_string(Severity severity);
    static std::string get_time_stamp();
  };
} // namespace stdx

#endif /* E1042EF3_758D_43B8_931C_46321B09EFD8 */
