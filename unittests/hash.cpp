#include "catch.hpp"
#include <cool/hash.hpp>

using namespace cool;

TEST_CASE("hash") {
	REQUIRE(murmur3_32("Hello, world!", 0) == 3224780355);
	REQUIRE("Hello, world!"_hash == 3224780355);
}
