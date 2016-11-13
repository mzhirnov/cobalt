#include "catch.hpp"
#include <cobalt/utility/enum_traits.hpp>

#include <sstream>

CO_DEFINE_ENUM_CLASS(
	enum_class, uint32_t,
	value1,
	value2 = 42,
	value3
)

CO_DEFINE_ENUM_FLAGS_CLASS(
	enum_flags, uint32_t,
	value1 = 1 << 0,
	value2 = 1 << 1,
	value3 = 1 << 2
)

CO_DEFINE_ENUM_STRUCT(
	enum_struct, type, uint32_t,
	value1,
	value2,
	value3
)

CO_DEFINE_ENUM_FLAGS_STRUCT(
	struct_flags, type, uint32_t,
	value1 = 1 << 0,
	value2 = 1 << 1,
	value3 = 1 << 2
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
	
	SECTION("flags enum traits") {
		REQUIRE(cobalt::enum_traits<enum_flags>::is_enum == true);
		REQUIRE(cobalt::enum_traits<enum_flags>::is_flags == true);
		REQUIRE(cobalt::enum_traits<enum_flags>::num_items == 3);
	}
	
	SECTION("flags to string") {
		std::ostringstream oss;
		oss << (enum_flags::value1|enum_flags::value3);
		REQUIRE(oss.str() == "value1|value3");
		
		REQUIRE(cobalt::enum_traits<enum_flags>::to_string(enum_flags::value1|enum_flags::value3) == "value1|value3");
	}
	
	SECTION("flags from string") {
		REQUIRE(cobalt::enum_traits<enum_flags>::from_string("value1|value2") == (enum_flags::value1|enum_flags::value2));
		REQUIRE(cobalt::enum_traits<enum_flags>::from_string(" value1|value2 ") == (enum_flags::value1|enum_flags::value2));
		REQUIRE(cobalt::enum_traits<enum_flags>::from_string("value1 | value2") == (enum_flags::value1|enum_flags::value2));
		REQUIRE(cobalt::enum_traits<enum_flags>::from_string(" value1 | value2 ") == (enum_flags::value1|enum_flags::value2));
	}
}

TEST_CASE("enum struct") {
	SECTION("enum traits") {
		REQUIRE(cobalt::enum_traits<enum_struct>::is_enum == true);
		REQUIRE(cobalt::enum_traits<enum_struct>::is_flags == false);
	}
	
	SECTION("items") {
		REQUIRE(cobalt::enum_traits<enum_struct>::num_items == 3);
		
		auto items = cobalt::enum_traits<enum_struct>::items();
		
		REQUIRE(items[0].name() == "value1");
		REQUIRE(items[1].name() == "value2");
		REQUIRE(items[2].name() == "value3");
		
		REQUIRE(items[0].value() == enum_struct::value1);
		REQUIRE(items[1].value() == enum_struct::value2);
		REQUIRE(items[2].value() == enum_struct::value3);
	}
	
	SECTION("to string") {
		std::ostringstream oss;
		oss << enum_struct::value3;
		REQUIRE(oss.str() == "2");
		
		REQUIRE(cobalt::enum_traits<enum_struct>::to_string(enum_struct::value3) == "value3");
	}
	
	SECTION("from string") {
		REQUIRE(cobalt::enum_traits<enum_struct>::from_string("value2") == enum_struct::value2);
		REQUIRE(cobalt::enum_traits<enum_struct>::from_string(" value2") == enum_struct::value2);
		REQUIRE(cobalt::enum_traits<enum_struct>::from_string("value2 ") == enum_struct::value2);
		REQUIRE(cobalt::enum_traits<enum_struct>::from_string(" value2 ") == enum_struct::value2);
	}
	
	SECTION("flags enum traits") {
		REQUIRE(cobalt::enum_traits<struct_flags>::is_enum == true);
		REQUIRE(cobalt::enum_traits<struct_flags>::is_flags == true);
		REQUIRE(cobalt::enum_traits<struct_flags>::num_items == 3);
	}
	
	SECTION("flags to string") {
		std::ostringstream oss;
		oss << (struct_flags::value1|struct_flags::value3);
		REQUIRE(oss.str() == "5");
		
		REQUIRE(cobalt::enum_traits<struct_flags>::to_string(struct_flags::value1|struct_flags::value3) == "value1|value3");
	}
	
	SECTION("flags from string") {
		REQUIRE(cobalt::enum_traits<struct_flags>::from_string("value1|value2") == (struct_flags::value1|struct_flags::value2));
		REQUIRE(cobalt::enum_traits<struct_flags>::from_string(" value1|value2 ") == (struct_flags::value1|struct_flags::value2));
		REQUIRE(cobalt::enum_traits<struct_flags>::from_string("value1 | value2") == (struct_flags::value1|struct_flags::value2));
		REQUIRE(cobalt::enum_traits<struct_flags>::from_string(" value1 | value2 ") == (struct_flags::value1|struct_flags::value2));
	}
}
