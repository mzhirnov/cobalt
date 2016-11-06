#ifndef COBALT_UTILITY_HASH_HPP_INCLUDED
#define COBALT_UTILITY_HASH_HPP_INCLUDED

#pragma once

#include <cstdint>

namespace cobalt {
namespace compiletime {
namespace detail {
	
// http://www.elbeno.com/blog/?p=1254

constexpr uint32_t murmur3_32_k(uint32_t k)
{
	return ((static_cast<uint32_t>(k * 0xcc9e2d51ull) << 15)
		| (static_cast<uint32_t>(k * 0xcc9e2d51ull) >> 17)) * 0x1b873593ull;
}

constexpr uint32_t murmur3_32_hashround(uint32_t k, uint32_t hash)
{
	return (((hash ^ k) << 13) | ((hash ^ k) >> 19)) * 5ull + 0xe6546b64;
}

constexpr uint32_t word32le(const char* s, uint32_t len)
{
	return
		(len > 0 ? static_cast<uint32_t>(s[0]) : 0)
		| (len > 1 ? (static_cast<uint32_t>(s[1]) << 8) : 0)
		| (len > 2 ? (static_cast<uint32_t>(s[2]) << 16) : 0)
		| (len > 3 ? (static_cast<uint32_t>(s[3]) << 24) : 0);
}

constexpr uint32_t word32le(const char* s)
{
	return word32le(s, 4);
}

constexpr uint32_t murmur3_32_loop(const char* key, uint32_t len, uint32_t hash)
{
	return len == 0 ? hash :
		murmur3_32_loop(
			key + 4,
			len - 1,
			murmur3_32_hashround(
				murmur3_32_k(word32le(key)), hash));
}

constexpr uint32_t murmur3_32_end0(uint32_t k)
{
	return ((static_cast<uint32_t>(k * 0xcc9e2d51ull) << 15)
		| (static_cast<uint32_t>(k * 0xcc9e2d51ull) >> 17)) * 0x1b873593ull;
}

constexpr uint32_t murmur3_32_end1(uint32_t k, const char* key)
{
	return murmur3_32_end0(k ^ static_cast<uint32_t>(key[0]));
}

constexpr uint32_t murmur3_32_end2(uint32_t k, const char* key)
{
	return murmur3_32_end1(k ^ (static_cast<uint32_t>(key[1]) << 8), key);
}

constexpr uint32_t murmur3_32_end3(uint32_t k, const char* key)
{
	return murmur3_32_end2(k ^ (static_cast<uint32_t>(key[2]) << 16), key);
}

constexpr uint32_t murmur3_32_end(uint32_t hash, const char* key, uint32_t rem)
{
	return rem == 0 ? hash :
		hash ^ (rem == 3 ? murmur3_32_end3(0, key) :
			rem == 2 ? murmur3_32_end2(0, key) :
			murmur3_32_end1(0, key));
}

constexpr uint32_t murmur3_32_final1(uint32_t hash)
{
	return (hash ^ (hash >> 16)) * 0x85ebca6bull;
}

constexpr uint32_t murmur3_32_final2(uint32_t hash)
{
	return (hash ^ (hash >> 13)) * 0xc2b2ae35ull;
}

constexpr uint32_t murmur3_32_final3(uint32_t hash)
{
	return (hash ^ (hash >> 16));
}

constexpr uint32_t murmur3_32_final(uint32_t hash, uint32_t len)
{
	return
		murmur3_32_final3(
			murmur3_32_final2(
				murmur3_32_final1(hash ^ len)));
}

constexpr uint32_t murmur3_32_value(const char* key, uint32_t len, uint32_t seed)
{
	return
		murmur3_32_final(
			murmur3_32_end(
				murmur3_32_loop(key, len / 4, seed),
				key + (len / 4) * 4, len & 3),
			len);
}

constexpr uint32_t fnv1_32(uint32_t h, const char* s)
{
	return *s == 0 ? h : fnv1_32(h * 0x1000193 ^ static_cast<uint32_t>(*s), s + 1);
}

constexpr uint32_t fnv1a_32(uint32_t h, const char* s)
{
	return *s == 0 ? h : fnv1_32((h ^ static_cast<uint32_t>(*s)) * 0x1000193, s + 1);
}

constexpr uint64_t fnv1_64(uint64_t h, const char* s)
{
	return *s == 0 ? h : fnv1_64((h * 0x100000001b3) ^ static_cast<uint64_t>(*s), s + 1);
}

constexpr uint64_t fnv1a_64(uint64_t h, const char* s)
{
	return *s == 0 ? h : fnv1_64((h ^ static_cast<uint64_t>(*s)) * 0x100000001b3, s + 1);
}

constexpr uint32_t length(const char* str)
{
	return *str == 0 ? 0 : 1 + length(str + 1);
}

} // namespace detail

constexpr uint32_t murmur3_32(const char *key, uint32_t seed)
{
	return detail::murmur3_32_value(key, detail::length(key), seed);
}

constexpr uint32_t murmur3_32(const char *key, size_t len, uint32_t seed)
{
	return detail::murmur3_32_value(key, static_cast<uint32_t>(len), seed);
}

constexpr uint32_t fnv1_32(const char* s)
{
	return detail::fnv1_32(0x811c9dc5, s);
}

constexpr uint32_t fnv1a_32(const char* s)
{
	return detail::fnv1a_32(0x811c9dc5, s);
}

constexpr uint64_t fnv1_64(const char* s)
{
	return detail::fnv1_64(0xcbf29ce484222325, s);
}

constexpr uint64_t fnv1a_64(const char* s)
{
	return detail::fnv1a_64(0xcbf29ce484222325, s);
}
	
} // namespace compiletime

namespace runtime {
namespace detail {
	
static inline uint32_t rotl32(uint32_t x, int8_t r) noexcept
{
	return (x << r) | (x >> (32 - r));
}

static inline uint32_t fmix32(uint32_t h) noexcept
{
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;

	return h;
}

static uint32_t murmur3_32_value(const void* key, uint32_t len, uint32_t seed) noexcept
{
	const uint8_t * data = (const uint8_t*)key;
	const int nblocks = len / 4;
		
	uint32_t h1 = seed;
		
	constexpr uint32_t c1 = 0xcc9e2d51;
	constexpr uint32_t c2 = 0x1b873593;
	
	// body
		
	const uint32_t* blocks = (const uint32_t *)(data + nblocks*4);
		
	for (int i = -nblocks; i; i++) {
		uint32_t k1 = blocks[i];

		k1 *= c1;
		k1 = rotl32(k1,15);
		k1 *= c2;

		h1 ^= k1;
		h1 = rotl32(h1,13);
		h1 = h1 * 5+0xe6546b64;
	}

	// tail
		
	const uint8_t* tail = (const uint8_t*)(data + nblocks*4);
		
	uint32_t k1 = 0;
		
	switch(len & 3) {
	case 3: k1 ^= tail[2] << 16;
	case 2: k1 ^= tail[1] << 8;
	case 1: k1 ^= tail[0];
			k1 *= c1; k1 = rotl32(k1,15); k1 *= c2; h1 ^= k1;
	}
	
	// finalization
		
	h1 ^= len;
		
	return fmix32(h1);
}

} // namespace detail
	
inline uint32_t murmur3_32(const char *key, uint32_t seed) noexcept
{
	return detail::murmur3_32_value(key, static_cast<uint32_t>(std::strlen(key)), seed);
}

inline uint32_t murmur3_32(const char *key, size_t len, uint32_t seed) noexcept
{
	return detail::murmur3_32_value(key, static_cast<uint32_t>(len), seed);
}
	
} // namespace runtime

typedef uint32_t hash_type;

constexpr hash_type operator ""_hash(const char* str, size_t len)
{
	return cobalt::compiletime::murmur3_32(str, static_cast<uint32_t>(len), 0);
}

inline hash_type murmur3(const char *str, uint32_t seed) noexcept
{
	return cobalt::runtime::murmur3_32(str, seed);
}

inline hash_type murmur3(const char *str, size_t len, uint32_t seed) noexcept
{
	return cobalt::runtime::murmur3_32(str, static_cast<uint32_t>(len), seed);
}

template <typename T>
struct dont_hash {
	typedef T argument_type;
	typedef T result_type;

	constexpr result_type operator()(argument_type v) const noexcept {
		return v;
	}
};
	
} // namespace cobalt

#endif // COBALT_UTILITY_HASH_HPP_INCLUDED
