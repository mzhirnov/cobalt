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

#include <cobalt/utility/uid.hpp>

#include <boost/smart_ptr/intrusive_ptr.hpp>

#include <system_error>

namespace cobalt {
namespace com {

#define DECLARE_INTERFACE(Namespace, Interface) \
	struct Interface; \
	DECLARE_UID_NS(Namespace, Interface)
	
#define DECLARE_CLASS(Namespace, Class) \
	class Class; \
	DECLARE_UID_NS(Namespace, Class)
	
DECLARE_INTERFACE(com, any)
DECLARE_INTERFACE(com, class_factory)
	
template <typename T, typename = std::enable_if_t<std::is_base_of_v<any, T>>>
using ref = boost::intrusive_ptr<T>;

/// Any is identity of an object
struct any {
	virtual ref<any> cast(const uid& iid, std::error_code& ec) noexcept = 0;

protected:
	virtual uint32_t retain() noexcept = 0;
	virtual uint32_t release() noexcept = 0;
	
	friend void intrusive_ptr_add_ref(any* p) noexcept { BOOST_ASSERT(p); p ? p->retain() : 0; }
	friend void intrusive_ptr_release(any* p) noexcept { BOOST_ASSERT(p); p ? p->release() : 0; }
};

/// Class factory creates object instances
struct class_factory : any {
	virtual ref<any> create_instance(any* outer, const uid& iid, std::error_code& ec) noexcept = 0;
};

// These functions are implemented in class module_base as friends.
extern ref<class_factory> get_class_object(const uid& clsid, std::error_code& ec) noexcept;
extern ref<any> create_instance(any* outer, const uid& clsid, const uid& iid, std::error_code& ec) noexcept;

template <typename Q>
inline ref<Q> create_instance(const uid& clsid, std::error_code& ec) noexcept
	{ return boost::static_pointer_cast<Q>(create_instance(nullptr, clsid, UIDOF(Q), ec)); }

template <typename Q>
inline ref<Q> create_instance(any* outer, const uid& clsid, std::error_code& ec) noexcept
	{ return boost::static_pointer_cast<Q>(create_instance(outer, clsid, UIDOF(Q), ec)); }

} // namespace com
} // namespace cobalt

#endif // COBALT_COM_CORE_HPP_INCLUDED
