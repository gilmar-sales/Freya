#pragma once

#include "Freya/Config.hpp"

#include <concepts>
#include <cstdint>
#include <type_traits>

namespace FREYA_NAMESPACE
{
    template <typename T>
    concept FlagEnum = std::is_enum_v<T> && requires {
        { T::None } -> std::convertible_to<T>;
    };

    template <FlagEnum T>
    [[nodiscard]] constexpr std::underlying_type_t<T> ToUnderlying(T value)
    {
        return static_cast<std::underlying_type_t<T>>(value);
    }

    template <FlagEnum T>
    [[nodiscard]] constexpr T operator|(T lhs, T rhs) noexcept
    {
        return static_cast<T>(ToUnderlying(lhs) | ToUnderlying(rhs));
    }

    template <FlagEnum T>
    [[nodiscard]] constexpr T operator&(T lhs, T rhs) noexcept
    {
        return static_cast<T>(ToUnderlying(lhs) & ToUnderlying(rhs));
    }

    template <FlagEnum T>
    [[nodiscard]] constexpr T operator^(T lhs, T rhs) noexcept
    {
        return static_cast<T>(ToUnderlying(lhs) ^ ToUnderlying(rhs));
    }

    template <FlagEnum T>
    [[nodiscard]] constexpr T operator~(T value) noexcept
    {
        return static_cast<T>(~ToUnderlying(value));
    }

    template <FlagEnum T>
    constexpr T& operator|=(T& lhs, T rhs) noexcept
    {
        lhs = lhs | rhs;
        return lhs;
    }

    template <FlagEnum T>
    constexpr T& operator&=(T& lhs, T rhs) noexcept
    {
        lhs = lhs & rhs;
        return lhs;
    }

    template <FlagEnum T>
    constexpr T& operator^=(T& lhs, T rhs) noexcept
    {
        lhs = lhs ^ rhs;
        return lhs;
    }

    template <FlagEnum T>
    [[nodiscard]] constexpr bool HasFlag(T value, T flag) noexcept
    {
        return (ToUnderlying(value) & ToUnderlying(flag)) != 0;
    }

    template <FlagEnum T>
    constexpr void SetFlag(T& value, T flag, bool enabled = true) noexcept
    {
        if (enabled)
            value |= flag;
        else
            value = static_cast<T>(ToUnderlying(value) & ~ToUnderlying(flag));
    }

    template <FlagEnum T>
    [[nodiscard]] constexpr std::uint32_t ToBits(T value) noexcept
    {
        return static_cast<std::uint32_t>(ToUnderlying(value));
    }

    template <FlagEnum T>
    [[nodiscard]] constexpr T FromBits(std::uint32_t bits) noexcept
    {
        return static_cast<T>(bits);
    }

} // namespace FREYA_NAMESPACE
