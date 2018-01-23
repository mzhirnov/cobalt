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

inline void intrusive_ptr_add_ref(any* p) noexcept { BOOST_ASSERT(!!p); p ? p->retain() : (size_t)0; }
inline void intrusive_ptr_release(any* p) noexcept { BOOST_ASSERT(!!p); p ? p->release() : (size_t)0; }

template <typename Q> inline ref_ptr<Q> cast(any* p) noexcept { BOOST_ASSERT(!!p); return p ? static_cast<Q*>(p->cast(UIDOF(Q))) : nullptr; }
template <typename Q> inline ref_ptr<Q> cast(const ref_ptr<any>& sp) noexcept { return cast<Q>(sp.get()); }

/// Checks if two interfaces have the same identity
inline bool identical(any* p1, any* p2) noexcept {
	if (!p1 || !p2) return p1 == p2;
	// Having been called from any interface, `cast()` must return the same pointer to `any`
	return p1->cast(UIDOF(any)) == p2->cast(UIDOF(any));
}

inline bool identical(const ref_ptr<any>& sp1, const ref_ptr<any>& sp2) noexcept { return identical(sp1.get(), sp2.get()); }

} // namespace com
} // namespace cobalt

#endif // COBALT_COM_UTILITY_HPP_INCLUDED
