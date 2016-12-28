#ifndef COBALT_IO_HPP_INCLUDED
#define COBALT_IO_HPP_INCLUDED

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

#include <cobalt/utility/enum_traits.hpp>
#include <cobalt/utility/intrusive.hpp>
#include <cobalt/utility/throw_error.hpp>

#include <type_traits>
#include <iterator>
#include <vector>
#include <cstdio>
#include <cstdint>

CO_DEFINE_ENUM_CLASS(
	seek_origin, uint8_t,
	begin,
	current,
	end
)

CO_DEFINE_ENUM_CLASS(
	open_mode, uint8_t,
	create_always, ///< Create new or overwrite if exists
	create_new,    ///< Create new if not exists
	open_existing, ///< Open if exists
	open_or_create ///< Open if exists or create otherwise
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
};

inline size_t stream::read(void* buffer, size_t size) {
	std::error_code ec;
	auto ret = read(buffer, size, ec);
	throw_error(ec);
	return ret;
}

inline size_t stream::write(const void* buffer, size_t size) {
	std::error_code ec;
	auto ret = write(buffer, size, ec);
	throw_error(ec);
	return ret;
}

inline void stream::flush() const {
	std::error_code ec;
	flush(ec);
	throw_error(ec);
}

inline int64_t stream::seek(int64_t offset, seek_origin origin) {
	std::error_code ec;
	auto ret = seek(offset, origin, ec);
	throw_error(ec);
	return ret;
}

inline int64_t stream::tell() const {
	std::error_code ec;
	auto ret = tell(ec);
	throw_error(ec);
	return ret;
}

inline bool stream::eof() const {
	std::error_code ec;
	auto ret = eof(ec);
	throw_error(ec);
	return ret;
}

inline bool stream::can_read() noexcept {
	std::error_code ec;
	read(nullptr, 0, ec);
	return !ec;
}

inline bool stream::can_write() noexcept {
	std::error_code ec;
	write(nullptr, 0, ec);
	return !ec;
}

inline bool stream::can_seek() noexcept {
	std::error_code ec;
	seek(0, seek_origin::current, ec);
	return !ec;
}

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

inline stream_view::stream_view(stream& stream, int64_t offset, int64_t length)
	: _stream(&stream)
	, _offset(offset)
	, _length(length)
{
	BOOST_ASSERT(_stream->can_seek());
	
	if (!_stream->can_seek())
		throw std::system_error(std::make_error_code(std::errc::function_not_supported), "seek");
}

inline stream_view::stream_view(stream* stream, int64_t offset, int64_t length)
	: _stream(stream)
	, _owning(true)
	, _offset(offset)
	, _length(length)
{
	counted_ptr<class stream> sp = _stream;
	
	BOOST_ASSERT(_stream != nullptr);
	if (!_stream)
		throw std::system_error(std::make_error_code(std::errc::invalid_argument), "stream");

	BOOST_ASSERT(_stream->can_seek());
	if (!_stream->can_seek())
		throw std::system_error(std::make_error_code(std::errc::function_not_supported), "seek");
	
	BOOST_ASSERT(_offset >= 0);
	if (_offset < 0)
		throw std::system_error(std::make_error_code(std::errc::invalid_argument), "offset");
	
	BOOST_ASSERT(_length >= 0);
	if (_length < 0)
		throw std::system_error(std::make_error_code(std::errc::invalid_argument), "length");
	
	sp.detach();
}

inline stream_view::~stream_view()
{
	if (_owning)
		release(_stream);
}

inline size_t stream_view::read(void* buffer, size_t size, std::error_code& ec) noexcept {
	auto position = tell(ec);
	if (ec)
		return static_cast<size_t>(position);
	auto count = std::min(static_cast<size_t>(_length - position), size);
	return _stream->read(buffer, count, ec);
}

inline size_t stream_view::write(const void* buffer, size_t size, std::error_code& ec) noexcept {
	auto position = tell(ec);
	if (ec)
		return static_cast<size_t>(position);
	auto count = std::min(static_cast<size_t>(_length - position), size);
	return _stream->write(buffer, count, ec);
}

inline void stream_view::flush(std::error_code& ec) const noexcept {
	_stream->flush(ec);
}

