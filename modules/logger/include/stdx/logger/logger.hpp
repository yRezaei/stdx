#ifndef STDX_LOGGER_HPP
#define STDX_LOGGER_HPP

#include <string>
#include <filesystem>
#include <functional>
#include <cstdint>

#if defined(STDX_INCLUDE_EXPORT)
#include "stdx/stdx_export.hpp"
#else
#  define STDX_API
#endif

namespace stdx
{
    namespace detail{
        class LoggerImpl;
    }
    
    enum class SEVERITY : std::uint8_t
    {
        INFO,
        DEB,
        WARN,
        ERR
    };

    class STDX_API Logger
    {
    public:
        // Log a message with given severity
        void log(SEVERITY severity, const std::string &message);

        // Force a flush (blocks until the file is flushed)
        void flush();

    private:
        friend class LogManager;
        // Private constructor, only accessible by LogManager
        Logger(const std::string &class_name, detail::LoggerImpl *impl)
            : class_name_(class_name), impl_(impl)
        {
        }

    private:
        // The "class" or "module" name, e.g. "MyClass", used in log lines
        std::string class_name_;
        detail::LoggerImpl *impl_;
    };

} // namespace stdx

#endif // STDX_LOGGER_HPP
