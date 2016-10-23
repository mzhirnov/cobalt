#pragma once

// Classes in this file:
//     tpoint<>
//     tsize<>
//     trect<>

template<typename T>
struct tpoint {
	T x, y;

	tpoint() : x(), y() {}
	tpoint(T x, T y) : x(x), y(y) {}
	
	template<typename U>
	tpoint(const tpoint<U>& p)
		: x(static_cast<T>(p.x)), y(static_cast<T>(p.y))
	{}
};

typedef tpoint<int> point;
typedef tpoint<float> pointf;

template<typename T>
struct tsize {
	T width, height;

	tsize() : width(), height() {}
	tsize(T width, T height) : width(width), height(height) {}
	
	template<typename U>
	tsize(const tsize<U>& s)
		: width(static_cast<T>(s.width)), height(static_cast<T>(s.height))
	{}
};

typedef tsize<int> size;
typedef tsize<float> sizef;

template<typename T>
struct trect {
	T x, y, width, height;

	trect() : x(), y(), width(), height() {}
	trect(T width, T height) : x(), y(), width(width), height(height) {}
	trect(T x, T y, T width, T height) : x(x), y(y), width(width), height(height) {}
	trect(T x, T y, const tsize<T>& s) : x(x), y(y), width(s.width), height(s.height) {}
	trect(const tpoint<T>& p, T width, T height) : x(p.x), y(p.y), width(width), height(height) {}
	trect(const tpoint<T>& p, const tsize<T>& s) : x(p.x), y(p.y), width(s.width), height(s.height) {}

	template<typename U>
	trect(const trect<U>& r)
		: x(static_cast<T>(r.x)), y(static_cast<T>(r.y)), width(static_cast<T>(r.width)), height(static_cast<T>(r.height))
	{}

	friend trect operator + (const trect<T>& r, const tpoint<T>& p) {
		return trect(r.x + p.x, r.y + p.y, r.width, r.height);
	}

	friend trect operator - (const trect<T>& r, const tpoint<T>& p) {
		return trect(r.x - p.x, r.y - p.y, r.width, r.height);
	}

	friend trect operator + (const trect<T>& r, const tsize<T>& s) {
		return trect(r.x, r.y, r.width + s.width, r.height + s.height);
	}

	friend trect operator - (const trect<T>& r, const tsize<T>& s) {
		return trect(r.x, r.y, r.width - s.width, r.height - s.height);
	}
};

typedef trect<int> rect;
typedef trect<float> rectf;