inline int64_t stream_view::seek(int64_t offset, seek_origin origin, std::error_code& ec) noexcept {
	switch (origin) {
	case seek_origin::begin:
		if (offset < 0 || offset > _length)
			ec = std::make_error_code(std::errc::invalid_seek);
		else
			return _stream->seek(_offset + offset, origin, ec) - _offset;
	case seek_origin::current:
		// Backdoor to seek beyond the range
		return _stream->seek(offset, origin, ec) - _offset;
	case seek_origin::end:
		if (offset > 0 || -offset > _length)
			ec = std::make_error_code(std::errc::invalid_seek);
		else
			return _stream->seek(_offset + _length - offset, origin, ec) - _offset;
	}
	
	BOOST_ASSERT(false);
	return _stream->tell(ec) - _offset;
}

inline int64_t stream_view::tell(std::error_code& ec) const noexcept {
	return _stream->tell(ec) - _offset;
}

inline bool stream_view::eof(std::error_code& ec) const noexcept {
	return _stream->tell(ec) >= _offset + _length;
}

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
	int64_t seek_impl(int64_t offset, seek_origin origin, size_t size, std::error_code& ec) noexcept;
	bool eof_impl(std::error_code& ec, std::true_type) const noexcept;
	
	size_t read_impl(void* buffer, size_t size, std::error_code& ec, std::false_type) noexcept;
	size_t write_impl(const void* buffer, size_t size, std::error_code& ec, std::false_type) noexcept;
	bool eof_impl(std::error_code& ec, std::false_type) const noexcept;

private:
	union {
		value_type* _buffer = nullptr;
		std::vector<value_type>* _vec;
	};
	size_t _size = 0;
	size_t _position = 0;
	access_mode _access = access_mode::read_only;
};

inline memory_stream::memory_stream(size_t capacity)
	: _vec(new std::vector<value_type>())
{
	_vec->reserve(capacity);
}

inline memory_stream::memory_stream(void* buffer, size_t size) noexcept
	: _buffer(reinterpret_cast<value_type*>(buffer))
	, _size(size)
{
	BOOST_ASSERT(_size != 0);
}

inline memory_stream::memory_stream(void* buffer, size_t size, access_mode access) noexcept
	: _buffer(reinterpret_cast<value_type*>(buffer))
	, _size(size)
	, _access(access)
{
	BOOST_ASSERT(_size != 0);
}

inline memory_stream::~memory_stream() {
	if (dynamic())
		boost::checked_delete(_vec);
}

inline access_mode memory_stream::access() const noexcept {
	return _access;
}

inline void memory_stream::access(access_mode access) {
	_access = access;
}

inline bool memory_stream::dynamic() const noexcept {
	return _size == 0;
}

inline std::pair<memory_stream::value_type*, size_t> memory_stream::buffer() const noexcept {
	return (dynamic()) ?
		std::make_pair(_vec->data(), _vec->size()) :
		std::make_pair(_buffer, _size);
}

inline size_t memory_stream::read(void* buffer, size_t size, std::error_code& ec) noexcept {
	return (dynamic()) ?
		read_impl(buffer, size, ec, std::true_type()) :
		read_impl(buffer, size, ec, std::false_type());
}

inline size_t memory_stream::write(const void* buffer, size_t size, std::error_code& ec) noexcept {
	if (_access == access_mode::read_only) {
		ec = std::make_error_code(std::errc::operation_not_supported);
		return 0;
	}
	
	return (dynamic()) ?
		write_impl(buffer, size, ec, std::true_type()) :
		write_impl(buffer, size, ec, std::false_type());
}

inline void memory_stream::flush(std::error_code& ec) const noexcept {
	ec = std::error_code();
}

inline int64_t memory_stream::seek(int64_t offset, seek_origin origin, std::error_code& ec) noexcept {
	return seek_impl(offset, origin, (dynamic()) ? _vec->size() : _size, ec);
}

inline int64_t memory_stream::tell(std::error_code& ec) const noexcept {
	ec = std::error_code();
	return _position;
}

inline bool memory_stream::eof(std::error_code& ec) const noexcept {
	return (dynamic()) ?
		eof_impl(ec, std::true_type()) :
		eof_impl(ec, std::false_type());
}

inline size_t memory_stream::read_impl(void* buffer, size_t size, std::error_code& ec, std::true_type) noexcept {
	ec = std::error_code();
	
	size_t count = std::min(_vec->size() - _position, size);
	if (count > 0) {
		BOOST_ASSERT(buffer != nullptr);
		memcpy(buffer, _vec->data() + _position, count);
		_position += count;
	}
	
	return count;
}

inline size_t memory_stream::read_impl(void* buffer, size_t size, std::error_code& ec, std::false_type) noexcept {
	ec = std::error_code();
	
	size_t count = std::min(_size - _position, size);
	if (count > 0) {
		BOOST_ASSERT(buffer != nullptr);
		memcpy(buffer, _buffer + _position, count);
		_position += count;
	}

	return count;
}

