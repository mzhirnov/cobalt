#ifndef COBALT_COM_UTILITY_HPP_INCLUDED
#define COBALT_COM_UTILITY_HPP_INCLUDED

#pragma once

// Functions in this file:
//     cast
//     identical

#include <cobalt/com/core.hpp>

namespace cobalt {
namespace com {

// Safe functions for casting objects

template <typename Q>
inline ref<Q> cast(const ref<any>& rid, std::error_code& ec) noexcept {
	if (!rid) {
		ec = make_error_code(std::errc::invalid_argument);
		return nullptr;
	}
	return boost::static_pointer_cast<Q>(rid->cast(UIDOF(Q), ec));
}

template <typename Q>
inline ref<Q> cast(const ref<any>& rid) noexcept {
	std::error_code ec;
	auto ret = cast<Q>(rid, ec);
	BOOST_ASSERT_MSG(!ec, ec.message().c_str());
	return ret;
}

// Functions for checking if two objects have the same identity

inline bool identical(const ref<any>& lhs, const ref<any>& rhs) noexcept {
	// Treat two nullptrs as identical objects.
	if (!lhs || !rhs) return lhs == rhs;
	
	std::error_code ec;
	
	auto rid1 = lhs->cast(UIDOF(any), ec);
	BOOST_ASSERT(!ec);
	if (ec) return false;
	
	auto rid2 = rhs->cast(UIDOF(any), ec);
	BOOST_ASSERT(!ec);
	if (ec) return false;
	
	// Having been called from any interface, `cast()` must return the same pointer to `any`.
	return rid1 == rid2;
}

} // namespace com
} // namespace cobalt

#endif // COBALT_COM_UTILITY_HPP_INCLUDED
