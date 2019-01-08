#ifndef COBALT_UTILITY_UID_HPP_INCLUDED
#define COBALT_UTILITY_UID_HPP_INCLUDED

#pragma once

// Classes in this file:
//     uid

#include <boost/functional/hash.hpp>

#include <string_view>

#define DECLARE_UID(Type) \
	DECLARE_UID_WITH_NAME(Type, #Type)

#define DECLARE_UID_NS(Namespace, Type) \
	DECLARE_UID_WITH_NAME_NS(Namespace, Type, #Type)

#define DECLARE_UID_WITH_NAME_NS(Namespace, Type, Name) \
	DECLARE_UID_WITH_NAME(Type, #Namespace "::" Name)

#define DECLARE_UID_WITH_NAME(Type, Name) \
	inline constexpr const char* uid_name(const Type*) noexcept { return Name; }

#define UIDOF(x) \
	::cobalt::uid::of<x>()

namespace cobalt {
	
/// Unique type identifier
class uid {
public:
	const char* name() const noexcept { return _name; }
	
	size_t hash_value() const noexcept { return boost::hash_value(this); }
	friend size_t hash_value(const uid& uid) noexcept { return uid.hash_value(); }
	
	constexpr bool operator==(const uid& rhs) const noexcept { return this == &rhs; }
	constexpr bool operator!=(const uid& rhs) const noexcept { return this != &rhs; }
	constexpr bool operator<(const uid& rhs) const noexcept { return std::less<const uid*>()(this, &rhs); }
	
	constexpr explicit operator bool() const noexcept { return this != &null(); }
	
	template <typename T>
	static constexpr const uid& of(const T* = nullptr) noexcept { return data<T>::instance; }
	static constexpr const uid& null() noexcept { return data<std::nullptr_t>::instance; }
	
	static const uid& from_string(std::string_view name) noexcept {
		for (auto p = head(); p; p = p->_next) {
			if (name == p->_name)
				return *p;
		}
		return null();
	}
	
private:
	explicit uid(const char* name) noexcept : _name(name), _next(head()) { head() = this; }
	
	uid(const uid&) = delete;
	uid& operator=(const uid&) = delete;
	
	static uid*& head() noexcept {
		static uid* head = nullptr;
		return head;
	}
	
private:
	const char* _name = nullptr;
	uid* _next = nullptr;
	
	template <typename T>
	struct data {
		static uid instance;
	};
};

// Use uid of `nullptr_t` as `null`
inline constexpr const char* uid_name(const std::nullptr_t*) noexcept { return "null"; }

// Make use of ADL name lookup for `uid_name`
// Use DECLARE_UID(Type) for your type if got "No matching function for call to 'uid_name'" compilation error here.
template<typename T> uid uid::data<T>::instance(uid_name(static_cast<const T*>(nullptr)));
	
} // namespace cobalt

#endif // COBALT_UTILITY_UID_HPP_INCLUDED
