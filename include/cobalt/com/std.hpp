#ifndef COBALT_COM_STD_HPP_INCLUDED
#define COBALT_COM_STD_HPP_INCLUDED

#pragma once

// Classes in this file:
//     equatable
//     comparable
//     hashable
//     copyable
//     coder
//     decodable
//     encodable
//     codable

#include <cobalt/com/error.hpp>
#include <cobalt/utility/intrusive.hpp>
#include <cobalt/utility/uid.hpp>

namespace cobalt {
namespace com {

DECLARE_INTERFACE(com, equatable)
struct equatable : any {
	virtual bool equal(any*) const noexcept = 0;
};

DECLARE_INTERFACE(com, comparable)
struct comparable : equatable {
	virtual bool less(any*) const noexcept = 0;
};

DECLARE_INTERFACE(com, hashable)
struct hashable : equatable {
	virtual size_t hash_value() const noexcept = 0;
};

DECLARE_INTERFACE(com, copyable)
struct copyable : any {
	virtual any* copy() const noexcept = 0;
};

enum class coding_mode {
	encoding,
	decoding
};

DECLARE_INTERFACE(com, coder)
struct coder : any {
	virtual coding_mode mode() const noexcept = 0;

	virtual bool encode_bool(const char* name, bool& value) noexcept = 0;
	virtual bool encode_int(const char* name, int& value) noexcept = 0;
	virtual bool encode_int_range(const char* name, int& value, int min, int max) noexcept = 0;
	virtual bool encode_float(const char* name, float& value) noexcept = 0;
	virtual bool encode_float_range(const char* name, float& value, float min, float max, float resolution) noexcept = 0;
	virtual bool encode_bytes(const char* name, void* data, size_t size_in_bytes) noexcept = 0;
};

DECLARE_INTERFACE(com, decodable)
struct decodable : any {
	virtual void init(coder*) noexcept = 0;
};

DECLARE_INTERFACE(com, encodable)
struct encodable : any {
	virtual void encode(coder*) const noexcept = 0;
};

DECLARE_INTERFACE(com, codable)
struct codable : decodable, encodable {
};

} // namespace com
} // namespace cobalt

#endif // COBALT_COM_STD_HPP_INCLUDED

