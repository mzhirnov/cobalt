#include "catch2/catch.hpp"
#include <cobalt/utility/options.hpp>

using namespace cobalt;

enum class test_options {
	none,
	option1 = bit(0),
	option2 = bit(1),
	option3 = bit(2)
};

enum class test_non_options1 {
	none = 1,
	option1 = bit(0),
	option2 = bit(1),
	option3 = bit(2)
};

enum class test_non_options2 {
	option1 = bit(0),
	option2 = bit(1),
	option3 = bit(2)
};

TEST_CASE("options", "[options]") {
	SECTION("is_options_enum_v") {
		REQUIRE(is_options_enum_v<test_options>);
		REQUIRE_FALSE(is_options_enum_v<test_non_options1>);
		REQUIRE_FALSE(is_options_enum_v<test_non_options2>);
	}
	
	SECTION("has_options") {
		auto options = test_options::option1 | test_options::option3;
		
		REQUIRE(has_options(options, test_options::option1));
		REQUIRE(has_options(options, test_options::option1 | test_options::option3));
		REQUIRE_FALSE(has_options(options, test_options::option2));
	}
}
