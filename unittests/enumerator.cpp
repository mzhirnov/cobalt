#include "catch2/catch.hpp"
#include <cobalt/utility/enumerator.hpp>

#include <vector>
#include <algorithm>

namespace {

template <typename Enumerator>
inline void test_enumerator(Enumerator&& e, size_t count) {
	REQUIRE_FALSE(e.begin() == e.end());
	REQUIRE(*e.begin() == 1);
	REQUIRE(std::distance(e.begin(), e.end()) == count);
}

} // namespace

TEST_CASE("enumerator", "[enumerator]") {
	char array[5] = { 1, 2, 3, 4, 5 };
	constexpr size_t count = sizeof(array)/sizeof(array[0]);
	
	SECTION("for C array") {
		test_enumerator(cobalt::make_enumerator(array), sizeof(array)/sizeof(array[0]));
	}
	
	SECTION("for two pointers") {
		char* begin = &array[0];
		char* end = begin + count;
		
		test_enumerator(cobalt::make_enumerator(begin, end), count);
	}
	
	SECTION("for two const pointers") {
		const char* begin = &array[0];
		const char* end = begin + count;
		
		test_enumerator(cobalt::make_enumerator(begin, end), count);
	}
	
	SECTION("for std::vector") {
		std::vector<char> vec = { 1, 2, 3, 4, 5 };
		
		test_enumerator(cobalt::make_enumerator(vec), vec.size());
	}
	
	SECTION("for const std::vector") {
		const std::vector<char> vec = { 1, 2, 3, 4, 5 };
		
		test_enumerator(cobalt::make_enumerator(vec), vec.size());
	}
}
