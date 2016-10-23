#pragma once

template <typename Iterator>
class enumerator {
public:
	typedef Iterator iterator;

	explicit enumerator(iterator begin, iterator end)
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
	return enumerator<typename Container::iterator>(container.begin(), container.end());
}

template <typename Container>
enumerator<typename Container::const_iterator> make_enumerator(const Container& container) {
	return enumerator<typename Container::const_iterator>(container.cbegin(), container.cend());
}

template <typename T, size_t N>
enumerator<T*> make_enumerator(T(&array)[N]) {
	return enumerator<T*>(&array[0], &array[N]);
}

template <typename T>
enumerator<T*> make_enumerator(T* begin, T* end) {
	return enumerator<T*>(begin, end);
}
