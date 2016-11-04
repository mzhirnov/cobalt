#include "catch.hpp"
#include <cobalt/object.hpp>

using namespace cobalt;

TEST_CASE("object") {
	ref_ptr<object> o = new object();
	
	SECTION("add child") {
		auto child = o->add_child(new object());
	
		REQUIRE(child->parent() == o.get());
	}
}
