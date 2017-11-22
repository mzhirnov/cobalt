#ifndef COBALT_COM_CORE_HPP_INCLUDED
#define COBALT_COM_CORE_HPP_INCLUDED

#pragma once

// Classes in this file:
//     unknown
//     class_factory
//
// Functions in this file:
//     get_class_object
//     create_instance

#include <cobalt/com/error.hpp>
#include <cobalt/utility/type_index.hpp>
#include <cobalt/utility/intrusive.hpp>

namespace cobalt {
namespace com {

using iid = type_info;
using clsid = iid;

#define IIDOF(x) type_id<x>().type_info()

struct unknown {
	virtual size_t retain() noexcept = 0;
	virtual size_t release() noexcept = 0;
	virtual unknown* cast(const iid& iid) noexcept = 0;
};

struct class_factory : unknown {
	virtual unknown* create_instance(unknown* outer, const iid& iid) noexcept = 0;
};

extern unknown* get_class_object(const clsid& clsid) noexcept;
extern unknown* create_instance(unknown* outer, const clsid& clsid, const iid& iid) noexcept;

template <typename Q> inline ref_ptr<Q> create_instance(const clsid& clsid) noexcept
	{ return static_cast<Q*>(create_instance(nullptr, clsid, IIDOF(Q))); }

template <typename Q> inline ref_ptr<Q> create_instance(unknown* outer, const clsid& clsid) noexcept
	{ return static_cast<Q*>(create_instance(outer, clsid, IIDOF(Q))); }

} // namespace com
} // namespace cobalt

#endif // COBALT_COM_CORE_HPP_INCLUDED
