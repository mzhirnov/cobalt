#ifndef COBALT_GEOMETRY_HPP_INCLUDED
#define COBALT_GEOMETRY_HPP_INCLUDED

#pragma once

#include <type_traits>
#include <ostream>

// Classes in this file:
//     basic_point<>
//     basic_size<>
//     basic_rect<>

namespace cobalt {

template<typename T, typename = typename std::enable_if_t<std::is_arithmetic<T>::value>>
struct basic_point {
	T x{};
	T y{};

	constexpr basic_point() noexcept = default;
	constexpr basic_point(T x, T y) noexcept
		: x(x), y(y) {}
	
	template<typename U>
	constexpr basic_point(const basic_point<U>& p) noexcept
		: x(static_cast<T>(p.x))
		, y(static_cast<T>(p.y))
	{}
	
	friend bool operator==(const basic_point& lhs, const basic_point& rhs) noexcept {
		return lhs.x == rhs.x && lhs.y == rhs.y;
	}
	
	friend bool operator!=(const basic_point& lhs, const basic_point& rhs) noexcept {
		return lhs.x != rhs.x || lhs.y != rhs.y;
	}
	
	template <typename CharT>
	friend std::basic_ostream<CharT>& operator<<(std::basic_ostream<CharT>& os, const basic_point& p) {
		os << '{' << p.x << ';' << p.y << '}';
		return os;
	}
};

using point = basic_point<int>;
using pointf = basic_point<float>;

template<typename T, typename = typename std::enable_if_t<std::is_arithmetic<T>::value>>
struct basic_size {
	T width{};
	T height{};

	constexpr basic_size() noexcept = default;
	constexpr basic_size(T width, T height) noexcept
		: width(width), height(height) {}
	
	template<typename U>
	constexpr basic_size(const basic_size<U>& s) noexcept
		: width(static_cast<T>(s.width))
		, height(static_cast<T>(s.height))
	{}
	
	friend bool operator==(const basic_size& lhs, const basic_size& rhs) noexcept {
		return lhs.width == rhs.width && lhs.height == rhs.height;
	}
	
	friend bool operator!=(const basic_size& lhs, const basic_size& rhs) noexcept {
		return lhs.width != rhs.width || lhs.height != rhs.height;
	}
	
	template <typename CharT>
	friend std::basic_ostream<CharT>& operator<<(std::basic_ostream<CharT>& os, const basic_size& s) {
		os << '{' << s.width << ';' << s.height << '}';
		return os;
	}
};

using size = basic_size<int>;
using sizef = basic_size<float>;

template<typename T, typename = typename std::enable_if_t<std::is_arithmetic<T>::value>>
struct basic_rect {
	T x{};
	T y{};
	T width{};
	T height{};

	constexpr basic_rect() noexcept = default;
	constexpr basic_rect(T width, T height) noexcept
		: width(width), height(height) {}
	
	constexpr basic_rect(T x, T y, T width, T height) noexcept
		: x(x), y(y), width(width), height(height) {}
	
	constexpr basic_rect(T x, T y, const basic_size<T>& s) noexcept
		: x(x), y(y), width(s.width), height(s.height) {}
	
	constexpr basic_rect(const basic_point<T>& p, T width, T height) noexcept
		: x(p.x), y(p.y), width(width), height(height) {}
	
	constexpr basic_rect(const basic_point<T>& p, const basic_size<T>& s) noexcept
		: x(p.x), y(p.y), width(s.width), height(s.height) {}

	template<typename U>
	constexpr basic_rect(const basic_rect<U>& r) noexcept
		: x(static_cast<T>(r.x))
		, y(static_cast<T>(r.y))
		, width(static_cast<T>(r.width))
		, height(static_cast<T>(r.height))
	{}
	
	basic_point<T> origin() const noexcept { return {x, y}; }
	
	basic_size<T> size() const noexcept { return {width, height}; }
	
	friend bool operator==(const basic_rect& lhs, const basic_rect& rhs) noexcept {
		return lhs.x == rhs.x && lhs.y == rhs.y && lhs.width == rhs.width && lhs.height == rhs.height;
	}
	
	friend bool operator!=(const basic_rect& lhs, const basic_rect& rhs) noexcept {
		return lhs.x != rhs.x || lhs.y != rhs.y || lhs.width != rhs.width || lhs.height != rhs.height;
	}

	friend basic_rect operator+(const basic_rect<T>& r, const basic_point<T>& p) noexcept {
		return {r.x + p.x, r.y + p.y, r.width, r.height};
	}
	
	friend basic_rect& operator+=(basic_rect<T>& r, const basic_point<T>& p) noexcept {
		r.x += p.x;
		r.y += p.y;
		return r;
	}

	friend basic_rect operator-(const basic_rect<T>& r, const basic_point<T>& p) noexcept {
		return {r.x - p.x, r.y - p.y, r.width, r.height};
	}
	
	friend basic_rect& operator-=(basic_rect<T>& r, const basic_point<T>& p) noexcept {
		r.x -= p.x;
		r.y -= p.y;
		return r;
	}

	friend basic_rect operator+(const basic_rect<T>& r, const basic_size<T>& s) noexcept {
		return {r.x, r.y, r.width + s.width, r.height + s.height};
	}
	
	friend basic_rect& operator+=(basic_rect<T>& r, const basic_size<T>& s) noexcept {
		r.width += s.width;
		r.height += s.height;
		return r;
	}

	friend basic_rect operator-(const basic_rect<T>& r, const basic_size<T>& s) noexcept {
		return {r.x, r.y, r.width - s.width, r.height - s.height};
	}
	
	friend basic_rect& operator-=(basic_rect<T>& r, const basic_size<T>& s) noexcept {
		r.width -= s.width;
		r.height -= s.height;
		return r;
	}
	
	template <typename CharT>
	friend std::basic_ostream<CharT>& operator<<(std::basic_ostream<CharT>& os, const basic_rect& r) {
		os << '{' << r.x << ';' << r.y << ';' << r.width << ';' << r.height << '}';
		return os;
	}
};

using rect = basic_rect<int>;
using rectf = basic_rect<float>;

} // namespace cobalt

#endif // COBALT_GEOMETRY_HPP_INCLUDED
