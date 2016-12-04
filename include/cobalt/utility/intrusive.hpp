#ifndef COBALT_UTILITY_INTRUSIVE_HPP_INCLUDED
#define COBALT_UTILITY_INTRUSIVE_HPP_INCLUDED

#pragma once

#include <boost/smart_ptr/intrusive_ref_counter.hpp>
#include <boost/smart_ptr/intrusive_ptr.hpp>

#include <boost/intrusive/slist.hpp>
#include <boost/intrusive/list.hpp>

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

template <typename T, typename... Args>
inline ref_ptr<T> make_ref(Args&&... args) {
	return new T(std::forward<Args>(args)...);
}

template <typename Tag>
using intrusive_slist_base = boost::intrusive::slist_base_hook<boost::intrusive::tag<Tag>, boost::intrusive::link_mode<boost::intrusive::safe_link>>;

template <typename T, typename Tag = T>
using intrusive_slist = boost::intrusive::slist<T, boost::intrusive::base_hook<intrusive_slist_base<Tag>>, boost::intrusive::constant_time_size<false>>;

template <typename Tag>
using intrusive_list_base = boost::intrusive::list_base_hook<boost::intrusive::tag<Tag>, boost::intrusive::link_mode<boost::intrusive::safe_link>>;

template <typename T, typename Tag = T>
using intrusive_list = boost::intrusive::list<T, boost::intrusive::base_hook<intrusive_list_base<Tag>>, boost::intrusive::constant_time_size<false>>;

} // namespace cobalt

#endif // COBALT_UTILITY_INTRUSIVE_HPP_INCLUDED
