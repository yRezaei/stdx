#include <iostream>
#include <stdexcept>
#include <string>
#include <cstdint>

// Include your headers
#include <stdx/utils.hpp>
#include <stdx/flag.hpp>

// Example enum for testing
enum class MyFlags : std::uint8_t
{
    None  = 0x00,
    Flag1 = 0x01,
    Flag2 = 0x02,
    Flag3 = 0x04,
    All   = Flag1 | Flag2 | Flag3
};

// Global counter to track test failures
static int g_testFailures = 0;

// Helper to compare equality with descriptive output
template<typename T>
void checkEqual(const T& actual, const T& expected, const std::string& testName)
{
    if (actual != expected)
    {
        std::cerr << "TEST FAILED: " << testName << "\n"
                  << "  Expected: " << expected << "\n"
                  << "  Got:      " << actual << "\n";
        g_testFailures++;
    }
}

// Helper to check a boolean is true
void checkTrue(bool condition, const std::string& testName)
{
    if (!condition)
    {
        std::cerr << "TEST FAILED: " << testName << " -> expected TRUE, got FALSE\n";
        g_testFailures++;
    }
}

// Helper to check a boolean is false
void checkFalse(bool condition, const std::string& testName)
{
    if (condition)
    {
        std::cerr << "TEST FAILED: " << testName << " -> expected FALSE, got TRUE\n";
        g_testFailures++;
    }
}

