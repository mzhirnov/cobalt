#include "catch.hpp"
#include <cobalt/hash.hpp>

TEST_CASE("hash") {
	REQUIRE(cobalt::murmur3_32("Hello, world!", 0) == 3224780355);
	REQUIRE("Hello, world!"_hash == 3224780355);
}