inline size_t memory_stream::write_impl(const void* buffer, size_t size, std::error_code& ec, std::true_type) noexcept {
	ec = std::error_code();
	
	size_t count = std::min(_vec->size() - _position, size);
	if (count > 0) {
		BOOST_ASSERT(buffer != nullptr);
		memcpy(_vec->data() + _position, buffer, count);
	}

	// Extra data beyond the vector end
	auto extra = size - count;
	if (extra > 0) {
		BOOST_ASSERT(buffer != nullptr);
		const value_type* p = reinterpret_cast<const value_type*>(buffer) + count;
		try {
			_vec->insert(_vec->end(), p, p + extra);
		} catch (const std::bad_alloc&) {
			ec = std::make_error_code(std::errc::not_enough_memory);
			return 0;
		}
	}

	_position += size;
	return size;
}

inline size_t memory_stream::write_impl(const void* buffer, size_t size, std::error_code& ec, std::false_type) noexcept {
	ec = std::error_code();
	
	size_t count = std::min(_size - _position, size);
	if (count > 0) {
		BOOST_ASSERT(buffer != nullptr);
		memcpy(_buffer + _position, buffer, count);
		_position += count;
	}

	return count;
}

inline int64_t memory_stream::seek_impl(int64_t offset, seek_origin origin, size_t size, std::error_code& ec) noexcept {
	ec = std::error_code();
	
	switch (origin) {
	case seek_origin::begin:
		if (offset < 0 || offset > size)
			ec = std::make_error_code(std::errc::invalid_seek);
		else
			_position = static_cast<size_t>(offset);
		break;
	case seek_origin::current:
		if ((offset < 0 && _position < -offset) || _position + offset > size)
			ec = std::make_error_code(std::errc::invalid_seek);
		else
			_position += offset;
		break;
	case seek_origin::end:
		if (offset > 0 || -offset > size)
			ec = std::make_error_code(std::errc::invalid_seek);
		else
			_position = size + offset;
		break;
	}
	
	return _position;
}

inline bool memory_stream::eof_impl(std::error_code& ec, std::true_type) const noexcept {
	return _position == _vec->size();
}

inline bool memory_stream::eof_impl(std::error_code& ec, std::false_type) const noexcept {
	return _position == _size;
}

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

inline file_stream::~file_stream() noexcept {
	std::error_code ec;
	close(ec);
	BOOST_ASSERT(!ec);
}

inline void file_stream::open(const char* filename, open_mode mode, access_mode access, std::error_code& ec) noexcept {
	close(ec);
	
	_access = access;
	
	ec = std::error_code();
	
	switch (access) {
	case access_mode::read_only:
		switch (mode) {
		case open_mode::open_existing:
			_fp = std::fopen(filename, "rb");
			break;
		default: // Skip all create modes
			ec = std::make_error_code(std::errc::invalid_argument);
			break;
		}
		break;
	case access_mode::read_write:
		switch (mode) {
		case open_mode::create_always:
			_fp = std::fopen(filename, "w+b");
			break;
		case open_mode::create_new:
			if ((_fp = std::fopen(filename, "rb"))) {
				close(ec);
				ec = std::make_error_code(std::errc::file_exists);
			} else {
				_fp = std::fopen(filename, "w+b");
			}
			break;
		case open_mode::open_existing:
			_fp = std::fopen(filename, "r+b");
			break;
		case open_mode::open_or_create:
			if (!(_fp = std::fopen(filename, "r+b")))
				_fp = std::fopen(filename, "w+b");
			break;
		default:
			ec = std::make_error_code(std::errc::invalid_argument);
			break;
		}
		break;
	default:
		ec = std::make_error_code(std::errc::invalid_argument);
		break;
	}
	
	if (!ec && !_fp)
		ec = std::error_code(errno, std::generic_category());
}

inline void file_stream::open(const char* filename, open_mode open_mode, access_mode access) {
	std::error_code ec;
	open(filename, open_mode, access, ec);
	throw_error(ec);
}

inline void file_stream::close(std::error_code& ec) noexcept {
	if (!_fp) {
		ec = std::error_code();
	} else {
		std::fclose(_fp);
		_fp = nullptr;
		ec = std::error_code(errno, std::generic_category());
	}
}

inline void file_stream::close() {
	std::error_code ec;
	close(ec);
	throw_error(ec);
}

inline bool file_stream::valid() const noexcept {
	return _fp != nullptr;
}

