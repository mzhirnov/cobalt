#ifndef COBALT_ENUMERATOR_HPP_INCLUDED
#define COBALT_ENUMERATOR_HPP_INCLUDED

#pragma once

#include <type_traits>

namespace cobalt {

template <typename Iterator>
class enumerator {
public:
	typedef Iterator iterator;

	enumerator(iterator begin, iterator end)
		: _begin(begin)
		, _end(end)
	{
	}

	iterator begin() const { return _begin; }
	iterator end() const { return _end; }

private:
	iterator _begin;
	iterator _end;
};

template <typename Container>
enumerator<typename Container::iterator> make_enumerator(Container& container) {
	return { std::begin(container), std::end(container) };
}

template <typename Container>
enumerator<typename Container::const_iterator> make_enumerator(const Container& container) {
	return { std::cbegin(container), std::cend(container) };
}

template <typename T, size_t N>
enumerator<T*> make_enumerator(T(&array)[N]) {
	return { &array[0], &array[N] };
}

template <typename T, typename = std::enable_if_t<std::is_pointer<T>::value>>
enumerator<T> make_enumerator(T begin, T end) {
	return { begin, end };
}

} // namespace cobalt
	
#endif // COBALT_ENUMERATOR_HPP_INCLUDED
