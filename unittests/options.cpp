#include "catch2/catch.hpp"
#include <cobalt/utility/options.hpp>

using cobalt::bit;

enum class test_options {
	option1 = bit(0),
	option2 = bit(1),
	option3 = bit(2)
};

DECLARE_OPTIONS_ENUM(test_options)

enum class test_non_options {
	option1 = bit(0),
	option2 = bit(1),
	option3 = bit(2)
};

enum my_options : uint32_t {
	my_option1 = 1,
	my_option2 = 2,
	my_option3 = 4
};

TEST_CASE("options", "[options]") {
	SECTION("is_options_enum_v") {
		REQUIRE(cobalt::is_options_enum_v<test_options>);
		REQUIRE_FALSE(cobalt::is_options_enum_v<test_non_options>);
	}
	
	SECTION("has_options witin enum class") {
		auto options = test_options::option1 | test_options::option3;
		
		REQUIRE(cobalt::has_options(options, test_options::option1));
		REQUIRE(cobalt::has_options(options, test_options::option1 | test_options::option3));
		REQUIRE_FALSE(cobalt::has_options(options, test_options::option2));
	}
	
	SECTION("has_options witin integer") {
		uint32_t options = my_option1 | my_option3;
		
		REQUIRE(cobalt::has_options(options, my_option1));
		REQUIRE(cobalt::has_options(options, my_option1 | my_option3));
		REQUIRE_FALSE(cobalt::has_options(options, my_option2));
	}
}
