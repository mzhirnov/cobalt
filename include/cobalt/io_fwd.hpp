#ifndef COBALT_IO_FWD_HPP_INCLUDED
#define COBALT_IO_FWD_HPP_INCLUDED

#pragma once

// Classes in this file:
//     stream
//     stream_view
//     memory_stream
//     file_stream
//     binary_writer
//     binary_reader
//     bitpack_writer
//     bitpack_reader
//
// Functions in this file:
//     copy

#include <cobalt/utility/enum_traits.hpp>
#include <cobalt/utility/intrusive.hpp>
#include <cobalt/utility/throw_if_error.hpp>

#include <vector>
#include <cstdio>
#include <cstdint>

CO_DEFINE_ENUM(
	seek_origin, uint8_t,
	begin,
	current,
	end
)

CO_DEFINE_ENUM(
	open_mode, uint8_t,
	create,              ///< Create if doesn't exist or overwrite otherwise
	create_new,          ///< Create if doesn't exist
	open,                ///< Open existing
	open_or_create       ///< Open if exists or create new otherwise
)

CO_DEFINE_ENUM(
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
	virtual bool eof(std::error_code& ec) const noexcept = 0;
	
	/// Throw exception on error
	size_t read(void* buffer, size_t size);
	size_t write(const void* buffer, size_t size);
	void flush() const;
	int64_t seek(int64_t offset, seek_origin origin);
	int64_t tell() const;
	bool eof() const;
	
	bool can_read() noexcept;
	bool can_write() noexcept;
	bool can_seek() noexcept;
	
	void copy_to(stream& stream, std::error_code& ec) noexcept;
	void copy_to(stream& stream, size_t max_size, std::error_code& ec) noexcept;
	
	void copy_to(stream& stream);
	void copy_to(stream& stream, size_t max_size);
};

/// Stream view adapter
class stream_view : public stream {
public:
	/// Will not increase reference count
	stream_view(stream& stream, int64_t offset, int64_t length);
	/// Will increase reference count
	stream_view(stream* stream, int64_t offset, int64_t length);
	
	~stream_view();
	
	stream* base_stream() const noexcept { return _stream; }
	
	virtual size_t read(void* buffer, size_t size, std::error_code& ec) noexcept override;
	virtual size_t write(const void* buffer, size_t size, std::error_code& ec) noexcept override;
	virtual void flush(std::error_code& ec) const noexcept override;
	virtual int64_t seek(int64_t offset, seek_origin origin, std::error_code& ec) noexcept override;
	virtual int64_t tell(std::error_code& ec) const noexcept override;
	virtual bool eof(std::error_code& ec) const noexcept override;
	
private:
	stream* _stream = nullptr;
	bool _owning = false;
	int64_t _offset = 0;
	int64_t _length = 0;
};

/// Memory stream
class memory_stream : public stream {
public:
	explicit memory_stream(size_t capacity = 0);
	memory_stream(void* buffer, size_t size) noexcept;
	memory_stream(void* buffer, size_t size, access_mode access) noexcept;
	
	memory_stream(const memory_stream&) = delete;
	memory_stream& operator=(const memory_stream&) = delete;
	
	~memory_stream();
	
	access_mode access() const noexcept;
	void access(access_mode access);
	
	bool dynamic() const noexcept;
	
	std::pair<value_type*, size_t> buffer() const noexcept;
	
	virtual size_t read(void* buffer, size_t size, std::error_code& ec) noexcept override;
	virtual size_t write(const void* buffer, size_t size, std::error_code& ec) noexcept override;
	virtual void flush(std::error_code& ec) const noexcept override;
	virtual int64_t seek(int64_t offset, seek_origin origin, std::error_code& ec) noexcept override;
	virtual int64_t tell(std::error_code& ec) const noexcept override;
	virtual bool eof(std::error_code& ec) const noexcept override;
	
private:
	size_t read_impl(void* buffer, size_t size, std::error_code& ec, std::true_type) noexcept;
	size_t write_impl(const void* buffer, size_t size, std::error_code& ec, std::true_type) noexcept;
	bool eof_impl(std::error_code& ec, std::true_type) const noexcept;
	