int main()
{
    using stdx::utils::is_valid_combination;
    using stdx::utils::combine_flags;
    using stdx::Flag;

    // -------------------------------------------------------------------------
    // 1) Test utils.hpp
    // -------------------------------------------------------------------------
    {
        // is_valid_combination
        checkTrue(is_valid_combination<MyFlags>(0),  "utils.is_valid_combination(0)");
        checkTrue(is_valid_combination<MyFlags>(1),  "utils.is_valid_combination(1)");  // Flag1
        checkTrue(is_valid_combination<MyFlags>(2),  "utils.is_valid_combination(2)");  // Flag2
        checkTrue(is_valid_combination<MyFlags>(4),  "utils.is_valid_combination(4)");  // Flag3
        checkTrue(is_valid_combination<MyFlags>(3),  "utils.is_valid_combination(3)");  // Flag1 | Flag2
        checkTrue(is_valid_combination<MyFlags>(5),  "utils.is_valid_combination(5)");  // Flag1 | Flag3
        checkTrue(is_valid_combination<MyFlags>(6),  "utils.is_valid_combination(6)");  // Flag2 | Flag3
        checkTrue(is_valid_combination<MyFlags>(7),  "utils.is_valid_combination(7)");  // Flag1 | Flag2 | Flag3
        checkFalse(is_valid_combination<MyFlags>(8), "utils.is_valid_combination(8)");  // Out-of-range bit

        // combine_flags
        {
            auto combined1 = combine_flags(MyFlags::Flag1);
            checkEqual(combined1, static_cast<std::uint8_t>(0x01), "combine_flags(MyFlags::Flag1)");

            auto combined2 = combine_flags(MyFlags::Flag1, MyFlags::Flag2);
            checkEqual(combined2, static_cast<std::uint8_t>(0x01 | 0x02), "combine_flags(Flag1, Flag2)");

            auto combined3 = combine_flags(MyFlags::Flag1, MyFlags::Flag2, MyFlags::Flag3);
            checkEqual(combined3, static_cast<std::uint8_t>(0x01 | 0x02 | 0x04), "combine_flags(Flag1, Flag2, Flag3)");
        }
    }

    // -------------------------------------------------------------------------
    // 2) Test flag.hpp (Flag class)
    // -------------------------------------------------------------------------
    {
        // (a) Default constructor
        {
            Flag<MyFlags> f_default;
            checkEqual(f_default.get(), static_cast<std::uint8_t>(0), "Flag<MyFlags> default constructor");
        }

        // (b) Constructor from a single enum value
        {
            Flag<MyFlags> f_single(MyFlags::Flag1);
            checkEqual(f_single.get(), static_cast<std::uint8_t>(0x01), "Flag<MyFlags>(Flag1)");
        }

        // (c) Variadic constructor
        {
            Flag<MyFlags> f_variadic(MyFlags::Flag1, MyFlags::Flag2, MyFlags::Flag3);
            checkEqual(f_variadic.get(), static_cast<std::uint8_t>(0x01 | 0x02 | 0x04),
                       "Flag<MyFlags>(Flag1, Flag2, Flag3)");
        }

        // (d) Constructor from numeric type (valid)
        {
            Flag<MyFlags> f_num_valid(3); // 3 == Flag1 | Flag2
            checkEqual(f_num_valid.get(), static_cast<std::uint8_t>(3), "Flag<MyFlags>(3) valid numeric");
        }

        // (e) Constructor from numeric type (invalid -> throws)
        {
            bool caught_exception = false;
            try
            {
                Flag<MyFlags> f_num_invalid(8); // 8 is invalid for MyFlags
            }
            catch(const std::invalid_argument&)
            {
                caught_exception = true;
            }
            catch(const std::exception&)
            {
                caught_exception = true;
            }
            checkTrue(caught_exception,
                      "Flag<MyFlags>(8) should throw std::invalid_argument (invalid numeric value)");
        }

        // (f) add(...) - variadic
        {
            Flag<MyFlags> f_add(MyFlags::Flag1);
            f_add.add(MyFlags::Flag2, MyFlags::Flag3);
            checkEqual(f_add.get(), static_cast<std::uint8_t>(0x01 | 0x02 | 0x04),
                       "Flag<MyFlags>::add(Flag2, Flag3)");
        }

        // (g) remove(...) - variadic
        {
            Flag<MyFlags> f_remove(MyFlags::Flag1, MyFlags::Flag2, MyFlags::Flag3);
            f_remove.remove(MyFlags::Flag2);
            checkEqual(f_remove.get(), static_cast<std::uint8_t>(0x01 | 0x04),
                       "Flag<MyFlags>::remove(Flag2)");
        }

        // (h) has(...) - variadic
        {
            Flag<MyFlags> f_has(MyFlags::Flag1, MyFlags::Flag2);
            checkTrue(f_has.has(MyFlags::Flag1),             "f_has.has(Flag1)");
            checkTrue(f_has.has(MyFlags::Flag2),             "f_has.has(Flag2)");
            checkTrue(f_has.has(MyFlags::Flag1, MyFlags::Flag2),
                      "f_has.has(Flag1, Flag2)");
            checkFalse(f_has.has(MyFlags::Flag3),            "f_has.has(Flag3)");
        }

        // (i) operator|  (bitwise OR)
        {
            Flag<MyFlags> f_or_1(MyFlags::Flag1);
            auto f_or_2 = f_or_1 | MyFlags::Flag2;
            checkEqual(f_or_2.get(), static_cast<std::uint8_t>(0x01 | 0x02),
                       "operator| (Flag1 | Flag2)");
        }

        // (j) operator|= (bitwise OR assignment)
        {
            Flag<MyFlags> f_or_assign(MyFlags::Flag1);
            f_or_assign |= MyFlags::Flag3;
            checkEqual(f_or_assign.get(), static_cast<std::uint8_t>(0x01 | 0x04),
                       "operator|=");
        }

        // (k) operator& (bitwise AND)
        {
            Flag<MyFlags> f_and(MyFlags::Flag1, MyFlags::Flag2, MyFlags::Flag3); // 7
            auto f_and_result = f_and & MyFlags::Flag2;
            checkEqual(f_and_result.get(), static_cast<std::uint8_t>(0x02),
                       "operator&");
        }

        // (l) operator&= (bitwise AND assignment)
        {
            Flag<MyFlags> f_and_assign(MyFlags::Flag1, MyFlags::Flag2); // 3
            f_and_assign &= MyFlags::Flag1;
            checkEqual(f_and_assign.get(), static_cast<std::uint8_t>(0x01),
                       "operator&=");
        }

        // (m) operator~ (bitwise NOT)
        {
            // If we have 0x01, ~0x01 == 0xFE in 8-bit, but let's just verify it's not 1.
            Flag<MyFlags> f_not(MyFlags::Flag1);
            auto f_not_result = ~f_not;
            checkFalse(f_not_result.get() == 0x01, "operator~(Flag1)");
        }

        // (n) Equality / inequality operators
        {
            Flag<MyFlags> f_eq_1(MyFlags::Flag1, MyFlags::Flag2); // 3
            Flag<MyFlags> f_eq_2(MyFlags::Flag1, MyFlags::Flag2); // 3
            Flag<MyFlags> f_neq(MyFlags::Flag3);                  // 4

            checkTrue((f_eq_1 == f_eq_2), "operator==(Flag1|Flag2, Flag1|Flag2)");
            checkTrue((f_eq_1 != f_neq), "operator!=(Flag1|Flag2, Flag3)");
        }
    }

    // -------------------------------------------------------------------------
    // Final Result
    // -------------------------------------------------------------------------
    if (g_testFailures == 0)
    {
        std::cout << "All Flag and utils tests passed!\n";
        return 0; // success
    }
    else
    {
        std::cerr << g_testFailures << " test(s) failed.\n";
        return 1; // failure
    }
}
