#include "catch.hpp"
#include <cobalt/hash.hpp>

TEST_CASE("hash") {
	static_assert(cobalt::murmur3_32("Hello, world!", 0) == 3224780355, "");
	static_assert("Hello, world!"_hash == 3224780355, "");
	
	REQUIRE(cobalt::murmur3_32("Hello, world!", 0) == 3224780355);
	REQUIRE("Hello, world!"_hash == 3224780355);
}
