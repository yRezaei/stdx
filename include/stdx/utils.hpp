#pragma once

#include <type_traits>
#include <cstdint>

namespace stdx
{
    namespace utils
    {
        // Primary template: defaults to false
        template <typename T, typename = void>
        struct has_enum_all : std::false_type
        {
        };

        // Specialization: if we can do `static_cast<underlying>(T::All)`, then T::All exists.
        template <typename T>
        struct has_enum_all<T, std::void_t<decltype(static_cast<typename std::underlying_type<T>::type>(T::All))>>
            : std::true_type
        {
        };

        // Helper variable template (C++17)
        template <typename T>
        constexpr bool has_enum_all_v = has_enum_all<T>::value;

        // Helper to check if a numeric value is a valid combination of Enum flags
        template <typename EnumType>
        constexpr bool is_valid_combination(typename std::underlying_type<EnumType>::type numeric_value)
        {
            static_assert(std::is_enum_v<EnumType>, "EnumType must be an enumeration");
            using value_type = typename std::underlying_type<EnumType>::type;
            constexpr value_type ALL_BITS = static_cast<value_type>(EnumType::All);

            return (numeric_value & ~ALL_BITS) == 0;
        }

        // Helper to combine multiple enum flags into a single underlying value
        template <typename EnumType>
        constexpr typename std::underlying_type<EnumType>::type combine_flags(EnumType flag)
        {
            return static_cast<typename std::underlying_type<EnumType>::type>(flag);
        }

        template <typename EnumType, typename... Flags>
        constexpr typename std::underlying_type<EnumType>::type combine_flags(EnumType first_flag, Flags... rest_flags)
        {
            static_assert(std::is_enum_v<EnumType>, "EnumType must be an enumeration");
            return combine_flags(first_flag) | combine_flags(rest_flags...);
        }

    } // namespace utils
} // namespace stdx
