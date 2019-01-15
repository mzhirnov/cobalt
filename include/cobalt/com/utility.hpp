#ifndef COBALT_COM_UTILITY_HPP_INCLUDED
#define COBALT_COM_UTILITY_HPP_INCLUDED

#pragma once

// Functions in this file:
//     cast
//     identical

#include <cobalt/com/core.hpp>
#include <cobalt/utility/intrusive.hpp>

namespace cobalt {
namespace com {

// Helper functions for boost::intrusive_ptr<> to deal with `any`

inline void intrusive_ptr_add_ref(any* p) noexcept {
	BOOST_ASSERT(p);
	p ? p->retain() : 0;
}

inline void intrusive_ptr_release(any* p) noexcept {
	BOOST_ASSERT(p);
	p ? p->release() : 0;
}

// Safe functions for casting objects

template <typename Q>
inline ref_ptr<Q> cast(any* p, std::error_code& ec) noexcept {
	if (!p) {
		ec = make_error_code(std::errc::invalid_argument);
		return nullptr;
	}
	return static_cast<Q*>(p->cast(UIDOF(Q), ec));
}

template <typename Q>
inline ref_ptr<Q> cast(any* p) noexcept {
	std::error_code ec;
	auto q = cast<Q>(p, ec);
	BOOST_ASSERT_MSG(!ec, ec.message().c_str());
	return q;
}

template <typename Q>
inline ref_ptr<Q> cast(const ref_ptr<any>& sp, std::error_code& ec) noexcept
	{ return cast<Q>(sp.get(), ec); }

template <typename Q>
inline ref_ptr<Q> cast(const ref_ptr<any>& sp) noexcept
	{ return cast<Q>(sp.get()); }

// Functions for checking if two objects have the same identity

inline bool identical(any* lhs, any* rhs) noexcept {
	// Treat two nullptrs as identical objects
	if (!lhs || !rhs) return lhs == rhs;
	
	std::error_code ec;
	
	auto id1 = lhs->cast(UIDOF(any), ec);
	BOOST_ASSERT(!ec);
	if (ec) return false;
	
	auto id2 = rhs->cast(UIDOF(any), ec);
	BOOST_ASSERT(!ec);
	if (ec) return false;
	
	// Having been called from any interface, `cast()` must return the same pointer to `any`.
	// It must also not create new object, so don't care about retain/release.
	return id1 == id2;
}

inline bool identical(const ref_ptr<any>& lhs, const ref_ptr<any>& rhs) noexcept
	{ return identical(lhs.get(), rhs.get()); }

} // namespace com
} // namespace cobalt

#endif // COBALT_COM_UTILITY_HPP_INCLUDED
