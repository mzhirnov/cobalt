#ifndef COBALT_UTILITY_OVERLOADED_HPP_INCLUDED
#define COBALT_UTILITY_OVERLOADED_HPP_INCLUDED

#pragma once

namespace cobalt {

/*
template <typename... Ts>
struct overloaded;

template <typename T, typename... Rest>
struct overloaded<T, Rest...> : T, overloaded<Rest...> {
	overload(T&& t, Rest&&... rest)
		: T(std::forward<T>(t))
		, overloaded<Rest...>(std::forward<Rest>(rest)...)
	{}
	
	using T::operator();
	using overloaded<Rest...>::operator();
};

template <typename T>
struct overloaded<T> : T {
	overloaded(T&& t) : T(std::forward<T>(t)) {}
	
	using T::operator();
};

template <typename... Ts>
inline auto make_overloaded(Ts&&... ts) {
	return overloaded<Ts...>(std::forward<Ts>(ts)...);
}
*/

template <typename... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template <typename... Ts> overloaded(Ts...) -> overloaded<Ts...>;

} // namespace cobalt

#endif // COBALT_UTILITY_OVERLOADED_HPP_INCLUDED

