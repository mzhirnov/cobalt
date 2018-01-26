#ifndef COBALT_COM_CORE_HPP_INCLUDED
#define COBALT_COM_CORE_HPP_INCLUDED

#pragma once

// Classes in this file:
//     any
//     class_factory
//
// Functions in this file:
//     get_class_object
//     create_instance

#include <cobalt/utility/intrusive.hpp>
#include <cobalt/utility/uid.hpp>

namespace cobalt {
namespace com {

#define DECLARE_INTERFACE(Namespace, Interface) \
	struct Interface; \
	DECLARE_UID_NS(Namespace, Interface)
	
#define DECLARE_CLASS(Namespace, Class) \
	class Class; \
	DECLARE_UID_NS(Namespace, Class)

/// Any is identity of an object
DECLARE_INTERFACE(com, any)
struct any {
	virtual size_t retain() noexcept = 0;
	virtual size_t release() noexcept = 0;
	virtual any* cast(const uid& iid) noexcept = 0;
};

/// Class factory creates object instances
DECLARE_INTERFACE(com, class_factory)
struct class_factory : any {
	virtual any* create_instance(any* outer, const uid& iid) noexcept = 0;
};

extern class_factory* get_class_object(const uid& clsid) noexcept;
extern any* create_instance(any* outer, const uid& clsid, const uid& iid) noexcept;

template <typename Q> inline ref_ptr<Q> create_instance(const uid& clsid) noexcept
	{ return static_cast<Q*>(create_instance(nullptr, clsid, UIDOF(Q))); }

template <typename Q> inline ref_ptr<Q> create_instance(any* outer, const uid& clsid) noexcept
	{ return static_cast<Q*>(create_instance(outer, clsid, UIDOF(Q))); }

} // namespace com
} // namespace cobalt

#endif // COBALT_COM_CORE_HPP_INCLUDED
