#include "catch.hpp"
#include <cobalt/utility/static_initializer.hpp>

using namespace cobalt;

class my_class {
	DECLARE_STATIC_INITIALIZER(my_class)
	
public:
	static int initialized;
	
private:
	static void static_initialize() { ++initialized; }
	static void static_uninitialize() { --initialized; }
};

int my_class::initialized = 0;

TEST_CASE("static_initializer") {
	SECTION("w/o instances") {
		REQUIRE(my_class::initialized == 1);
	}
	
	SECTION("with an instance") {
		my_class inst;
		
		REQUIRE(my_class::initialized == 1);
	}
}
