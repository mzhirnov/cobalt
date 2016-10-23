#pragma once

#include <boost/smart_ptr/intrusive_ptr.hpp>

template <typename T, typename... Args>
boost::intrusive_ptr<T> make_intrusive(Args&&... args) {
	return new T(std::forward<Args>(args)...);
}
