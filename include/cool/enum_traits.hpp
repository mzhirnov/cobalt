#pragma once

#include <cstdint>
#include <cstring>

#include <type_traits>
#include <ostream>
#include <sstream>

#include <boost/preprocessor/variadic/size.hpp>
#include <boost/assert.hpp>

template <typename T>
std::string type_name() {
	return detail::type_name_helper<T>::name();
}

struct enum_item_info {
	const char* name;
	size_t name_length;
	size_t value;
};

template <typename Enum>
struct enum_traits {
	static const bool is_enum = false;
};


#define DEFINE_ENUM_STRUCT(struct_name, enum_name, enum_type, ...)                    \
	struct struct_name {                                                              \
		enum enum_name : enum_type {                                                  \
			__VA_ARGS__                                                               \
		};                                                                            \
	};                                                                                \
	DEFINE_ENUM_TRAITS(struct_name, __VA_ARGS__)                                      \


#define DEFINE_ENUM_STRUCT_FLAGS(struct_name, enum_name, enum_type, ...)              \
	struct struct_name {                                                              \
		enum enum_name : enum_type {                                                  \
			__VA_ARGS__                                                               \
		};                                                                            \
	};                                                                                \
	DEFINE_ENUM_TRAITS_FLAGS(struct_name, __VA_ARGS__)                                \
	DEFINE_ENUM_FLAGS_OPERATORS(struct_name::enum_name)                               \


#define DEFINE_ENUM_CLASS(enum_name, enum_type, ...)                                  \
	enum class enum_name : enum_type {                                                \
		__VA_ARGS__                                                                   \
	};                                                                                \
	DEFINE_ENUM_TRAITS(enum_name, __VA_ARGS__)                                        \
	DEFINE_ENUM_OSTREAM_OPERATOR(enum_name)                                           \


#define DEFINE_ENUM_CLASS_FLAGS(enum_name, enum_type, ...)                            \
	enum class enum_name : enum_type {                                                \
		__VA_ARGS__                                                                   \
	};                                                                                \
	DEFINE_ENUM_TRAITS_FLAGS(enum_name, __VA_ARGS__)                                  \
	DEFINE_ENUM_FLAGS_OPERATORS(enum_name)                                            \
	DEFINE_ENUM_OSTREAM_OPERATOR(enum_name)                                           \


