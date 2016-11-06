#include "catch.hpp"
#include <cobalt/hash.hpp>

using namespace cobalt;

TEST_CASE("hash") {
	SECTION("compile time") {
		static_assert(compiletime::murmur3_32("Hello, world!", 0) == 3224780355, "");
		static_assert("Hello, world!"_hash == 3224780355, "");
		
		REQUIRE(compiletime::murmur3_32("Hello, world!", 0) == 3224780355);
		REQUIRE(compiletime::murmur3_32("Hello, world!", 13, 0) == 3224780355);
		REQUIRE("Hello, world!"_hash == 3224780355);
	}
	
	SECTION("runtime") {
		std::string str("Hello, world!");
		
		REQUIRE(murmur3(str.c_str(), 0) == 3224780355);
		REQUIRE(murmur3(str.c_str(), str.size(), 0) == 3224780355);
		REQUIRE(murmur3("Hello, world!", 0) == 3224780355);
	}
}
