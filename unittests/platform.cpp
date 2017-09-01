#include "catch.hpp"
#include <cobalt/platform.hpp>

TEST_CASE("platform", "[platform]") {
	$macos(REQUIRE(true));
	$not_macos(REQUIRE(false));
}
