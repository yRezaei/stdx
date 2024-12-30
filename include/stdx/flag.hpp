#pragma once

#include <type_traits>
#include <cstdint>
#include <stdexcept>
#include "stdx/utils.hpp"

namespace stdx
{

    template <typename EnumType>
    class Flag
    {
        static_assert(std::is_enum_v<EnumType>, "EnumType must be an enumeration");
        static_assert(stdx::utils::has_enum_all_v<EnumType>,
                      "EnumType must define an 'All' enumerator (e.g. EnumType::All) in order to use Flag.");

        using value_type = typename std::underlying_type<EnumType>::type;

    public:
        // Default constructor
        Flag() : value(0) {}

        // Constructor from a single enum value
        explicit Flag(EnumType flag) : value(static_cast<value_type>(flag)) {}

        // Variadic constructor
        template <typename... Flags>
        explicit Flag(EnumType first_flag, Flags... rest_flags)
        {
            static_assert((std::conjunction_v<std::is_same<EnumType, Flags>...>),
                          "All arguments must be of the same enum type");
            value = stdx::utils::combine_flags(first_flag, rest_flags...);
        }

        // Constructor from numeric type
        explicit Flag(value_type numeric_value) : value(numeric_value)
        {
            if (!stdx::utils::is_valid_combination<EnumType>(numeric_value))
            {
                throw std::invalid_argument("Numeric value does not represent a valid combination of enum flags.");
            }
        }

        // Copy and move constructors
        Flag(const Flag &other) = default;
        Flag(Flag &&other) noexcept = default;

        // Copy and move assignment operators
        Flag &operator=(const Flag &other) = default;
        Flag &operator=(Flag &&other) noexcept = default;

        // Destructor
        ~Flag() = default;

        // Get the underlying value
        value_type get() const
        {
            return value;
        }

        // Variadic add function
        template <typename... Flags>
        void add(EnumType first_flag, Flags... rest_flags)
        {
            static_assert((std::conjunction_v<std::is_same<EnumType, Flags>...>),
                          "All arguments must be of the same enum type");
            value |= stdx::utils::combine_flags(first_flag, rest_flags...);
        }

        // Variadic remove function
        template <typename... Flags>
        void remove(EnumType first_flag, Flags... rest_flags)
        {
            static_assert((std::conjunction_v<std::is_same<EnumType, Flags>...>),
                          "All arguments must be of the same enum type");
            value &= ~stdx::utils::combine_flags(first_flag, rest_flags...);
        }

        // Variadic has function
        template <typename... Flags>
        bool has(EnumType first_flag, Flags... rest_flags) const
        {
            static_assert((std::conjunction_v<std::is_same<EnumType, Flags>...>),
                          "All arguments must be of the same enum type");
            value_type mask = stdx::utils::combine_flags(first_flag, rest_flags...);
            return (value & mask) == mask;
        }

        // Bitwise OR operator
        Flag operator|(EnumType flag) const
        {
            return Flag(static_cast<value_type>(value | static_cast<value_type>(flag)));
        }

        // Bitwise OR assignment
        Flag &operator|=(EnumType flag)
        {
            value |= static_cast<value_type>(flag);
            return *this;
        }

        // Bitwise AND operator
        Flag operator&(EnumType flag) const
        {
            return Flag(static_cast<value_type>(value & static_cast<value_type>(flag)));
        }

        // Bitwise AND assignment
        Flag &operator&=(EnumType flag)
        {
            value &= static_cast<value_type>(flag);
            return *this;
        }

        Flag operator~() const
        {
            // Suppose 'All' enumerates all bits you consider valid (e.g. 0x07).
            // So we XOR with that mask to flip *only* those bits, not the entire byte.
            constexpr value_type MASK = static_cast<value_type>(EnumType::All);
            return Flag(value ^ MASK);
        }

        bool operator==(const Flag &other) const
        {
            return value == other.value;
        }

        bool operator!=(const Flag &other) const
        {
            return value != other.value;
        }

    private:
        value_type value;
    };

} // namespace stdx
