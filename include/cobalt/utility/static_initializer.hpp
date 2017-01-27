#ifndef COBALT_UTILITY_STATIC_INITIALIZER_HPP_INCLUDED
#define COBALT_UTILITY_STATIC_INITIALIZER_HPP_INCLUDED

#pragma once

namespace cobalt {

template <typename T>
struct static_initializer {
	struct helper {
		helper& _instantiate_me;
		
		helper() : _instantiate_me(_helper)
		{
			T::static_initialize();
		}
		
		~helper()
		{
			T::static_uninitialize();
		}
	};

private:
	static helper _helper;
};

template <typename T> typename static_initializer<T>::helper static_initializer<T>::_helper;

#define DECLARE_STATIC_INITIALIZER(Class) \
	private: \
		friend struct static_initializer<Class>; \
		static static_initializer<Class>::helper initializer() { return {}; } \
	public:

} // namespace cobalt

#endif // COBALT_UTILITY_STATIC_INITIALIZER_HPP_INCLUDED
