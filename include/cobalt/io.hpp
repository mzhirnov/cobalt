#ifndef COBALT_IO_HPP_INCLUDED
#define COBALT_IO_HPP_INCLUDED

#pragma once

// Classes in this file:
//     stream
//     static_memory_stream
//     dynamic_memory_stream
//     file_stream
//     binary_writer
//     binary_reader
//     bitpack_writer
//     bitpack_reader

#include <cobalt/utility/enum_traits.hpp>
#include <cobalt/utility/intrusive.hpp>
#include <cobalt/utility/throw_error.hpp>

#include <iterator>
#include <deque>
#include <cstdio>
#include <cstdint>

CO_DEFINE_ENUM_CLASS(
	seek_origin, uint8_t,
	beginning,
	current,
	end
)

CO_DEFINE_ENUM_CLASS(
	open_mode, uint8_t,
	create_always,
	open_existing,
	open_or_create
)

CO_DEFINE_ENUM_CLASS(
	access_mode, uint8_t,
	read_only,
	read_write
)

namespace cobalt { namespace io {

/// Stream
class stream : public ref_counter<stream> {
public:
	using value_type = uint8_t;
	
	virtual ~stream() noexcept = default;
	
	virtual size_t read(void* buffer, size_t size, std::error_code& ec) noexcept = 0;
	virtual size_t write(const void* buffer, size_t size, std::error_code& ec) noexcept = 0;
	virtual void flush(std::error_code& ec) const noexcept = 0;
	virtual int64_t seek(int64_t offset, seek_origin origin, std::error_code& ec) noexcept = 0;
	virtual int64_t tell(std::error_code& ec) const noexcept = 0;
	
	size_t read(void* buffer, size_t size);
	size_t write(const void* buffer, size_t size);
	void flush() const;
	int64_t seek(int64_t offset, seek_origin origin);
	int64_t tell() const;
	
	bool can_read() const noexcept;
	bool can_write() const noexcept;
	bool can_seek() const  noexcept;
};

class static_memory_stream : public stream {
public:
	static_memory_stream(void* begin, size_t size);
	static_memory_stream(void* begin, size_t size, access_mode access);
	
	access_mode access() const noexcept;
	void access(access_mode access);
	
	virtual size_t read(void* buffer, size_t size, std::error_code& ec) noexcept override;
	virtual size_t write(const void* buffer, size_t size, std::error_code& ec) noexcept override;
	virtual void flush(std::error_code& ec) const noexcept override;
	virtual int64_t seek(int64_t offset, seek_origin origin, std::error_code& ec) noexcept override;
	virtual int64_t tell(std::error_code& ec) const noexcept override;

private:
	value_type* _begin = nullptr;
	size_t _size = 0;
	size_t _position = 0;
	access_mode _access = access_mode::read_only;
};

class dynamic_memory_stream : public stream {
public:
	dynamic_memory_stream();
	
	access_mode access() const noexcept;
	void access(access_mode access);
	
	virtual size_t read(void* buffer, size_t size, std::error_code& ec) noexcept override;
	virtual size_t write(const void* buffer, size_t size, std::error_code& ec) noexcept override;
	virtual void flush(std::error_code& ec) const noexcept override;
	virtual int64_t seek(int64_t offset, seek_origin origin, std::error_code& ec) noexcept override;
	virtual int64_t tell(std::error_code& ec) const noexcept override;

private:
	std::deque<value_type> _data;
	access_mode _access = access_mode::read_write;
};

class file_stream : public stream {
public:
	file_stream(const char* filename, open_mode open, access_mode access);
	
	file_stream(file_stream&&) noexcept;
	file_stream& operator=(file_stream&&) noexcept;
	
	file_stream(const file_stream&) = delete;
	file_stream& operator=(const file_stream&) = delete;
	
	~file_stream() noexcept;
	
	virtual size_t read(void* buffer, size_t size, std::error_code& ec) noexcept override;
	virtual size_t write(const void* buffer, size_t size, std::error_code& ec) noexcept override;
	virtual void flush(std::error_code& ec) const noexcept override;
	virtual int64_t seek(int64_t offset, seek_origin origin, std::error_code& ec) noexcept override;
	virtual int64_t tell(std::error_code& ec) const noexcept override;

private:
	FILE* _fp = nullptr;
	access_mode _access = access_mode::read_only;
};

/// Binary writer
class binary_writer {
public:
	explicit binary_writer(stream* stream);

	stream* base_stream() const { return _stream.get(); }

	void write(uint8_t i);
	void write(uint16_t i);
	void write(uint32_t i);
	void write(uint64_t i);
	void write(int8_t i);
	void write(int16_t i);
	void write(int32_t i);
	void write(int64_t i);
	void write(float f);
	void write(double f);
	void write(bool b);
	void write_7bit_encoded_int(uint32_t i);
	void write_unicode_char(uint32_t c);
	void write_c_string(const char* str); // zero ended string
	void write_pascal_string(const char* str); // length prepended string

private:
	counted_ptr<stream> _stream;
};

/// Binary reader
class binary_reader {
public:
	explicit binary_reader(stream* stream);

	stream* base_stream() const { return _stream.get(); }

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
	std::string read_c_string(); // zero ended string
	std::string read_pascal_string(); // length prepended string

private:
	counted_ptr<stream> _stream;
};

/// Bit packed stream writer
class bitpack_writer {
public:
	explicit bitpack_writer(stream* stream);
	~bitpack_writer() { flush(); }

	stream* base_stream() const { return _writer.base_stream(); }

	void write_bits(uint32_t value, size_t bits);
	void flush();

private:
	binary_writer _writer;
	uint64_t _scratch = 0;
	size_t _scratch_bits = 0;
};

/// Bit packed stream reader
class bitpack_reader {
public:
	explicit bitpack_reader(stream* stream);

	stream* base_stream() const { return _reader.base_stream(); }

	uint32_t read_bits(size_t bits);

private:
	binary_reader _reader;
	uint64_t _scratch = 0;
	size_t _scratch_bits = 0;
};

template <typename OutputIterator, typename = typename std::enable_if_t<sizeof(std::iterator_traits<OutputIterator>::value_type) == 1>>
void read_all(stream* stream, OutputIterator it);

template <typename InputIterator, typename = typename std::enable_if_t<sizeof(std::iterator_traits<InputIterator>::value_type) == 1>>
void read_all(InputIterator it, stream* stream);

void read_all(stream* istream, stream* ostream);

template <typename OutputIterator, typename = typename std::enable_if_t<sizeof(std::iterator_traits<OutputIterator>::value_type) == 1>>
void read_some(stream* stream, OutputIterator it, size_t size);

template <typename InputIterator, typename = typename std::enable_if_t<sizeof(std::iterator_traits<InputIterator>::value_type) == 1>>
void read_some(InputIterator it, stream* stream, size_t size);

void read_some(stream* istream, stream* ostream, size_t size);

template <typename OutputIterator, typename = typename std::enable_if_t<sizeof(std::iterator_traits<OutputIterator>::value_type) == 1>>
void copy(stream* stream, OutputIterator it);

void copy(stream* istream, stream* ostream);

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

}} // namespace cobalt::io

#endif // COBALT_IO_HPP_INCLUDED
