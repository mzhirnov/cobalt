#ifndef COBALT_UTILITY_OPTIONS_HPP_INCLUDED
#define COBALT_UTILITY_OPTIONS_HPP_INCLUDED

#pragma once

#include <type_traits>

#define DECLARE_OPTIONS_ENUM(E) \
namespace cobalt { template <> struct is_options_enum<E> : std::true_type {}; }

namespace cobalt {

constexpr inline auto bit(unsigned int n) noexcept { return 1u << n; }

template <typename T>
struct is_options_enum : std::false_type {};

template <typename T>
static constexpr bool is_options_enum_v = is_options_enum<T>::value;

template <typename T, typename =
	std::enable_if_t<
		(std::is_integral_v<T> && std::is_unsigned_v<T>) ||
		cobalt::is_options_enum_v<T>
	>>
constexpr bool has_options(T value, T options) noexcept {
	return (value & options) == options;
}

template <typename E, typename U = std::underlying_type_t<E>, typename =
	std::enable_if_t<
		std::is_enum_v<E> && std::is_unsigned_v<U>
	>>
constexpr bool has_options(U value, E options) noexcept {
	return static_cast<E>(value & static_cast<U>(options)) == options;
}

} // namespace cobalt

template <typename E, typename = std::enable_if_t<cobalt::is_options_enum_v<E>>, typename U = std::underlying_type_t<E>>
constexpr E operator~(E value) noexcept { return static_cast<E>(~static_cast<U>(value)); }

template <typename E, typename = std::enable_if_t<cobalt::is_options_enum_v<E>>, typename U = std::underlying_type_t<E>>
constexpr E operator|(E lhs, E rhs) noexcept { return static_cast<E>(static_cast<U>(lhs) | static_cast<U>(rhs)); }

template <typename E, typename = std::enable_if_t<cobalt::is_options_enum_v<E>>, typename U = std::underlying_type_t<E>>
constexpr E operator&(E lhs, E rhs) noexcept { return static_cast<E>(static_cast<U>(lhs) & static_cast<U>(rhs)); }

template <typename E, typename = std::enable_if_t<cobalt::is_options_enum_v<E>>, typename U = std::underlying_type_t<E>>
constexpr E operator^(E lhs, E rhs) noexcept { return static_cast<E>(static_cast<U>(lhs) ^ static_cast<U>(rhs)); }

template <typename E, typename = std::enable_if_t<cobalt::is_options_enum_v<E>>, typename U = std::underlying_type_t<E>>
E& operator|=(E& lhs, E rhs) noexcept { return reinterpret_cast<E&>(reinterpret_cast<U&>(lhs) |= static_cast<U>(rhs)); }

template <typename E, typename = std::enable_if_t<cobalt::is_options_enum_v<E>>, typename U = std::underlying_type_t<E>>
E& operator&=(E& lhs, E rhs) noexcept { return reinterpret_cast<E&>(reinterpret_cast<U&>(lhs) &= static_cast<U>(rhs)); }

template <typename E, typename = std::enable_if_t<cobalt::is_options_enum_v<E>>, typename U = std::underlying_type_t<E>>
E& operator^=(E& lhs, E rhs) noexcept { return reinterpret_cast<E&>(reinterpret_cast<U&>(lhs) ^= static_cast<U>(rhs)); }

#endif // COBALT_UTILITY_OPTIONS_HPP_INCLUDED
