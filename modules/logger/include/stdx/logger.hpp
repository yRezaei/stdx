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
#include <functional>
#include <vector>

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
        using RotationStrategy = std::function<void(const std::filesystem::path &, std::ofstream &)>;

        static void initialize(const std::string &name, const std::filesystem::path &file_path,
                               size_t max_file_size = 10 * 1024 * 1024, // 10 MB
                               size_t max_backup_files = 5,
                               RotationStrategy custom_strategy = nullptr);

        static Logger &get_instance(const std::string &name);
        static void shutdown(const std::string &name);
        void log(Severity severity, const std::string &class_name, const std::string &message);
        void flush();

        ~Logger();

        Logger(const Logger &) = delete;
        Logger &operator=(const Logger &) = delete;

    private:
        explicit Logger(const std::filesystem::path &file_path, size_t max_file_size,
                        size_t max_backup_files, RotationStrategy custom_strategy);

        void worker_thread_function();
        void rotate_file();
        void stop_worker();

        std::filesystem::path file_path_;
         std::ofstream file_;
         std::mutex file_mutex_;
         std::queue<LogMessage> message_queue_;
         std::mutex queue_mutex_;
         std::condition_variable queue_cv_;
        std::mutex rotation_mutex_;
        std::atomic<bool> force_flush_{false};

        std::vector<LogMessage> buffer_;
        size_t max_file_size_;
        size_t max_backup_files_;
        RotationStrategy custom_rotation_strategy_;
        std::atomic<bool> is_running_;
        std::thread worker_thread_;
        std::chrono::steady_clock::time_point last_flush_time_;
        std::string first_timestamp_;
        std::string last_timestamp_;
        static constexpr size_t flush_threshold_ = 10;
        static constexpr std::chrono::seconds time_threshold_ = std::chrono::seconds(5);

        static std::unordered_map<std::string, std::unique_ptr<Logger>> loggers_;
        static std::mutex map_mutex_;

        static struct LoggerGlobalInitializer
        {
            LoggerGlobalInitializer()
            {
                std::atexit([]
                            { Logger::shutdown(""); }); // Shutdown all loggers on program exit
            }
        } loggerGlobalInitializer;

        static std::string severity_to_string(Severity severity);
        static std::string get_time_stamp();
        static void clear_logger(const std::string &name);
    };

} // namespace stdx

#endif /* E1042EF3_758D_43B8_931C_46321B09EFD8 */
