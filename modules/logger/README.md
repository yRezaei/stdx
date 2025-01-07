### Logger Overview

The **`stdx::Logger`** is a versatile, high-performance logging library designed for multithreaded applications. It provides asynchronous logging with advanced features such as log file rotation, buffered writing, and user-configurable formatting. The logger is part of the `stdx` library and can be seamlessly integrated into any C++ project.

---

### Key Features

1. **Asynchronous Logging**:
   - Offloads log writing to a dedicated worker thread to minimize impact on application performance.

2. **Buffered Writing**:
   - Logs are buffered and written to the file in batches, either when a message threshold is reached (default: 10 messages) or after a time threshold (default: 5 seconds).

3. **File Rotation**:
   - Automatically rotates log files when a size threshold is exceeded.
   - Rotated files are saved in a `logs/history` directory, with names that include timestamps and optional sequence numbers to ensure uniqueness.

4. **Customizable Log Format**:
   - Logs include a timestamp, severity, class/component name, and the message.
   - Example:
     ```
     2025_01_07-18_14_17.123456 | ComponentA |    INFO | This is an informational message.
     ```

5. **User Configurability**:
   - Options such as log file size, rotation policies, and log formats can be customized.
   - Supports user-defined log rotation strategies for advanced use cases.

6. **Thread Safety**:
   - All operations, including log writing and file rotation, are fully synchronized to ensure data integrity in multithreaded environments.

7. **Integration with Conan**:
   - Can be easily included in projects using Conan, with options to enable or disable the logger module.

---

### How to Use the Logger

#### **Integration in CMake**

To use the logger in your project, include it in your `CMakeLists.txt`:

```cmake
find_package(logger REQUIRED)
target_link_libraries(YOUR_TARGET PUBLIC stdx::logger)
```

#### **Integration in Conan**

Add the logger to your `conanfile.txt` or `conanfile.py`:

**In `conanfile.txt`:**
```plaintext
[requires]
stdx/VERSION_NUMBER

[options]
stdx/*:enable_logger=True
```

**In `conanfile.py`:**
```python
requires = "stdx/VERSION_NUMBER"
options = {"stdx/*:enable_logger": True}
```

---

### Example Usage

#### **Initialization**
The logger must be initialized before use. Provide a unique name and the file path where logs should be written:
```cpp
#include <stdx/logger.hpp>

stdx::Logger::initialize("app_logger", "logs/app.log");
```

#### **Logging Messages**
Retrieve the logger instance by name and use it to log messages:
```cpp
auto& logger = stdx::Logger::get_instance("app_logger");

// Log messages with different severities
logger.log(stdx::Severity::INFO, "ComponentA", "This is an informational message.");
logger.log(stdx::Severity::DEBUG, "ComponentB", "Debugging details here.");
logger.log(stdx::Severity::ERROR, "ComponentA", "An error occurred!");
```

#### **File Rotation and Buffering**
- Logs are written asynchronously and buffered.
- When the file size exceeds the configured limit (default: 10MB), the logger automatically rotates the file:
  - The current file is moved to `logs/history`, and a new file is created.
  - File names include timestamps to distinguish rotated files:
    ```
    logs/history/app-2025_01_07-18_14_17.123456-2025_01_07-18_14_18.456789.log
    ```

#### **Shutting Down the Logger**
Ensure that all log messages are flushed before shutting down:
```cpp
stdx::Logger::shutdown("app_logger");
```

#### **Custom Rotation Strategy**
For advanced use cases, you can define a custom log rotation strategy:
```cpp
stdx::Logger::initialize("custom_logger", "logs/custom.log", 1024 * 1024 /* 1MB */, 3 /* max backups */, [](auto file_path, auto& stream) {
    // User-defined rotation logic
    std::cerr << "Custom rotation strategy applied for: " << file_path << std::endl;
});
```

---

### Advanced Configuration Options

- **Log File Size**:
  - Configure the maximum file size before rotation:
    ```cpp
    stdx::Logger::initialize("app_logger", "logs/app.log", 5 * 1024 * 1024 /* 5MB */);
    ```

- **Buffered Writing**:
  - By default, logs are written to the file after 10 messages or 5 seconds.
  - This behavior improves performance for high-frequency logging.

- **Multiple Loggers**:
  - Initialize separate loggers for different components:
    ```cpp
    stdx::Logger::initialize("error_logger", "logs/error.log");
    stdx::Logger::initialize("debug_logger", "logs/debug.log");
    ```

---

### Practical Use Cases

1. **Application Logging**:
   - Centralize logs for debugging, performance monitoring, and issue tracking.
2. **Multithreaded Applications**:
   - Asynchronous logging ensures minimal interference with application performance.
3. **Log Archival**:
   - Automatically manage log sizes and history using rotation policies.

---

With its asynchronous design and powerful features, the `stdx::Logger` is an excellent choice for managing logs in C++ applications, ensuring clarity and performance even under high-load scenarios.