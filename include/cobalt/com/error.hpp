#ifndef COBALT_COM_ERROR_HPP_INCLUDED
#define COBALT_COM_ERROR_HPP_INCLUDED

#pragma once

#include <system_error>
#include <cstdio>

// Functions in this file:
//     get_last_error
//     set_last_error

namespace cobalt {
namespace com {

enum class errc {
	success,
	failure,
	no_such_interface,
	no_such_class,
	aggregation_not_supported,
	class_disabled,
	not_implemented
};

namespace detail {

class com_error_category : public std::error_category {
public:
	virtual const char* name() const noexcept override { return "com"; }
	
	virtual std::string message(int ev) const override {
		switch (static_cast<errc>(ev)) {
		case errc::success:
			return "operation succeeded";
		case errc::failure:
			return "operation failed";
		case errc::no_such_interface:
			return "no such interface";
		case errc::no_such_class:
			return "no such class";
		case errc::aggregation_not_supported:
			return "aggregation not supported";
		case errc::class_disabled:
			return "class disabled";
		case errc::not_implemented:
			return "not implemented";
		}
		
		char buffer[32];
		std::snprintf(buffer, sizeof(buffer), "unknown error: 0x%X", ev);
		return buffer;
	}
};

inline std::error_code& last_error_code() noexcept {
	thread_local static std::error_code ec;
	return ec;
}

} // namespace detail

inline const std::error_category& com_category() noexcept {
	static detail::com_error_category instance;
	return instance;
}

inline std::error_code make_error_code(errc e) noexcept {
	return {static_cast<int>(e), com_category()};
}

inline std::error_condition make_error_condition(errc e) noexcept {
	return {static_cast<int>(e), com_category()};
}

inline const std::error_code& get_last_error() noexcept { return detail::last_error_code(); }
inline void set_last_error(const std::error_code& ec) noexcept { detail::last_error_code() = ec; }

} // namespace com
} // namespace cobalt

namespace std {
	template <>
	struct is_error_condition_enum<::cobalt::com::errc> : true_type {};
}

#endif // COBALT_COM_ERROR_HPP_INCLUDED
