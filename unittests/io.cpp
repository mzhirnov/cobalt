#include "catch.hpp"
#include <cobalt/io.hpp>

using namespace cobalt;

static void test_read_stream(io::stream* stream) {
	REQUIRE(stream->can_read());
	REQUIRE(stream->can_seek());
	
	std::error_code ec;
	
	stream->seek(0, seek_origin::end, ec);
	REQUIRE_FALSE(ec);
	
	auto size = stream->tell(ec);
	REQUIRE_FALSE(ec);
	
	stream->seek(0, seek_origin::begin, ec);
	REQUIRE_FALSE(ec);
	
	std::vector<char> buffer(size, 0);
	auto read = stream->read(buffer.data(), size - 1, ec);
	REQUIRE_FALSE(ec);
	REQUIRE(read == size - 1);
	
	auto eof = stream->eof(ec);
	REQUIRE_FALSE(ec);
	REQUIRE(eof == false);
	
	read = stream->read(buffer.data() + read, size, ec);
	REQUIRE_FALSE(ec);
	REQUIRE(read == 1);
	
	eof = stream->eof(ec);
	REQUIRE_FALSE(ec);
	REQUIRE(eof == true);
	
	auto pos = stream->tell(ec);
	REQUIRE_FALSE(ec);
	
	REQUIRE(pos == stream->seek(-1, seek_origin::begin, ec));
	REQUIRE(ec);
	
	REQUIRE(pos == stream->seek(1, seek_origin::end, ec));
	REQUIRE(ec);
	
	io::memory_stream stream2;
	stream->seek(0, seek_origin::begin, ec);
	REQUIRE_FALSE(ec);
	stream->copy_to(stream2, ec);
	REQUIRE_FALSE(ec);
	REQUIRE(stream2.buffer().second == buffer.size());
	REQUIRE(std::memcmp(buffer.data(), stream2.buffer().first, buffer.size()) == 0);
	
	io::memory_stream stream3;
	stream->seek(0, seek_origin::begin, ec);
	REQUIRE_FALSE(ec);
	stream->copy_to(stream3, 10, ec);
	REQUIRE_FALSE(ec);
	REQUIRE(stream3.buffer().second <= 10);
	REQUIRE(std::memcmp(buffer.data(), stream3.buffer().first, stream3.buffer().second) == 0);
}

static void test_write_stream(io::stream* stream) {
	REQUIRE(stream->can_read());
	REQUIRE(stream->can_write());
	REQUIRE(stream->can_seek());
	
	char data[] = "Hello, world!";
	std::error_code ec;
	
	auto written = stream->write(data, sizeof(data), ec);
	REQUIRE_FALSE(ec);
	REQUIRE(written == sizeof(data));
	
	stream->seek(-7, seek_origin::end, ec);
	REQUIRE_FALSE(ec);
	
	written = stream->write(data, sizeof(data), ec);
	REQUIRE_FALSE(ec);
	REQUIRE(written == sizeof(data));
	
	std::vector<char> buffer(stream->tell(ec), 0);
	REQUIRE_FALSE(ec);
	
	stream->seek(0, seek_origin::begin, ec);
	REQUIRE_FALSE(ec);
	
	auto read = stream->read(buffer.data(), buffer.size(), ec);
	REQUIRE_FALSE(ec);
	REQUIRE(read == buffer.size());
	REQUIRE(std::memcmp(buffer.data(), "Hello, Hello, world!", buffer.size()) == 0);
	
	auto pos = stream->tell(ec);
	REQUIRE_FALSE(ec);
	
	REQUIRE(pos == stream->seek(-1, seek_origin::begin, ec));
	REQUIRE(ec);
	
	REQUIRE(pos == stream->seek(1, seek_origin::end, ec));
	REQUIRE(ec);
}

TEST_CASE("io") {
	SECTION("dynamic memory_stream") {
		io::memory_stream stream;
		
		test_write_stream(&stream);
	}
	
	SECTION("static memory_stream") {
		char buffer[] = "Hello, world!";
		io::memory_stream stream(buffer, sizeof(buffer) - 1);
		
		test_read_stream(&stream);
	}
	
	SECTION("stream_view") {
		char buffer[] = "Hello, world!";
		io::memory_stream stream(buffer, sizeof(buffer) - 1);
		io::stream_view view(stream, 7, 5);
		
		test_read_stream(&view);
	}
	
	SECTION("file_stream") {
		io::file_stream stream;
		
		SECTION("write") {
			std::error_code ec;
			stream.open("file.tmp", open_mode::create, access_mode::read_write, ec);
			REQUIRE_FALSE(ec);
			
			test_write_stream(&stream);
		}
		
		SECTION("read") {
			std::error_code ec;
			stream.open("file.tmp", open_mode::open, access_mode::read_only, ec);
			REQUIRE_FALSE(ec);
			
			test_read_stream(&stream);
		}
		
		SECTION("remove") {
			unlink("file.tmp");
		}
	}
	
	SECTION("functions") {
		char buffer[] = "Hello, world!";
		io::memory_stream stream(buffer, sizeof(buffer) - 1);
		std::error_code ec;
		constexpr size_t max_bytes = 5;
		
		SECTION("copy 1") {
			std::vector<char> vec;
			auto read = io::copy(stream, std::back_inserter(vec), max_bytes, ec);
			REQUIRE_FALSE(ec);
			REQUIRE(read == max_bytes);
			REQUIRE(vec.size() == read);
			REQUIRE(std::memcmp(buffer, vec.data(), read) == 0);
			
			SECTION("copy 2") {
				std::vector<char> vec;
				auto read = io::copy(stream, std::back_inserter(vec), ec);
				REQUIRE_FALSE(ec);
				REQUIRE(read == sizeof(buffer) - 1 - max_bytes);
				REQUIRE(vec.size() == read);
				REQUIRE(std::memcmp(buffer + max_bytes, vec.data(), read) == 0);
			}
		}
		
		SECTION("copy 3") {
			std::vector<char> vec = {'h', 'e', 'l', 'l', 'o'};
			io::memory_stream stream2;			
			auto written = io::copy(vec.begin(), vec.end(), stream2, ec);
			REQUIRE_FALSE(ec);
			REQUIRE(written == vec.size());
			REQUIRE(std::memcmp(vec.data(), stream2.buffer().first, written) == 0);
		}
	}
}
