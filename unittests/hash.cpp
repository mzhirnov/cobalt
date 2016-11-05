#include "catch.hpp"
#include <cobalt/hash.hpp>

TEST_CASE("hash") {
	SECTION("compile time") {
		static_assert(cobalt::compiletime::murmur3_32("Hello, world!", 0) == 3224780355, "");
		static_assert("Hello, world!"_hash == 3224780355, "");
		
		REQUIRE(cobalt::compiletime::murmur3_32("Hello, world!", 0) == 3224780355);
		REQUIRE(cobalt::compiletime::murmur3_32("Hello, world!", 13, 0) == 3224780355);
		REQUIRE("Hello, world!"_hash == 3224780355);
	}
	
	SECTION("runtime") {
		std::string str("Hello, world!");
		
		REQUIRE(cobalt::runtime::murmur3_32(str.c_str(), 0) == 3224780355);
		REQUIRE(cobalt::runtime::murmur3_32(str.c_str(), str.size(), 0) == 3224780355);
		REQUIRE(cobalt::runtime::murmur3_32("Hello, world!", 0) == 3224780355);
	}
}
