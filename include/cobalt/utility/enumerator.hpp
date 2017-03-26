#ifndef COBALT_UTILITY_ENUMERATOR_HPP_INCLUDED
#define COBALT_UTILITY_ENUMERATOR_HPP_INCLUDED

#pragma once

#include <type_traits>

namespace cobalt {

template <typename Iterator>
class enumerator {
public:
	using iterator_type = Iterator;

	constexpr enumerator(iterator_type begin, iterator_type end) noexcept
		: _begin(begin)
		, _end(end)
	{
	}

	constexpr iterator_type begin() const noexcept { return _begin; }
	constexpr iterator_type end() const noexcept { return _end; }

private:
	iterator_type _begin;
	iterator_type _end;
};

template <typename Container>
constexpr enumerator<typename Container::iterator> make_enumerator(Container& container) noexcept {
	return { std::begin(container), std::end(container) };
}

template <typename Container>
constexpr enumerator<typename Container::const_iterator> make_enumerator(const Container& container) noexcept {
	return { std::cbegin(container), std::cend(container) };
}

template <typename T, size_t N>
constexpr enumerator<T*> make_enumerator(T(&array)[N]) noexcept {
	return { std::begin(array), std::end(array) };
}

template <typename T, typename = std::enable_if_t<std::is_pointer<T>::value>>
constexpr enumerator<T> make_enumerator(T begin, T end) noexcept {
	return { begin, end };
}

} // namespace cobalt
	
#endif // COBALT_UTILITY_ENUMERATOR_HPP_INCLUDED