	size_t read_impl(void* buffer, size_t size, std::error_code& ec, std::false_type) noexcept;
	size_t write_impl(const void* buffer, size_t size, std::error_code& ec, std::false_type) noexcept;
	bool eof_impl(std::error_code& ec, std::false_type) const noexcept;
	
	int64_t seek_impl(int64_t offset, seek_origin origin, size_t size, std::error_code& ec) noexcept;

private:
	union {
		value_type* _buffer = nullptr;
		std::vector<value_type>* _vec;
	};
	size_t _size = 0;
	size_t _position = 0;
	access_mode _access = access_mode::read_only;
};

/// File stream
class file_stream : public stream {
public:
	file_stream() noexcept = default;
	
	file_stream(const file_stream&) = delete;
	file_stream& operator=(const file_stream&) = delete;
	
	~file_stream() noexcept;
	
	void open(const char* filename, open_mode open_mode, access_mode access, std::error_code& ec) noexcept;
	void open(const char* filename, open_mode open_mode, access_mode access);
	
	void close(std::error_code& ec) noexcept;
	void close();
	
	bool valid() const noexcept;
	
	access_mode access() const noexcept;
	
	virtual size_t read(void* buffer, size_t size, std::error_code& ec) noexcept override;
	virtual size_t write(const void* buffer, size_t size, std::error_code& ec) noexcept override;
	virtual void flush(std::error_code& ec) const noexcept override;
	virtual int64_t seek(int64_t offset, seek_origin origin, std::error_code& ec) noexcept override;
	virtual int64_t tell(std::error_code& ec) const noexcept override;
	virtual bool eof(std::error_code& ec) const noexcept override;

private:
	FILE* _fp = nullptr;
	access_mode _access = access_mode::read_only;
};

/// Copies bytes from current position until eof
/// @return Number of bytes actually read
template <typename OutputIterator>
size_t copy(stream& stream, OutputIterator it, std::error_code& ec) noexcept;

/// Copies bytes from current position until eof
/// Reads up to specified number of bytes
/// @return Number of bytes actually read
template <typename OutputIterator>
size_t copy(stream& stream, OutputIterator it, size_t max_bytes, std::error_code& ec) noexcept;

/// Writes bytes range to stream
/// @return Number of bytes actually written
template <typename InputIterator>
size_t copy(InputIterator begin, InputIterator end, stream& stream, std::error_code& ec) noexcept;

/// Copies bytes from current position until eof
/// Throws on error
/// @return Number of bytes written
template <typename OutputIterator>
size_t copy(stream& stream, OutputIterator it);

/// Reads up to specified number of bytes
/// Throws on error
/// @return Number of bytes actually read
template <typename OutputIterator>
size_t copy(stream& stream, OutputIterator it, size_t max_bytes);

/// Writes bytes range to stream
/// Throws on error
/// @return Number of bytes actually written
template <typename InputIterator>
size_t copy(InputIterator begin, InputIterator end, stream& stream);

/// Binary writer
class binary_writer {
public:
	explicit binary_writer(stream& stream) noexcept;
	explicit binary_writer(stream* stream) noexcept;
	
	~binary_writer();

	stream* base_stream() const noexcept { return _stream; }

