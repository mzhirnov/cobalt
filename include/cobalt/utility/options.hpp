#ifndef COBALT_UTILITY_OPTIONS_HPP_INCLUDED
#define COBALT_UTILITY_OPTIONS_HPP_INCLUDED

#pragma once

#include <type_traits>

namespace cobalt {

template <typename T>
struct is_options_enum {
private:
	template <typename E, typename = std::enable_if_t<std::is_enum_v<E>>>
	static std::bool_constant<E::none == static_cast<E>(0)> test(int);
	
	template <typename E>
	static std::false_type test(...);

	using type = decltype(test<T>(0));
	
public:
	static constexpr bool value = type::value;
};

template <typename T>
static constexpr bool is_options_enum_v = is_options_enum<T>::value;

constexpr inline size_t bit(size_t n) noexcept { return 1 << n; }

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

template <typename T, typename = std::enable_if_t<std::is_integral_v<T> || cobalt::is_options_enum_v<T>>>
constexpr bool has_options(T value, T options) noexcept {
	return (value & options) == options;
}

#endif // COBALT_UTILITY_OPTIONS_HPP_INCLUDED
