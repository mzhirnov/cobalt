#ifndef COBALT_UTILITY_STATIC_INITIALIZER_HPP_INCLUDED
#define COBALT_UTILITY_STATIC_INITIALIZER_HPP_INCLUDED

#pragma once

namespace cobalt {

template <typename T>
class static_initializer {
public:
	class helper {
	public:
		helper() noexcept
			: _instance(_helper)
		{
			T::static_initialize();
		}
		
		~helper() noexcept
		{
			T::static_uninitialize();
		}
		
	private:
		helper& _instance;
	};
	
	static static_initializer::helper& instance() noexcept {
		return _helper;
	}
	
private:
	static helper _helper;
};

template <typename T> typename static_initializer<T>::helper static_initializer<T>::_helper;

template <typename T>
class static_initializable {
public:
	~static_initializable() noexcept {
		static_initializer<T>::instance();
	}
};

#define CO_STATIC_INITIALIZE(Class) \
	private: \
		friend class static_initializer<Class>; \
		static const typename static_initializer<Class>::helper& initializer() noexcept { \
			return static_initializer<Class>::instance(); \
		} \
	public:

} // namespace cobalt

#endif // COBALT_UTILITY_STATIC_INITIALIZER_HPP_INCLUDED