#define DEFINE_ENUM_TRAITS(enum_name, ...)                                            \
	template<> struct enum_traits<enum_name> {                                        \
		static const bool is_enum = true;                                             \
		static const bool is_flags = false;                                           \
		static const size_t num_items = BOOST_PP_VARIADIC_SIZE(__VA_ARGS__);          \
		static const enum_item_info* const items() {                                  \
			static enum_item_info __info[num_items + 1];                              \
			for (static volatile bool __init = false; !__init && (__init = true); ) { \
				detail::auto_enum_value<enum_name> __VA_ARGS__;                       \
				size_t __values[] = { __VA_ARGS__ };                                  \
				detail::helper<>::parse_enum_values(__info, #__VA_ARGS__, __values);  \
			}                                                                         \
			return __info;                                                            \
		}                                                                             \
		static std::string to_string(size_t value)                                    \
			{ return detail::helper<>::to_string(items(), value); }                   \
		static size_t from_string(const char* s, size_t length = 0)                   \
			{ return detail::helper<>::from_string(items(), s, length); }             \
	};                                                                                \


#define DEFINE_ENUM_TRAITS_FLAGS(enum_name, ...)                                      \
	template<> struct enum_traits<enum_name> {                                        \
		static const bool is_enum = true;                                             \
		static const bool is_flags = true;                                            \
		static const size_t num_items = BOOST_PP_VARIADIC_SIZE(__VA_ARGS__);          \
		static const enum_item_info* const items() {                                  \
			static enum_item_info __info[num_items + 1];                              \
			for (static volatile bool __init = false; !__init && (__init = true); ) { \
				detail::auto_enum_value<enum_name> __VA_ARGS__;                       \
				size_t __values[] = { __VA_ARGS__ };                                  \
				detail::helper<>::parse_enum_values(__info, #__VA_ARGS__, __values);  \
			}                                                                         \
			return __info;                                                            \
		}                                                                             \
		static std::string to_string(size_t value)                                    \
			{ return detail::helper<>::to_flags_string(items(), value); }             \
		static size_t from_string(const char* s, size_t length = 0)                   \
			{ return detail::helper<>::from_flags_string(items(), s, length); }       \
	};                                                                                \


#define DEFINE_ENUM_FLAGS_OPERATORS(enum_name) \
	constexpr enum_name operator~(enum_name elem) noexcept \
		{ return static_cast<enum_name>(~static_cast<std::underlying_type<enum_name>::type>(elem)); } \
	constexpr enum_name operator|(enum_name lhs, enum_name rhs) noexcept \
		{ return static_cast<enum_name>(static_cast<std::underlying_type<enum_name>::type>(lhs)|static_cast<std::underlying_type<enum_name>::type>(rhs)); } \
	constexpr enum_name operator&(enum_name lhs, enum_name rhs) noexcept \
		{ return static_cast<enum_name>(static_cast<std::underlying_type<enum_name>::type>(lhs)&static_cast<std::underlying_type<enum_name>::type>(rhs)); } \
	constexpr enum_name operator^(enum_name lhs, enum_name rhs) noexcept \
		{ return static_cast<enum_name>(static_cast<std::underlying_type<enum_name>::type>(lhs)^static_cast<std::underlying_type<enum_name>::type>(rhs)); } \
	inline enum_name& operator|=(enum_name& lhs, enum_name rhs) noexcept \
		{ return reinterpret_cast<enum_name&>(reinterpret_cast<std::underlying_type<enum_name>::type&>(lhs)|=static_cast<std::underlying_type<enum_name>::type>(rhs)); } \
	inline enum_name& operator&=(enum_name& lhs, enum_name rhs) noexcept \
		{ return reinterpret_cast<enum_name&>(reinterpret_cast<std::underlying_type<enum_name>::type&>(lhs)&=static_cast<std::underlying_type<enum_name>::type>(rhs)); } \
	inline enum_name& operator^=(enum_name& lhs, enum_name rhs) noexcept \
		{ return reinterpret_cast<enum_name&>(reinterpret_cast<std::underlying_type<enum_name>::type&>(lhs)^=static_cast<std::underlying_type<enum_name>::type>(rhs)); } \


#define DEFINE_ENUM_OSTREAM_OPERATOR(enum_name)                                       \
	inline std::ostream& operator<<(std::ostream& os, enum_name e) {                  \
		os << enum_traits<enum_name>::to_string(static_cast<size_t>(e));              \
		return os;                                                                    \
	}                                                                                 \

///////////////////////////////////////////////////////////////////////////////

namespace detail {

template <typename Tag, typename IntType = size_t>
struct auto_enum_value {
	static IntType counter;

	auto_enum_value(IntType v) : val(v) { counter = val + 1; }
	auto_enum_value() : val(counter) { counter = val + 1; }

	operator IntType() const { return val; }

	IntType val;
};

template <typename Tag, typename IntType>
IntType auto_enum_value<Tag, IntType>::counter;

template <int Dummy = 0>
struct helper {
	// Handler(const char* name, size_t length)
	template <typename Handler>
	static void parse_enum_values_helper(const char* str, char separator, Handler handler) {
		auto is_space = [](char c) { return c <= 32; };
		auto is_identifier = [](char c) { return c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c >= '0' && c <= '9' || c == '_'; };

		for (const char* b = str, *e = strchr(b, separator); b; (b = e) ? e = strchr(++b, separator) : (void)0) {
			while (is_space(*b))
				++b;

			const char* name = b;

			while (is_identifier(*b))
				++b;

			handler(name, b - name);
		}
	}

	static void parse_enum_values(enum_item_info* info, const char* str, size_t* values) {
		size_t i = 0;
		parse_enum_values_helper(str, ',', [&i, info, values](const char* name, size_t length) {
			info[i].name = name;
			info[i].name_length = length;
			info[i].value = values[i];
			i++;
		});
		info[i].name = nullptr;
		info[i].value = 0;
	}

	static std::string to_string(const enum_item_info* const info, size_t value) {
		for (auto item = info; item->name; ++item) {
			if (value == item->value) {
				return std::string(item->name, item->name_length);
			}
		}
		BOOST_ASSERT_MSG(false, "Couldn't find such a value");
		return std::string();
	}

	static std::string to_flags_string(const enum_item_info* const info, size_t value) {
		if (!value)
			return to_string(info, value);

		std::ostringstream ss;

		for (auto item = info; item->name; ++item) {
			if (item->value && (value & item->value) == item->value) {
				value &= ~item->value;
				if (ss.tellp())
					ss << '|';
				ss.rdbuf()->sputn(item->name, item->name_length);
			}
		}

		BOOST_ASSERT_MSG(!value, "Unknown value found");

		// append unknown values as single hex
		if (value) {
			if (ss.tellp())
				ss << '|';
			ss << std::hex << std::showbase << value;
		}

		return ss.str();
	}

	static size_t from_string(const enum_item_info* const info, const char* name, size_t length = 0) {
		if (!length)
			length = std::strlen(name);

		for (auto item = info; item->name; ++item) {
			if (item->name_length == length && !std::strncmp(item->name, name, length))
				return item->value;
		}

		BOOST_ASSERT_MSG(false, "Couldn't find such a value");
		return 0;
	}

	static size_t from_flags_string(const enum_item_info* const info, const char* str, size_t /*length*/ = 0) {
		size_t value = 0;
		parse_enum_values_helper(str, '|', [&value, info](const char* name, size_t length) {
			value |= from_string(info, name, length);
		});
		return value;
	}
};

template <typename T>
struct type_name_helper {
	static std::string name() {
		constexpr size_t length = sizeof(__FUNCTION__) - sizeof("detail::type_name_helper<>::name");
		constexpr size_t offset = sizeof("detail::type_name_helper<") - 1;
		// TODO: Use constexpr substring here
		return std::string(__FUNCTION__ + offset, length);
	}
};

} // namespace detail
