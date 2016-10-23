#pragma once

// Classes in this file:
//     seekable
//     input_stream
//     output_stream
//     stream
//     binary_reader
//     binary_writer

#include "cool/common.hpp"
#include "cool/enum_traits.hpp"

#include <cstdio>
#include <cstdint>

#include <vector>

namespace io {

DEFINE_ENUM_CLASS(
	origin, uint32_t,
	beginning,
	current,
	end
)

/// seekable base
class seekable : public ref_counter<seekable> {
public:
	virtual ~seekable() {}

	virtual void seek(int pos, origin origin = origin::beginning) = 0;

	virtual size_t tell() const = 0;
};

/// input stream
class input_stream : virtual public seekable {
public:
	virtual size_t read(void* buffer, size_t length) = 0;

	virtual bool eof() const = 0;
};

typedef boost::intrusive_ptr<input_stream> input_stream_ptr;

/// output stream
class output_stream : virtual public seekable {
public:
	virtual size_t write(const void* buffer, size_t length) = 0;

	virtual void flush() const = 0;
};

typedef boost::intrusive_ptr<output_stream> output_stream_ptr;

DEFINE_ENUM_CLASS(
	file_mode, uint32_t,
	create_always,
	open_existing
)

DEFINE_ENUM_CLASS(
	file_access, uint32_t,
	read_only,
	read_write
)

/// input/output stream
class stream : public input_stream, public output_stream {
public:
	static stream* from_memory();
	static stream* from_memory(void* begin, void* end, file_access access = file_access::read_write);
	static stream* from_file(const char* filename, file_mode open_mode = file_mode::open_existing, file_access access = file_access::read_only);
	static input_stream* from_asset(const char* filename);

	static void read_all(input_stream* istream, std::vector<uint8_t>& data);
	static void write_all(output_stream* ostream, const std::vector<uint8_t>& data);
	static void copy(input_stream* istream, output_stream* ostream);
};

typedef boost::intrusive_ptr<stream> stream_ptr;

/// binary reader
class binary_reader {
public:
	explicit binary_reader(input_stream* stream);

	input_stream* get_base_stream() const { return _stream.get(); }

	uint8_t read_uint8();
	uint16_t read_uint16();
	uint32_t read_uint32();
	uint64_t read_uint64();
	int8_t read_int8();
	int16_t read_int16();
	int32_t read_int32();
	int64_t read_int64();
	float read_float();
	double read_double();
	bool read_boolean();
	uint32_t read_7bit_encoded_uint32();
	uint32_t read_unicode_char();
	std::string read_zero_string(); // zero ended string
	std::string read_pascal_string(); // length prepended string

private:
	input_stream_ptr _stream;
};

/// binary writer
class binary_writer {
public:
	explicit binary_writer(output_stream* stream);

	output_stream* get_base_stream() const { return _stream.get(); }

	void write_uint8(uint8_t i);
	void write_uint16(uint16_t i);
	void write_uint32(uint32_t i);
	void write_uint64(uint64_t i);
	void write_int8(int8_t i);
	void write_int16(int16_t i);
	void write_int32(int32_t i);
	void write_int64(int64_t i);
	void write_float(float f);
	void write_double(double f);
	void write_boolean(bool b);
	void write_7bit_encoded_uint32(uint32_t i);
	void write_unicode_char(uint32_t c);
	void write_zero_string(const std::string& str); // zero ended string
	void write_pascal_string(const std::string& str); // length prepended string

private:
	output_stream_ptr _stream;
};

} // namespace io
