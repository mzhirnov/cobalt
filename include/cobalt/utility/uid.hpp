#ifndef COBALT_UTILITY_UID_HPP_INCLUDED
#define COBALT_UTILITY_UID_HPP_INCLUDED

#pragma once

// Classes in this file:
//     uid

#include <boost/functional/hash.hpp>
#include <boost/utility/string_view.hpp>

namespace cobalt {
	
/// Unique type identifier
class uid {
public:
	const char* name() const noexcept { return _name; }
	
	size_t hash_value() const noexcept { return boost::hash_value(this); }
	friend size_t hash_value(const uid& uid) noexcept { return uid.hash_value(); }
	
	constexpr bool operator==(const uid& rhs) noexcept { return this == &rhs; }
	constexpr bool operator!=(const uid& rhs) noexcept { return this != &rhs; }
	constexpr bool operator<(const uid& rhs) const noexcept { return std::less<const uid*>()(this, &rhs); }
	
	constexpr operator bool() const noexcept { return this != &null(); }
	
	template <typename T>
	static constexpr const uid& of(const T* = nullptr) noexcept { return data<T>::instance; }
	static constexpr const uid& null() noexcept { return data<std::nullptr_t>::instance; }
	
	static const uid& from_string(boost::string_view name) noexcept {
		for (auto p = head(); p; p = p->_next) {
			if (name == p->_name)
				return *p;
		}
		return null();
	}
	
private:
	explicit uid(const char* name) noexcept : _name(name) { head() = this; }
	
	uid(const uid&) = delete;
	uid& operator=(const uid&) = delete;
	
	static uid*& head() noexcept {
		static uid* head = nullptr;
		return head;
	}
	
private:
	const char* _name = nullptr;
	uid* _next = head();
	
	template <typename T>
	struct data {
		static uid instance;
	};
};

// Use uid of `nullptr_t` as `null`
inline constexpr const char* uid_name(const std::nullptr_t*) noexcept { return "null"; }
inline constexpr const uid& uid_value(const std::nullptr_t*) noexcept { return uid::of<std::nullptr_t>(); }

// Make use of ADL name lookup for `uid_name` here
template<typename T> uid uid::data<T>::instance(uid_name(static_cast<const T*>(nullptr)));
	
#define DECLARE_UID(Type) \
	DECLARE_UID_WITH_NAME(Type, #Type)
	
#define DECLARE_UID_NS(Namespace, Type) \
	DECLARE_UID_WITH_NAME_NS(Namespace, Type, #Type)
	
#define DECLARE_UID_WITH_NAME_NS(Namespace, Type, Name) \
	DECLARE_UID_WITH_NAME(Type, #Namespace "::" Name)
	
#define DECLARE_UID_WITH_NAME(Type, Name) \
	inline constexpr const char* uid_name(const Type*) noexcept { return Name; } \
	inline constexpr const ::cobalt::uid& uid_value(const Type*) noexcept { return ::cobalt::uid::of<Type>(); }
	
#define UIDOF(x) \
	::cobalt::uid::of<x>()
	
} // namespace cobalt

#endif // COBALT_UTILITY_UID_HPP_INCLUDED
