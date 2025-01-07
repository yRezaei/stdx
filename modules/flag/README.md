### Flag Overview

The **`stdx::Flag`** is a powerful C++ utility class designed to simplify and enhance the management of bitmask flags using `enum class`. It provides a type-safe and feature-rich interface for operations such as combining, adding, removing, and checking flags. The class is part of the `stdx` library, making it easy to integrate and leverage alongside other utilities.

---

### Key Features

1. **Type-Safe Operations**:
   - Ensures that only valid enumerations can be used with `Flag`, avoiding common pitfalls of raw bitmask operations.

2. **Support for Enum Classes**:
   - Requires the use of `enum class`, providing better scope and type safety.

3. **Comprehensive Bitmask Operations**:
   - Easily combine, add, remove, and check flags using intuitive member functions and operator overloads.

4. **Automatic Validation**:
   - Validates that numeric values or bit combinations are valid for the provided enum type.

5. **Customizable Behavior**:
   - Supports user-defined enumerators, including a mandatory `All` enumerator to define valid bits.

6. **Integration with `stdx::utils`**:
   - Leverages utilities for flag combination and validation to provide out-of-the-box functionality.

---

### How to Use the Flag Class

#### **Integration in CMake**

To use the `Flag` class in your project, include it in your `CMakeLists.txt`:

```cmake
find_package(flag REQUIRED)
target_link_libraries(YOUR_TARGET PUBLIC stdx::flag)
```

#### **Integration in Conan**

Add the flag module to your `conanfile.txt` or `conanfile.py`:

**In `conanfile.txt`:**
```plaintext
[requires]
stdx/VERSION_NUMBER

[options]
stdx/*:enable_flag=True
```

**In `conanfile.py`:**
```python
requires = "stdx/VERSION_NUMBER"
options = {"stdx/*:enable_flag": True}
```

---

### Example Usage

#### **Define an Enum with an `All` Enumerator**

The `Flag` class requires that your enum type defines an `All` enumerator that represents all valid bits:

```cpp
enum class MyFlags : std::uint8_t {
    None  = 0x00,
    Flag1 = 0x01,
    Flag2 = 0x02,
    Flag3 = 0x04,
    All   = Flag1 | Flag2 | Flag3
};
```

#### **Basic Operations**

**Create Flags**:
```cpp
#include <stdx/flag.hpp>
#include <iostream>

using stdx::Flag;

Flag<MyFlags> my_flag(MyFlags::Flag1); // Initialize with a single flag
std::cout << my_flag.get() << std::endl; // Output: 1
```

**Combine Flags**:
```cpp
Flag<MyFlags> combined_flag(MyFlags::Flag1, MyFlags::Flag2); // Combine multiple flags
std::cout << combined_flag.get() << std::endl; // Output: 3
```

**Add and Remove Flags**:
```cpp
combined_flag.add(MyFlags::Flag3); // Add a flag
combined_flag.remove(MyFlags::Flag1); // Remove a flag
std::cout << combined_flag.get() << std::endl; // Output: 6
```

**Check Flags**:
```cpp
if (combined_flag.has(MyFlags::Flag2)) {
    std::cout << "Flag2 is set" << std::endl; // Output: Flag2 is set
}
```

---

### Advanced Operations

**Bitwise Operators**:
- Perform operations like `|`, `&`, and `~` directly on `Flag` objects.
```cpp
Flag<MyFlags> flag1(MyFlags::Flag1);
Flag<MyFlags> flag2(MyFlags::Flag2);

auto result = flag1 | MyFlags::Flag2; // Combine using bitwise OR
std::cout << result.get() << std::endl; // Output: 3

result &= MyFlags::Flag1; // Keep only Flag1 using bitwise AND
std::cout << result.get() << std::endl; // Output: 1

result = ~result; // Invert valid bits
std::cout << result.get() << std::endl; // Output: 6
```

**Validate Numeric Values**:
- Create a `Flag` from a numeric value with automatic validation.
```cpp
Flag<MyFlags> numeric_flag(3); // Valid numeric value
std::cout << numeric_flag.get() << std::endl; // Output: 3

// Throws std::invalid_argument for invalid values
try {
    Flag<MyFlags> invalid_flag(8); // 8 is not a valid combination
} catch (const std::invalid_argument& e) {
    std::cerr << e.what() << std::endl; // Output: Numeric value does not represent a valid combination of enum flags.
}
```

---

### Comprehensive Unit Tests

The `Flag` class is thoroughly tested to ensure reliability and robustness. The following test cases validate its functionality:

1. **Flag Creation**:
   - Verify construction from a single enum value or multiple flags.

2. **Add, Remove, and Check Flags**:
   - Ensure flags can be dynamically modified and queried.

3. **Bitwise Operators**:
   - Validate results for `|`, `&`, and `~`.

4. **Validation**:
   - Confirm invalid combinations are rejected with exceptions.

---

### Practical Use Cases

1. **Access Control**:
   - Manage permissions with bitmask enums, e.g., `READ`, `WRITE`, `EXECUTE`.

2. **State Management**:
   - Track states of objects or systems with multiple boolean attributes.

3. **Feature Toggles**:
   - Enable or disable features dynamically using bitmask flags.

---

The `stdx::Flag` class combines the flexibility of bitmask operations with the safety and convenience of modern C++. It is an excellent tool for managing flags in a robust, type-safe, and efficient manner.