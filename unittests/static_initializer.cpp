#include "catch.hpp"
#include <cobalt/utility/static_initializer.hpp>

using namespace cobalt;

class my_class {
	CO_STATIC_INITIALIZE(my_class)
	
public:
	static int initialized;
	
private:
	static void static_initialize() { ++initialized; puts(__PRETTY_FUNCTION__); }
	static void static_uninitialize() { --initialized; puts(__PRETTY_FUNCTION__); }
};

int my_class::initialized = 0;

class my_class2 : public static_initializable<my_class2> {
public:
	static int initialized;
	
	static void static_initialize() { ++initialized; puts(__PRETTY_FUNCTION__); }
	static void static_uninitialize() { --initialized; puts(__PRETTY_FUNCTION__); }
};

int my_class2::initialized = 0;

TEST_CASE("static_initializer with macros") {
	SECTION("w/o instances") {
		REQUIRE(my_class::initialized == 1);
	}
	
	SECTION("with an instance") {
		my_class inst;
		
		REQUIRE(my_class::initialized == 1);
	}
}

TEST_CASE("static_initializer with inheritance") {
	SECTION("w/o instances") {
		REQUIRE(my_class2::initialized == 1);
	}
	
	SECTION("with an instance") {
		my_class2 inst;
		
		REQUIRE(my_class2::initialized == 1);
	}
}
