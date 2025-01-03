# stdx

This repository contains a **C++17** library named **stdx**, offering a collection of useful components for both simple and advanced use cases. The codebase emphasizes modern C++ practices, modular organization, and flexible build options (static or shared libraries, optional components, etc.). It can be built **with** or **without** [Conan 2](https://docs.conan.io/en/latest/), enabling easy dependency management and packaging.

---

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Building **with Conan 2**](#building-with-conan-2)
   - [Configuring Build Options with Conan](#configuring-build-options-with-conan)
   - [Build and Test Steps](#build-and-test-steps)
3. [Building **without Conan**](#building-without-conan)
   - [Configuring Build Options (CMake)](#configuring-build-options-cmake)
   - [Build and Test Steps](#build-and-test-steps-1)
4. [Modules Included So Far](#modules-included-so-far)
5. [Installation](#installation)
6. [License](#license)

---

## Prerequisites

- **CMake** 3.15 or higher
- A **C++17**-capable compiler (GCC 7+, Clang 7+, MSVC 2019+)
- (Optional) **Conan 2** if you plan to manage dependencies via Conan

---

## Building **with Conan 2**

### 1. Install Conan Dependencies

From the project’s root directory, create or reuse a **build** folder and run:

```bash
conan install . -of=build --build=missing
```

This will:
- Read the [`conanfile.py`](./conanfile.py).
- Install any declared dependencies (if any).
- Generate the **CMakeToolchain** and **CMakeDeps** files into `build/`.

### 2. Configuring Build Options with Conan

The `conanfile.py` exposes several options:

- **`shared`**: `True` or `False` (default `False`)  
- **`enable_flag`**: `True` or `False` (default `True`)  
- **`enable_logger`**: `True` or `False` (default `True`)  

You can override these at install time. For example:

```bash
# Build shared libraries, disable the logger module
conan install . -of=build --build=missing \
    -o stdx:shared=True \
    -o stdx:enable_logger=False
```

### 3. Build and Test Steps

Then, from within the `build/` folder:

1. **Configure**:
   ```bash
   cmake .. -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake
   ```
2. **Build**:
   ```bash
   cmake --build . --config Release
   ```
3. **Test** (via CTest):
   ```bash
   ctest -C Release --output-on-failure
   ```

---

## Building **without Conan**

If you don’t want to use Conan, you can still build **stdx** using only CMake. This is straightforward because `stdx` currently has no external dependencies.

### 1. Configuring Build Options (CMake)

The top-level [`CMakeLists.txt`](./CMakeLists.txt) declares these options:

- **`STDX_BUILD_SHARED`**: `ON`/`OFF` (default `OFF`)  
- **`STDX_ENABLE_FLAG`**: `ON`/`OFF` (default `ON`)  
- **`STDX_ENABLE_LOGGER`**: `ON`/`OFF` (default `ON`)  

Example usage:

```bash
cmake -S . -B build \
      -DCMAKE_BUILD_TYPE=Release \
      -DSTDX_BUILD_SHARED=ON \
      -DSTDX_ENABLE_LOGGER=OFF
```

### 2. Build and Test Steps

From the project root:

1. **Generate**:
   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
   ```
2. **Build**:
   ```bash
   cmake --build build --config Debug
   ```
3. **Test**:
   ```bash
   cd build
   ctest --output-on-failure
   ```

---

## Modules Included So Far

- **`flag` (header-only)**: Provides an easy way to handle feature or configuration flags in your code.  
  - Located at `include/stdx/flag.hpp`.

- **`logger` (compiled)**: Offers simple logging functionality for typical application needs.  
  - Header at `include/stdx/logger.hpp` and implementation in `modules/logger/src/logger.cxx`.

- **Shared Header: `utils.hpp`**: Common utility functions or definitions used across modules.  
  - Located at `include/stdx/utils.hpp`.

Additional modules can be added in the future, each optionally built or omitted depending on your needs.

---

## Installation

After a successful build, you can install **stdx** (headers and libraries) to a local folder (or system directory):

```bash
cmake --install build --config <Debug|Release> --prefix /path/to/install
```

This copies:
- Headers into `include/stdx/`
- Libraries (static or shared) into `lib/`
- Binaries (e.g., test executables, if installed) into `bin/`

---

## License

This project is available under the [MIT License](LICENSE).  
Feel free to use, modify, and distribute as allowed by the license terms.