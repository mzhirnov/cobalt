#ifndef COBALT_UTILITY_MAKE_INTRUSIVE_HPP_INCLUDED
#define COBALT_UTILITY_MAKE_INTRUSIVE_HPP_INCLUDED

#pragma once

#include <boost/smart_ptr/intrusive_ptr.hpp>

namespace cobalt {

template <typename T, typename... Args>
boost::intrusive_ptr<T> make_intrusive(Args&&... args) {
	return new T(std::forward<Args>(args)...);
}
	
} // namespace cobalt

#endif // COBALT_UTILITY_MAKE_INTRUSIVE_HPP_INCLUDED
