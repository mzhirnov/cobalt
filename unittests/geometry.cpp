#include "catch2/catch.hpp"
#include <cobalt/geometry.hpp>

using namespace cobalt;

TEST_CASE("geometry", "[geometry]") {
	rect r{1, 1, 10, 10};
	
	SECTION("rect extents") {
		REQUIRE(r.origin() == (point(1, 1)));
		REQUIRE(r.size() == (size(10, 10)));
	}
	
	SECTION("rect operators") {
		REQUIRE((r + point(1, 2)) == (rect(2, 3, 10, 10)));
		REQUIRE((r - point(1, 2)) == (rect(0, -1, 10, 10)));
		
		REQUIRE((r + size(1, 2)) == (rect(1, 1, 11,12)));
		REQUIRE((r - size(1, 2)) == (rect(1, 1, 9, 8)));
		
		r += point(1, 2);
		
		REQUIRE(r == (rect(2, 3, 10, 10)));
		
		r -= point(1, 2);
		
		REQUIRE(r == (rect(1, 1, 10, 10)));
		
		r += size(1, 2);
		
		REQUIRE(r == (rect(1, 1, 11,12)));
		
		r -= size(1, 2);
		
		REQUIRE(r == (rect(1, 1, 10, 10)));
	}
}
