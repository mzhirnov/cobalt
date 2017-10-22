#ifndef COBALT_COM_UTILITY_HPP_INCLUDED
#define COBALT_COM_UTILITY_HPP_INCLUDED

#pragma once

// Functions in this file:
//     cast
//     same_objects

#include <cobalt/com/core.hpp>
#include <cobalt/utility/intrusive.hpp>

namespace cobalt {
namespace com {

inline void intrusive_ptr_add_ref(unknown* p) noexcept { p->retain(); }
inline void intrusive_ptr_release(unknown* p) noexcept { p->release(); }

template <typename Q> inline ref_ptr<Q> cast(unknown* unk) noexcept { return unk ? static_cast<Q*>(unk->cast(IIDOF(Q))) : nullptr; }
template <typename Q> inline ref_ptr<Q> cast(const ref_ptr<unknown>& unk) noexcept { return cast<Q>(unk.get()); }

inline bool same_objects(unknown* obj1, unknown* obj2) noexcept {
	if (!obj1 || !obj2) return obj1 == obj2;
	// Having been called from any object's interface, `cast` *must* return the same pointer to `unknown`
	return obj1->cast(IIDOF(unknown)) == obj2->cast(IIDOF(unknown));
}

inline bool same_objects(const ref_ptr<unknown>& obj1, const ref_ptr<unknown>& obj2) noexcept { return same_objects(obj1.get(), obj2.get()); }

} // namespace com
} // namespace cobalt

#endif // COBALT_COM_UTILITY_HPP_INCLUDED
