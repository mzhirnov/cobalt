#pragma once

// Classes in this file:
//     seekable
//     input_stream
//     output_stream
//     stream
//     binary_writer
//     binary_reader
//     bitpack_writer
//     bitpack_reader

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

/// bitpack_writer
class bitpack_writer {
public:
	explicit bitpack_writer(output_stream* stream);
	~bitpack_writer() { flush(); }

	output_stream* get_base_stream() const { return _writer.get_base_stream(); }

	void write_bits(uint32_t value, size_t bits);
	void flush();

private:
	binary_writer _writer;
	uint64_t _scratch = 0;
	size_t _scratch_bits = 0;
};

/// bitpack_reader
class bitpack_reader {
public:
	explicit bitpack_reader(input_stream* stream);

	input_stream* get_base_stream() const { return _reader.get_base_stream(); }

	uint32_t read_bits(size_t bits);

private:
	binary_reader _reader;
	uint64_t _scratch = 0;
	size_t _scratch_bits = 0;
};

namespace detail {

template <uint32_t x>
struct pop_count {
	enum {
		a = x - ((x >> 1) & 0x55555555),
		b = (((a >> 2) & 0x33333333) + (a & 0x33333333)),
		c = (((b >> 4) + b) & 0x0f0f0f0f),
		d = c + (c >> 8),
		e = d + (d >> 16),
		value = e & 0x0000003f
	};
};

template <uint32_t x>
struct log2 {
	enum {
		a = x | (x >> 1),
		b = a | (a >> 2),
		c = b | (b >> 4),
		d = c | (c >> 8),
		e = d | (d >> 16),
		f = e >> 1,
		value = pop_count<f>::value
	};
};

} // namespace detail

template <int64_t min, int64_t max>
struct bits_required {
	enum {
		value = (min == max) ? 0 : (detail::log2<uint32_t(max - min)>::value + 1)
	};
};

} // namespace io
