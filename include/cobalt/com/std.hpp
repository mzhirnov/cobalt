#ifndef COBALT_COM_STD_HPP_INCLUDED
#define COBALT_COM_STD_HPP_INCLUDED

#pragma once

// Classes in this file:
//     equatable
//     comparable
//     hashable
//     copyable
//     clonable
//     decoder
//     encoder
//     decodable
//     encodable

#include <cobalt/com/core.hpp>

#include <system_error>

namespace cobalt {
namespace com {

DECLARE_INTERFACE(com, equatable)
struct equatable : any {
	virtual bool equals(any*) const noexcept = 0;
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
	virtual ref<any> copy() const noexcept = 0;
};

DECLARE_INTERFACE(com, clonable)
struct clonable : copyable {
	virtual ref<any> clone() const noexcept = 0;
};

DECLARE_INTERFACE(com, decoder)
struct decoder : any {
	virtual void decode(const char* key, bool& value) noexcept = 0;
	virtual void decode(const char* key, int& value) noexcept = 0;
	virtual void decode(const char* key, int& value, int min, int max) noexcept = 0;
	virtual void decode(const char* key, float& value) noexcept = 0;
	virtual void decode(const char* key, float& value, float min, float max, float resolution) noexcept = 0;
	virtual void decode(const char* key, void* data, size_t size) noexcept = 0;
};

DECLARE_INTERFACE(com, encoder)
struct encoder : any {
	virtual void encode(const char* key, bool& value) noexcept = 0;
	virtual void encode(const char* key, int& value) noexcept = 0;
	virtual void encode(const char* key, int& value, int min, int max) noexcept = 0;
	virtual void encode(const char* key, float& value) noexcept = 0;
	virtual void encode(const char* key, float& value, float min, float max, float resolution) noexcept = 0;
	virtual void encode(const char* key, void* data, size_t size) noexcept = 0;
};

DECLARE_INTERFACE(com, decodable)
struct decodable : any {
	virtual void init(decoder*, std::error_code& ec) noexcept = 0;
};

DECLARE_INTERFACE(com, encodable)
struct encodable : any {
	virtual void encode(encoder*, std::error_code& ec) const noexcept = 0;
};

} // namespace com
} // namespace cobalt

#endif // COBALT_COM_STD_HPP_INCLUDED

