#include "catch.hpp"
#include <cobalt/io.hpp>

using namespace cobalt;

TEST_CASE("io") {
	SECTION("dynamic memory_stream") {
		io::memory_stream stream;
	}
	
	SECTION("static memory_stream") {
		char buffer[] = "Hello, world!";
		io::memory_stream stream(buffer, sizeof(buffer) - 1);
	}
}
