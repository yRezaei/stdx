#include "stdx/logger/logger.hpp"
#include "stdx/logger/log_manager.hpp"
#include "stdx/logger/impl/logger_impl.hpp"

#include <stdexcept>

namespace stdx
{
    void Logger::log(SEVERITY severity, const std::string &message)
    {
        if (impl_)
        {
            impl_->log(severity, class_name_, message);
        }
        else
        {
            if (LogManager::is_initialized())
            {
                throw std::runtime_error("Logger: LogManager initialized, but no implementation available!!!");
            }
            else
            {
                throw std::runtime_error("Logger: have you forgotten to initialize LogManager?");
            }
        }
    }

    void Logger::flush()
    {
        if (impl_)
        {
            impl_->flush();
        }
    }

} // namespace stdx