	void write(uint8_t value, std::error_code& ec) noexcept;
	void write(uint16_t value, std::error_code& ec) noexcept;
	void write(uint32_t value, std::error_code& ec) noexcept;
	void write(uint64_t value, std::error_code& ec) noexcept;
	void write(int8_t value, std::error_code& ec) noexcept;
	void write(int16_t value, std::error_code& ec) noexcept;
	void write(int32_t value, std::error_code& ec) noexcept;
	void write(int64_t value, std::error_code& ec) noexcept;
	void write(float value, std::error_code& ec) noexcept;
	void write(double value, std::error_code& ec) noexcept;
	void write(bool value, std::error_code& ec) noexcept;
	void write_7bit_encoded_int(uint32_t value, std::error_code& ec) noexcept;
	void write_unicode_char(uint32_t value, std::error_code& ec) noexcept;
	void write_c_string(const char* str, std::error_code& ec) noexcept; // zero ended string
	void write_pascal_string(const char* str, std::error_code& ec) noexcept; // length prepended string

	void write(uint8_t value);
	void write(uint16_t value);
	void write(uint32_t value);
	void write(uint64_t value);
	void write(int8_t value);
	void write(int16_t value);
	void write(int32_t value);
	void write(int64_t value);
	void write(float value);
	void write(double value);
	void write(bool value);
	void write_7bit_encoded_int(uint32_t value);
	void write_unicode_char(uint32_t value);
	void write_c_string(const char* str); // zero ended string
	void write_pascal_string(const char* str); // length prepended string

private:
	stream* _stream = nullptr;
	bool _owning = false;
};

/// Binary reader
class binary_reader {
public:
	explicit binary_reader(stream& stream) noexcept;
	explicit binary_reader(stream* stream) noexcept;
	
	~binary_reader();

	stream* base_stream() const noexcept { return _stream; }

	uint8_t read_uint8(std::error_code& ec) noexcept;
	uint16_t read_uint16(std::error_code& ec) noexcept;
	uint32_t read_uint32(std::error_code& ec) noexcept;
	uint64_t read_uint64(std::error_code& ec) noexcept;
	int8_t read_int8(std::error_code& ec) noexcept;
	int16_t read_int16(std::error_code& ec) noexcept;
	int32_t read_int32(std::error_code& ec) noexcept;
	int64_t read_int64(std::error_code& ec) noexcept;
	float read_float(std::error_code& ec) noexcept;
	double read_double(std::error_code& ec) noexcept;
	bool read_bool(std::error_code& ec) noexcept;
	uint32_t read_7bit_encoded_int(std::error_code& ec) noexcept;
	uint32_t read_unicode_char(std::error_code& ec) noexcept;
	std::string read_c_string(std::error_code& ec) noexcept; // zero ended string
	std::string read_pascal_string(std::error_code& ec) noexcept; // length prepended string

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
	bool read_bool();
	uint32_t read_7bit_encoded_int();
	uint32_t read_unicode_char();
	std::string read_c_string(); // zero ended string
	std::string read_pascal_string(); // length prepended string

private:
	stream* _stream = nullptr;
	bool _owning = false;
};

/// Bit packed stream writer
class bitpack_writer {
public:
	explicit bitpack_writer(stream& stream) noexcept;
	explicit bitpack_writer(stream* stream) noexcept;
	
	~bitpack_writer();

	stream* base_stream() const noexcept { return _writer.base_stream(); }

	/// Write specified number of bits
	void write_bits(uint32_t value, size_t bits, std::error_code& ec) noexcept;
	/// Write buffered bits aligning position by the byte boundary
	void flush(std::error_code& ec);
	
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
	explicit bitpack_reader(stream& stream) noexcept;
	explicit bitpack_reader(stream* stream) noexcept;
	
	~bitpack_reader() noexcept;

	stream* base_stream() const noexcept { return _reader.base_stream(); }

	/// Read specified number of bits
	uint32_t read_bits(size_t bits, std::error_code& ec) noexcept;
	/// Align position by the byte boundary
	void align(std::error_code& ec) noexcept;
	
	uint32_t read_bits(size_t bits);
	void align();

private:
	binary_reader _reader;
	uint64_t _scratch = 0;
	size_t _scratch_bits = 0;
};

}} // namespace cobalt::io

#endif // COBALT_IO_FWD_HPP_INCLUDED
