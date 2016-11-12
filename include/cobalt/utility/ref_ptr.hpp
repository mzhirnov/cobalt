#ifndef COBALT_UTILITY_REF_PTR_HPP_INCLUDED
#define COBALT_UTILITY_REF_PTR_HPP_INCLUDED

#pragma once

#include <boost/smart_ptr/intrusive_ref_counter.hpp>
#include <boost/smart_ptr/intrusive_ptr.hpp>

#include <type_traits>

namespace cobalt {

template <typename T>
using ref_counter = boost::intrusive_ref_counter<T>;

template <typename T>
using ref_ptr = boost::intrusive_ptr<T>;

template <typename T, typename = typename std::enable_if_t<std::is_base_of<ref_counter<T>, T>::value>>
inline void add_ref(T* p) {
	intrusive_ptr_add_ref(p);
}

template <typename T, typename = typename std::enable_if_t<std::is_base_of<ref_counter<T>, T>::value>>
inline void release(T* p) {
	intrusive_ptr_release(p);
}

} // namespace cobalt

#endif // COBALT_UTILITY_REF_PTR_HPP_INCLUDED
