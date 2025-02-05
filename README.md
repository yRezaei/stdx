### stdx: A Modular C++ Library

The **`stdx` library** is a versatile collection of C++ modules designed to enhance your development experience. It provides reusable, well-tested, and high-performance components such as a **Logger** for asynchronous logging and a **Flag** utility for managing bitmask enumerations.

This library is built with **CMake** as the primary build system and supports integration with **Conan 2**, enabling seamless dependency management and modular configuration.

---

### Features

1. **Modular Design**:
   - Each module (`logger`, `flag`, etc.) is self-contained and can be enabled or disabled individually.
   
2. **High Performance**:
   - Designed for multithreaded environments, ensuring robust and efficient operations.

3. **C++ Standards Compliance**:
   - Requires **C++17** or higher.

4. **Cross-Platform**:
   - Fully tested on major operating systems, including Windows, Linux, and macOS.

5. **Seamless Integration**:
   - Easily integrate with your project using **CMake** and **Conan**.

6. **Customizable Builds**:
   - Configure shared/static builds, module inclusion, and build types (`Debug`, `Release`, etc.).

---

### Supported Modules

| Module   | Description                              | Key Features                                                                                       |
|----------|------------------------------------------|---------------------------------------------------------------------------------------------------|
| [**flag**](https://github.com/yRezaei/stdx/blob/main/modules/flag/README.md) | A utility for managing bitmask enums.    | Type-safe flag manipulation, bitwise operations, validation, and customizable behavior.          |
| [**logger**](https://github.com/yRezaei/stdx/blob/main/modules/flag/README.md) | An asynchronous logging utility.         | Buffered logging, file rotation, custom log formatting, and support for multithreaded applications. |

---

### Building the Library

#### **Using CMake**

1. Clone the repository:
   ```bash
   git clone https://github.com/yrezaei/stdx.git
   cd stdx
   ```

2. Configure and build:
   ```bash
   mkdir build && cd build
   cmake .. -DCMAKE_BUILD_TYPE=Release -DSTDX_ENABLE_FLAG=ON -DSTDX_ENABLE_LOGGER=ON
   cmake --build .
   ```

3. Install the library (optional):
   ```bash
   cmake --install . --prefix /your/installation/path
   ```

#### **Using Conan**

1. Create a `conanfile.txt` or `conanfile.py` and specify `stdx` as a dependency.

**`conanfile.txt`:**
```plaintext
[requires]
stdx/VERSION_NUMBER

[options]
stdx/*:enable_flag=True
stdx/*:enable_logger=True
```

2. Install dependencies and build:
   ```bash
   conan install . --output-folder=build --build=missing
   cd build
   cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake
   cmake --build .
   ```

---

### Consuming the Library

#### **Using CMake**

Include the desired modules in your `CMakeLists.txt`:

```cmake
# Find and link the flag module
find_package(stdx REQUIRED)
target_link_libraries(YOUR_TARGET PUBLIC stdx::stdx)
```

#### **Using Conan**

Add `stdx` to your `conanfile.txt` or `conanfile.py` as shown earlier. Use the `find_package` mechanism in CMake to locate and link the modules.

### Customization and Options

| **CMake Option**       | **Description**                                | **Default** |
|-------------------------|-----------------------------------------------|-------------|
| `STDX_ENABLE_FLAG`      | Enable the `flag` module.                     | `ON`        |
| `STDX_ENABLE_LOGGER`    | Enable the `logger` module.                   | `ON`        |
| `BUILD_SHARED_LIBS`     | Build shared libraries instead of static.     | `OFF`       |

| **Conan Option**        | **Description**                                | **Default** |
|-------------------------|-----------------------------------------------|-------------|
| `stdx/*:enable_flag`    | Enable the `flag` module.                     | `True`      |
| `stdx/*:enable_logger`  | Enable the `logger` module.                   | `True`      |

---

### Contributing

1. Fork the repository.
2. Create a feature branch.
3. Submit a pull request with your changes.

---

### License

The `stdx` library is licensed under the [MIT License](LICENSE). Feel free to use, modify, and distribute it as per the terms of the license.

---

This README provides an overview of the `stdx` library and instructions for building, consuming, and using its modules effectively. Let me know if you'd like to include additional details or examples! 🚀