inline size_t file_stream::read(void* buffer, size_t size, std::error_code& ec) noexcept {
	BOOST_ASSERT(valid());
	if (!valid()) {
		ec = std::make_error_code(std::errc::bad_file_descriptor);
		return 0;
	}
	auto ret = std::fread(buffer, 1, size, _fp);
	ec = std::error_code(errno, std::generic_category());
	return ret;
}

inline size_t file_stream::write(const void* buffer, size_t size, std::error_code& ec) noexcept {
	if (_access == access_mode::read_only) {
		ec = std::make_error_code(std::errc::operation_not_supported);
		return 0;
	}
	
	BOOST_ASSERT(valid());
	if (!valid()) {
		ec = std::make_error_code(std::errc::bad_file_descriptor);
		return 0;
	}
	auto ret = std::fwrite(buffer, 1, size, _fp);
	ec = std::error_code(errno, std::generic_category());
	return ret;
}

inline void file_stream::flush(std::error_code& ec) const noexcept {
	BOOST_ASSERT(valid());
	if (!valid()) {
		ec = std::make_error_code(std::errc::bad_file_descriptor);
		return;
	}
	std::fflush(_fp);
	ec = std::error_code(errno, std::generic_category());
}

inline int64_t file_stream::seek(int64_t offset, seek_origin origin, std::error_code& ec) noexcept {
	BOOST_ASSERT(valid());
	if (!valid()) {
		ec = std::make_error_code(std::errc::bad_file_descriptor);
		return 0;
	}
	
	// Clear error and EOF flags to make rewinding possible
	std::clearerr(_fp);
	
	int Origin =
		(origin == seek_origin::begin) ? SEEK_SET :
		(origin == seek_origin::current) ? SEEK_CUR : SEEK_END;

	std::fseek(_fp, static_cast<long>(offset), Origin);
	ec = std::error_code(errno, std::generic_category());
	
	return std::ftell(_fp);
}

inline int64_t file_stream::tell(std::error_code& ec) const noexcept {
	BOOST_ASSERT(valid());
	if (!valid()) {
		ec = std::make_error_code(std::errc::bad_file_descriptor);
		return 0;
	}
	auto ret = std::ftell(_fp);
	ec = std::error_code(errno, std::generic_category());
	return ret;
}

inline bool file_stream::eof(std::error_code& ec) const noexcept {
	BOOST_ASSERT(valid());
	if (!valid()) {
		ec = std::make_error_code(std::errc::bad_file_descriptor);
		return 0;
	}
	auto ret = std::feof(_fp);
	ec = std::error_code(errno, std::generic_category());
	return ret != 0;
}

/// Binary writer
class binary_writer {
public:
	explicit binary_writer(stream& stream) noexcept;
	explicit binary_writer(stream* stream) noexcept;
	
	~binary_writer();

	stream* base_stream() const noexcept { return _stream; }

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
	stream* _stream = nullptr;
	bool _owning = false;
};

/// Binary reader
class binary_reader {
public:
	explicit binary_reader(stream& stream) noexcept;
	explicit binary_reader(stream* stream) noexcept;

	stream* base_stream() const noexcept { return _stream; }

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
	uint32_t read_7bit_encoded_uint32();
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
	
	~bitpack_writer() { flush(); }

	stream* base_stream() const noexcept { return _writer.base_stream(); }

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

	stream* base_stream() const noexcept { return _reader.base_stream(); }

	uint32_t read_bits(size_t bits);

private:
	binary_reader _reader;
	uint64_t _scratch = 0;
	size_t _scratch_bits = 0;
};

template <typename OutputIterator, typename = typename std::enable_if_t<sizeof(std::iterator_traits<OutputIterator>::value_type) == 1>>
size_t read_all(stream* stream, OutputIterator it);

template <typename InputIterator, typename = typename std::enable_if_t<sizeof(std::iterator_traits<InputIterator>::value_type) == 1>>
size_t read_all(InputIterator it, stream* stream);

size_t read_all(stream* istream, stream* ostream);

template <typename OutputIterator, typename = typename std::enable_if_t<sizeof(std::iterator_traits<OutputIterator>::value_type) == 1>>
size_t read_some(stream* stream, size_t size, OutputIterator it);

template <typename InputIterator, typename = typename std::enable_if_t<sizeof(std::iterator_traits<InputIterator>::value_type) == 1>>
size_t read_some(InputIterator it, size_t size, stream* stream);

size_t read_some(stream* istream, size_t size, stream* ostream);

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
