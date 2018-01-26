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

inline void intrusive_ptr_add_ref(any* p) noexcept
	{ BOOST_ASSERT(!!p); p ? p->retain() : 0; }

inline void intrusive_ptr_release(any* p) noexcept
	{ BOOST_ASSERT(!!p); p ? p->release() : 0; }

// Safe functions for casting objects

template <typename Q>
inline ref_ptr<Q> cast(any* p) noexcept
	{ BOOST_ASSERT(!!p); return p ? static_cast<Q*>(p->cast(UIDOF(Q))) : nullptr; }

template <typename Q>
inline ref_ptr<Q> cast(const ref_ptr<any>& sp) noexcept
	{ return cast<Q>(sp.get()); }

// Functions for checking if two objects have the same identity

inline bool identical(any* lhs, any* rhs) noexcept {
	// Treat two nullptrs as identical objects
	if (!lhs || !rhs) return lhs == rhs;
	// Having been called from any interface, `cast()` must return the same pointer to `any`
	return lhs->cast(UIDOF(any)) == rhs->cast(UIDOF(any));
}

inline bool identical(const ref_ptr<any>& lhs, const ref_ptr<any>& rhs) noexcept
	{ return identical(lhs.get(), rhs.get()); }

} // namespace com
} // namespace cobalt

#endif // COBALT_COM_UTILITY_HPP_INCLUDED
