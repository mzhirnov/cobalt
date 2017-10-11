#ifndef COBALT_UTILITY_OVERLOAD_HPP_INCLUDED
#define COBALT_UTILITY_OVERLOAD_HPP_INCLUDED

#pragma once

namespace cobalt {

template <typename... Ts>
struct overload;

template <typename T, typename... Rest>
struct overload<T, Rest...> : T, overload<Rest...> {
	overload(T&& t, Rest&&... rest)
		: T(std::forward<T>(t))
		, overload<Rest...>(std::forward<Rest>(rest)...)
	{}
	
	using T::operator();
	using overload<Rest...>::operator();
};

template <typename T>
struct overload<T> : T {
	overload(T&& t) : T(std::forward<T>(t)) {}
	
	using T::operator();
};

template <typename... Ts>
inline auto make_overload(Ts&&... ts) {
	return overload<Ts...>(std::forward<Ts>(ts)...);
}

} // namespace cobalt

#endif // COBALT_UTILITY_OVERLOAD_HPP_INCLUDED

