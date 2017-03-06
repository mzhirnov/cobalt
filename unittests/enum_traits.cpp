#include "catch.hpp"
#include <cobalt/utility/enum_traits.hpp>

#include <sstream>

CO_DEFINE_ENUM(
	enum_class, uint32_t,
	value1,
	value2 = 42,
	value3
)

CO_DEFINE_ENUM_FLAGS(
	enum_flags, uint32_t,
	value1 = 1 << 0,
	value2 = 1 << 1,
	value3 = 1 << 2,
	value4 = value2 | value3
)

TEST_CASE("enum class") {
	SECTION("enum traits") {
		REQUIRE(cobalt::enum_traits<enum_class>::is_enum == true);
		REQUIRE(cobalt::enum_traits<enum_class>::is_flags == false);
	}
	
	SECTION("items") {
		REQUIRE(cobalt::enum_traits<enum_class>::num_items == 3);
		
		auto items = cobalt::enum_traits<enum_class>::items();
		
		REQUIRE(items[0].name() == "value1");
		REQUIRE(items[1].name() == "value2");
		REQUIRE(items[2].name() == "value3");
		
		REQUIRE(items[0].value() == static_cast<size_t>(enum_class::value1));
		REQUIRE(items[1].value() == static_cast<size_t>(enum_class::value2));
		REQUIRE(items[2].value() == static_cast<size_t>(enum_class::value3));
	}
	
	SECTION("to string") {
		std::ostringstream oss;
		oss << enum_class::value3;
		REQUIRE(oss.str() == "value3");
		
		REQUIRE(cobalt::enum_traits<enum_class>::to_string(enum_class::value3) == "value3");
	}
	
	SECTION("from string") {
		REQUIRE(cobalt::enum_traits<enum_class>::from_string("value2") == enum_class::value2);
		REQUIRE(cobalt::enum_traits<enum_class>::from_string(" value2") == enum_class::value2);
		REQUIRE(cobalt::enum_traits<enum_class>::from_string("value2 ") == enum_class::value2);
		REQUIRE(cobalt::enum_traits<enum_class>::from_string(" value2 ") == enum_class::value2);
	}
}

TEST_CASE("enum flags") {
	SECTION("flags traits") {
		REQUIRE(cobalt::enum_traits<enum_flags>::is_enum == true);
		REQUIRE(cobalt::enum_traits<enum_flags>::is_flags == true);
	}
	
	SECTION("items") {
		REQUIRE(cobalt::enum_traits<enum_flags>::num_items == 4);
		
		auto items = cobalt::enum_traits<enum_flags>::items();
		
		REQUIRE(items[0].name() == "value1");
		REQUIRE(items[1].name() == "value2");
		REQUIRE(items[2].name() == "value3");
		REQUIRE(items[3].name() == "value4");
		
		REQUIRE(items[0].value() == static_cast<size_t>(enum_flags::value1));
		REQUIRE(items[1].value() == static_cast<size_t>(enum_flags::value2));
		REQUIRE(items[2].value() == static_cast<size_t>(enum_flags::value3));
		REQUIRE(items[3].value() == static_cast<size_t>(enum_flags::value4));
	}
	
	SECTION("flags to string") {
		std::ostringstream oss;
		oss << (enum_flags::value1|enum_flags::value3);
		REQUIRE(oss.str() == "value1|value3");
		
		REQUIRE(cobalt::enum_traits<enum_flags>::to_string(enum_flags::value1|enum_flags::value3) == "value1|value3");
		REQUIRE(cobalt::enum_traits<enum_flags>::to_string(enum_flags::value4) == "value2|value3");
	}
	
	SECTION("flags from string") {
		REQUIRE(cobalt::enum_traits<enum_flags>::from_string("value1|value2") == (enum_flags::value1|enum_flags::value2));
		REQUIRE(cobalt::enum_traits<enum_flags>::from_string(" value1|value2 ") == (enum_flags::value1|enum_flags::value2));
		REQUIRE(cobalt::enum_traits<enum_flags>::from_string("value1 | value2") == (enum_flags::value1|enum_flags::value2));
		REQUIRE(cobalt::enum_traits<enum_flags>::from_string(" value1 | value2 ") == (enum_flags::value1|enum_flags::value2));
	}
}
