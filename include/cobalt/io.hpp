#ifndef COBALT_IO_HPP_INCLUDED
#define COBALT_IO_HPP_INCLUDED

#pragma once

#include <cobalt/io_fwd.hpp>

#include <iterator>
#include <type_traits>

namespace cobalt { namespace io {

////////////////////////////////////////////////////////////////////////////////
// stream
//

inline size_t stream::read(void* buffer, size_t size) {
	std::error_code ec;
	auto ret = read(buffer, size, ec);
	throw_if_error(ec);
	return ret;
}

inline size_t stream::write(const void* buffer, size_t size) {
	std::error_code ec;
	auto ret = write(buffer, size, ec);
	throw_if_error(ec);
	return ret;
}

inline void stream::flush() const {
	std::error_code ec;
	flush(ec);
	throw_if_error(ec);
}

inline int64_t stream::seek(int64_t offset, seek_origin origin) {
	std::error_code ec;
	auto ret = seek(offset, origin, ec);
	throw_if_error(ec);
	return ret;
}

inline int64_t stream::tell() const {
	std::error_code ec;
	auto ret = tell(ec);
	throw_if_error(ec);
	return ret;
}

inline bool stream::eof() const {
	std::error_code ec;
	auto ret = eof(ec);
	throw_if_error(ec);
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

inline void stream::copy_to(stream& stream, std::error_code& ec) noexcept {
	ec = std::error_code();
	
	constexpr size_t buffer_size = 65536;
	stream::value_type buffer[buffer_size];
	
	while (!eof(ec)) {
		if (ec) break;
		
		auto count = read(buffer, buffer_size, ec);
		if (ec) break;
		
		stream.write(buffer, count, ec);
		if (ec) break;
	}
}

inline void stream::copy_to(stream& stream, size_t max_size, std::error_code& ec) noexcept {
	if (!can_read()) {
		ec = std::make_error_code(std::errc::operation_not_supported);
		return;
	}
	
	if (!stream.can_write()) {
		ec = std::make_error_code(std::errc::operation_not_supported);
		return;
	}
	
	ec = std::error_code();
	
	constexpr size_t buffer_size = 65536;
	stream::value_type buffer[buffer_size];
	
	while (!eof(ec) && max_size > 0) {
		if (ec) break;
		
		auto count = read(buffer, std::min(buffer_size, max_size), ec);
		if (ec) break;
		
		stream.write(buffer, count, ec);
		if (ec) break;
		
		max_size -= count;
	}
}

inline void stream::copy_to(stream& stream) {
	std::error_code ec;
	copy_to(stream, ec);
	throw_if_error(ec);
}

inline void stream::copy_to(stream& stream, size_t max_size) {
	std::error_code ec;
	copy_to(stream, max_size, ec);
	throw_if_error(ec);
}

////////////////////////////////////////////////////////////////////////////////
// memory_stream
//

inline memory_stream::memory_stream(size_t capacity)
	: _vec(new std::vector<value_type>())
	, _access(access_mode::read_write)
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
	
	ec = std::error_code();
	
	return size;
}

inline size_t memory_stream::write_impl(const void* buffer, size_t size, std::error_code& ec, std::false_type) noexcept {
	size_t count = std::min(_size - _position, size);
	if (count > 0) {
		BOOST_ASSERT(buffer != nullptr);
		memcpy(_buffer + _position, buffer, count);
		_position += count;
	}
	
	ec = (count == size) ?
		std::error_code() :
		std::make_error_code(std::errc::result_out_of_range);

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

////////////////////////////////////////////////////////////////////////////////
// file_stream
//

inline file_stream::~file_stream() noexcept {
	std::error_code ec;
	close(ec);
	BOOST_ASSERT(!ec);
}

inline void file_stream::open(const char* filename, open_mode mode, access_mode access, std::error_code& ec) noexcept {
	close(ec);
	BOOST_ASSERT(!ec);
	
	_access = access;
	
	ec = std::error_code();
	
	switch (access) {
	case access_mode::read_only:
		switch (mode) {
		case open_mode::open:
			_fp = std::fopen(filename, "rb");
			break;
		default: // Skip all create modes
			ec = std::make_error_code(std::errc::invalid_argument);
			break;
		}
		break;
	case access_mode::read_write:
		switch (mode) {
		case open_mode::create:
			_fp = std::fopen(filename, "w+b");
			break;
		case open_mode::create_new:
			if ((_fp = std::fopen(filename, "rb"))) {
				close(ec);
				BOOST_ASSERT(!ec);
				ec = std::make_error_code(std::errc::file_exists);
			} else {
				_fp = std::fopen(filename, "w+b");
			}
			break;
		case open_mode::open:
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
	throw_if_error(ec);
}

inline void file_stream::close(std::error_code& ec) noexcept {
	ec = std::error_code();
	if (_fp) {
		if (std::fclose(_fp) != 0)
			ec = std::make_error_code(std::errc::bad_file_descriptor);
		_fp = nullptr;
	}
}

inline void file_stream::close() {
	std::error_code ec;
	close(ec);
	throw_if_error(ec);
}

inline bool file_stream::valid() const noexcept {
	return _fp != nullptr;
}

inline access_mode file_stream::access() const noexcept {
	return _access;
}

inline size_t file_stream::read(void* buffer, size_t size, std::error_code& ec) noexcept {
	BOOST_ASSERT(valid());
	if (!valid()) {
		ec = std::make_error_code(std::errc::bad_file_descriptor);
		return 0;
	}
	
	ec = std::error_code();
	
	auto read = std::fread(buffer, 1, size, _fp);
	if (read < size && std::ferror(_fp))
		ec = std::make_error_code(std::errc::io_error);
	
	return read;
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
	
	ec = std::error_code();
	
	auto written = std::fwrite(buffer, 1, size, _fp);
	if (written < size)
		ec = std::make_error_code(std::errc::io_error);
	
	return written;
}

inline void file_stream::flush(std::error_code& ec) const noexcept {
	BOOST_ASSERT(valid());
	if (!valid()) {
		ec = std::make_error_code(std::errc::bad_file_descriptor);
		return;
	}
	
	ec = std::error_code();
	
	if (std::fflush(_fp) != 0)
		ec = std::make_error_code(std::errc::io_error);
}

inline int64_t file_stream::seek(int64_t offset, seek_origin origin, std::error_code& ec) noexcept {
	BOOST_ASSERT(valid());
	if (!valid()) {
		ec = std::make_error_code(std::errc::bad_file_descriptor);
		return 0;
	}
	
	if (origin != seek_origin::begin && offset != 0) {
		auto pos = std::ftell(_fp);
		if (pos == -1) {
			ec = std::error_code(errno, std::generic_category());
			return 0;
		}
		
		if (std::fseek(_fp, 0, SEEK_END) != 0) {
			ec = std::make_error_code(std::errc::invalid_seek);
			return pos;
		}
		
		auto length = std::ftell(_fp);
		if (length == -1) {
			ec = std::error_code(errno, std::generic_category());
			return length;
		}
		
		if (std::fseek(_fp, pos, SEEK_SET) != 0) {
			ec = std::make_error_code(std::errc::invalid_seek);
			return length;
		}
		
		if ((origin == seek_origin::begin && offset > length) ||
			(origin == seek_origin::current && pos + offset > length) ||
			(origin == seek_origin::end && offset > 0))
		{
			ec = std::make_error_code(std::errc::invalid_seek);
			return pos;
		}
	}
	
	ec = std::error_code();
	
	// Clear error and EOF flags to make rewinding possible
	std::clearerr(_fp);
	
	int Origin =
		(origin == seek_origin::begin) ? SEEK_SET :
		(origin == seek_origin::current) ? SEEK_CUR : SEEK_END;

	if (std::fseek(_fp, static_cast<long>(offset), Origin) != 0)
		ec = std::make_error_code(std::errc::invalid_seek);
	
	return std::ftell(_fp);
}

inline int64_t file_stream::tell(std::error_code& ec) const noexcept {
	BOOST_ASSERT(valid());
	if (!valid()) {
		ec = std::make_error_code(std::errc::bad_file_descriptor);
		return 0;
	}
	
	ec = std::error_code();
	
	auto pos = std::ftell(_fp);
	if (pos == -1)
		ec = std::error_code(errno, std::generic_category());
	
	return pos;
}

inline bool file_stream::eof(std::error_code& ec) const noexcept {
	BOOST_ASSERT(valid());
	if (!valid()) {
		ec = std::make_error_code(std::errc::bad_file_descriptor);
		return 0;
	}
	
	ec = std::error_code();
	
	auto ret = std::feof(_fp);
	if (std::ferror(_fp))
		ec = std::make_error_code(std::errc::io_error);
	
	return ret != 0;
}

namespace detail {

inline stream_holder::stream_holder(stream& stream)
	: _stream(&stream)
{
}

stream_holder::stream_holder(stream* stream)
	: _stream(stream)
	, _owning(true)
{
	BOOST_ASSERT(_stream != nullptr);
	if (_stream)
		retain(_stream);
}

stream_holder::~stream_holder() {
	if (_stream && _owning)
		release(_stream);
}

} // namespace detail

////////////////////////////////////////////////////////////////////////////////
// stream_view
//

inline stream_view::stream_view(stream& stream, int64_t offset, int64_t length)
	: stream_holder(stream)
	, _offset(offset)
	, _length(length)
{
	std::error_code ec;
	base_stream()->seek(_offset, seek_origin::begin, ec);
	throw_if_error(ec);
}

inline stream_view::stream_view(stream* stream, int64_t offset, int64_t length)
	: stream_holder(stream)
	, _offset(offset)
	, _length(length)
{
	if (!base_stream())
		throw std::system_error(std::make_error_code(std::errc::invalid_argument), "stream");

	BOOST_ASSERT(_offset >= 0);
	if (_offset < 0)
		throw std::system_error(std::make_error_code(std::errc::invalid_argument), "offset");
	
	BOOST_ASSERT(_length >= 0);
	if (_length < 0)
		throw std::system_error(std::make_error_code(std::errc::invalid_argument), "length");
	
	std::error_code ec;
	base_stream()->seek(_offset, seek_origin::begin, ec);
	throw_if_error(ec);
}

inline size_t stream_view::read(void* buffer, size_t size, std::error_code& ec) noexcept {
	auto position = tell(ec);
	if (ec)
		return static_cast<size_t>(position);
	auto count = std::min<size_t>(_length - position, size);
	return base_stream()->read(buffer, count, ec);
}

inline size_t stream_view::write(const void* buffer, size_t size, std::error_code& ec) noexcept {
	auto position = tell(ec);
	if (ec)
		return static_cast<size_t>(position);
	auto count = std::min<size_t>(_length - position, size);
	return base_stream()->write(buffer, count, ec);
}

inline void stream_view::flush(std::error_code& ec) const noexcept {
	base_stream()->flush(ec);
}

inline int64_t stream_view::seek(int64_t offset, seek_origin origin, std::error_code& ec) noexcept {
	int64_t oldpos = tell(ec);
	int64_t newpos = 0;
	
	if (ec)
		return oldpos;
	
	switch (origin) {
	case seek_origin::begin:
		newpos = offset;
		break;
	case seek_origin::current:
		newpos = oldpos + offset;
		break;
	case seek_origin::end:
		newpos = _length + offset;
		break;
	}
	
	if (newpos < 0 || newpos > _length) {
		ec = std::make_error_code(std::errc::invalid_seek);
		return oldpos;
	}
	
	return base_stream()->seek(newpos + _offset, seek_origin::begin, ec) - _offset;
}

inline int64_t stream_view::tell(std::error_code& ec) const noexcept {
	return base_stream()->tell(ec) - _offset;
}

inline bool stream_view::eof(std::error_code& ec) const noexcept {
	return base_stream()->tell(ec) >= _offset + _length;
}

////////////////////////////////////////////////////////////////////////////////
// Functions
//

template <typename OutputIterator>
inline size_t copy(stream& stream, OutputIterator it, std::error_code& ec) noexcept {
	ec = std::error_code();
	
	size_t count = 0;
	constexpr size_t buffer_size = 65536;
	stream::value_type buffer[buffer_size];
	
	while (!stream.eof(ec)) {
		if (ec) break;
		
		auto read = stream.read(buffer, buffer_size, ec);
		if (ec) break;
		
		for (int i = 0; i < read; ++i, ++count)
			*it++ = buffer[i];
	}
	
	return count;
}

template <typename OutputIterator>
inline size_t copy(stream& stream, OutputIterator it, size_t max_bytes, std::error_code& ec) noexcept {
	ec = std::error_code();
	
	size_t count = 0;
	constexpr size_t buffer_size = 65536;
	stream::value_type buffer[buffer_size];
	
	while (max_bytes != 0 && !stream.eof(ec)) {
		if (ec) break;
		
		auto read = stream.read(buffer, std::min(buffer_size, max_bytes), ec);
		if (ec) break;
		
		for (int i = 0; i < read; ++i, ++count)
			*it++ = buffer[i];
			
		max_bytes -= read;
	}
	
	return count;
}

template <typename InputIterator>
inline size_t copy(InputIterator begin, InputIterator end, stream& stream, std::error_code& ec) noexcept {
	ec = std::error_code();
	
	size_t count = 0;
	
	for (auto it = begin; it != end; ++it) {
		count += stream.write(std::addressof(*it), sizeof(stream::value_type), ec);
		if (ec) break;
	}
	
	return count;
}

template <typename OutputIterator>
inline size_t copy(stream& stream, OutputIterator it) {
	std::error_code ec;
	auto ret = copy(stream, it, ec);
	throw_if_error(ec);
	return ret;
}

template <typename OutputIterator>
inline size_t copy(stream& stream, OutputIterator it, size_t max_bytes) {
	std::error_code ec;
	auto ret = copy(stream, max_bytes, it, ec);
	throw_if_error(ec);
	return ret;
}

template <typename InputIterator>
inline size_t copy(InputIterator begin, InputIterator end, stream& stream) {
	std::error_code ec;
	auto ret = copy(begin, end, stream, ec);
	throw_if_error(ec);
	return ret;
}

////////////////////////////////////////////////////////////////////////////////
// binary_writer
//

inline binary_writer::binary_writer(stream& stream) noexcept
	: _stream(&stream)
{
}

inline binary_writer::binary_writer(stream* stream) noexcept
	: _stream(stream)
	, _owning(true)
{
	BOOST_ASSERT(_stream != nullptr);
	if (!_stream)
		throw std::system_error(std::make_error_code(std::errc::invalid_argument), "stream");
	
	retain(_stream);
}

inline binary_writer::~binary_writer() {
	if (_owning)
		release(_stream);
}

inline void binary_writer::write(uint8_t value, std::error_code& ec) noexcept {
	_stream->write(&value, sizeof(uint8_t), ec);
}

inline void binary_writer::write(uint16_t value, std::error_code& ec) noexcept {
	write(static_cast<uint8_t>( value        & 0xff), ec); if (ec) return;
	write(static_cast<uint8_t>((value >>  8) & 0xff), ec);
}

inline void binary_writer::write(uint32_t value, std::error_code& ec) noexcept {
	write(static_cast<uint8_t>( value        & 0xff), ec); if (ec) return;
	write(static_cast<uint8_t>((value >>  8) & 0xff), ec); if (ec) return;
	write(static_cast<uint8_t>((value >> 16) & 0xff), ec); if (ec) return;
	write(static_cast<uint8_t>((value >> 24) & 0xff), ec);
}

inline void binary_writer::write(uint64_t value, std::error_code& ec) noexcept {
	write(static_cast<uint8_t>( value        & 0xff), ec); if (ec) return;
	write(static_cast<uint8_t>((value >>  8) & 0xff), ec); if (ec) return;
	write(static_cast<uint8_t>((value >> 16) & 0xff), ec); if (ec) return;
	write(static_cast<uint8_t>((value >> 24) & 0xff), ec); if (ec) return;
	write(static_cast<uint8_t>((value >> 32) & 0xff), ec); if (ec) return;
	write(static_cast<uint8_t>((value >> 40) & 0xff), ec); if (ec) return;
	write(static_cast<uint8_t>((value >> 48) & 0xff), ec); if (ec) return;
	write(static_cast<uint8_t>((value >> 56) & 0xff), ec);
}

inline void binary_writer::write(int8_t value, std::error_code& ec) noexcept {
	write(static_cast<uint8_t>(value), ec);
}

inline void binary_writer::write(int16_t value, std::error_code& ec) noexcept {
	write(static_cast<uint16_t>(value), ec);
}

inline void binary_writer::write(int32_t value, std::error_code& ec) noexcept {
	write(static_cast<uint32_t>(value), ec);
}

inline void binary_writer::write(int64_t value, std::error_code& ec) noexcept {
	write(static_cast<uint64_t>(value), ec);
}

inline void binary_writer::write(float value, std::error_code& ec) noexcept {
	union float_to_uint32 {
		float f;
		uint32_t i;
	} f2i;
	
	f2i.f = value;
	
	write(f2i.i, ec);
}

inline void binary_writer::write(double value, std::error_code& ec) noexcept {
	union double_to_uint64 {
		double d;
		uint64_t i;
	} d2i;
	
	d2i.d = value;
	
	write(d2i.i, ec);
}

inline void binary_writer::write(bool value, std::error_code& ec) noexcept {
	uint8_t b = value ? 1 : 0;
	write(b, ec);
}

inline void binary_writer::write_7bit_encoded_int(uint32_t value, std::error_code& ec) noexcept {
	while (value >= 0x80) {
		write(static_cast<uint8_t>(value | 0x80), ec);
		if (ec)
			return;
		
		value >>= 7;
	}
	
	write(static_cast<uint8_t>(value), ec);
}

inline void binary_writer::write_unicode_char(uint32_t cp, std::error_code& ec) noexcept {
	BOOST_ASSERT(cp <= 0x10FFFF);
	if (cp > 0x10FFFF) {
		ec = std::make_error_code(std::errc::argument_out_of_domain);
		return;
	}
	
	if (cp < 0x80) {
		write(static_cast<uint8_t>(cp), ec);
	} else if (cp < 0x800) {
		write(static_cast<uint8_t>(cp >> 6)           | 0xC0, ec); if (ec) return;
		write(static_cast<uint8_t>(cp & 0x3F)         | 0x80, ec);
	} else if (cp < 0x10000) {
		write(static_cast<uint8_t>(cp >> 12)          | 0xE0, ec); if (ec) return;
		write(static_cast<uint8_t>((cp >> 6) & 0x3F)  | 0x80, ec); if (ec) return;
		write(static_cast<uint8_t>(cp & 0x3F)         | 0x80, ec);
	} else {
		write(static_cast<uint8_t>(cp >> 18)          | 0xF0, ec); if (ec) return;
		write(static_cast<uint8_t>((cp >> 12) & 0x3F) | 0x80, ec); if (ec) return;
		write(static_cast<uint8_t>((cp >> 6) & 0x3F)  | 0x80, ec); if (ec) return;
		write(static_cast<uint8_t>(cp & 0x3F)         | 0x80, ec);
	}
}

inline void binary_writer::write_c_string(const char* str, std::error_code& ec) noexcept {
	BOOST_ASSERT(str != nullptr);
	if (!str) {
		ec = std::make_error_code(std::errc::invalid_argument);
		return;
	}
	
	while (*str++) {
		write(*str, ec);
		if (ec)
			return;
	}
	
	write(*str, ec);
}

inline void binary_writer::write_pascal_string(const char* str, std::error_code& ec) noexcept {
	BOOST_ASSERT(str != nullptr);
	if (!str) {
		ec = std::make_error_code(std::errc::invalid_argument);
		return;
	}
	
	write_7bit_encoded_int(static_cast<uint32_t>(std::strlen(str)), ec);
	
	while (*str++) {
		write(*str, ec);
		if (ec)
			return;
	}
}

inline void binary_writer::write(uint8_t value) {
	std::error_code ec;
	write(value, ec);
	throw_if_error(ec);
}

inline void binary_writer::write(uint16_t value) {
	std::error_code ec;
	write(value, ec);
	throw_if_error(ec);
}

inline void binary_writer::write(uint32_t value) {
	std::error_code ec;
	write(value, ec);
	throw_if_error(ec);
}

inline void binary_writer::write(uint64_t value) {
	std::error_code ec;
	write(value, ec);
	throw_if_error(ec);
}

inline void binary_writer::write(int8_t value) {
	std::error_code ec;
	write(value, ec);
	throw_if_error(ec);
}

inline void binary_writer::write(int16_t value) {
	std::error_code ec;
	write(value, ec);
	throw_if_error(ec);
}

inline void binary_writer::write(int32_t value) {
	std::error_code ec;
	write(value, ec);
	throw_if_error(ec);
}

inline void binary_writer::write(int64_t value) {
	std::error_code ec;
	write(value, ec);
	throw_if_error(ec);
}

inline void binary_writer::write(float value) {
	std::error_code ec;
	write(value, ec);
	throw_if_error(ec);
}

inline void binary_writer::write(double value) {
	std::error_code ec;
	write(value, ec);
	throw_if_error(ec);
}

inline void binary_writer::write(bool value) {
	std::error_code ec;
	write(value, ec);
	throw_if_error(ec);
}

inline void binary_writer::write_7bit_encoded_int(uint32_t value) {
	std::error_code ec;
	write_7bit_encoded_int(value, ec);
	throw_if_error(ec);
}

inline void binary_writer::write_unicode_char(uint32_t value) {
	std::error_code ec;
	write_unicode_char(value, ec);
	throw_if_error(ec);
}

inline void binary_writer::write_c_string(const char* str) {
	std::error_code ec;
	write_c_string(str, ec);
	throw_if_error(ec);
}

inline void binary_writer::write_pascal_string(const char* str) {
	std::error_code ec;
	write_pascal_string(str, ec);
	throw_if_error(ec);
}

////////////////////////////////////////////////////////////////////////////////
// binary_reader
//

inline binary_reader::binary_reader(stream& stream) noexcept
	: _stream(&stream)
{
}

inline binary_reader::binary_reader(stream* stream) noexcept
	: _stream(stream)
	, _owning(true)
{
	BOOST_ASSERT(_stream != nullptr);
	if (!_stream)
		throw std::system_error(std::make_error_code(std::errc::invalid_argument), "stream");
	
	retain(_stream);
}

inline binary_reader::~binary_reader() {
	if (_owning)
		release(_stream);
}

inline uint8_t binary_reader::read_uint8(std::error_code& ec) noexcept {
	uint8_t result = 0;
	_stream->read(&result, sizeof(uint8_t), ec);
	return result;
}

inline uint16_t binary_reader::read_uint16(std::error_code& ec) noexcept {
	uint16_t b0 = read_uint8(ec); if (ec) return 0;
	uint16_t b1 = read_uint8(ec);
	return (b1 << 8) | b0;
}

inline uint32_t binary_reader::read_uint32(std::error_code& ec) noexcept {
	uint32_t w0 = read_uint16(ec); if (ec) return 0;
	uint32_t w1 = read_uint16(ec);
	return (w1 << 16) | w0;
}

inline uint64_t binary_reader::read_uint64(std::error_code& ec) noexcept {
	uint64_t dw0 = read_uint32(ec); if (ec) return 0;
	uint64_t dw1= read_uint32(ec);
	return (dw1 << 32) | dw0;
}

inline int8_t binary_reader::read_int8(std::error_code& ec) noexcept {
	return static_cast<int8_t>(read_uint8(ec));
}

inline int16_t binary_reader::read_int16(std::error_code& ec) noexcept {
	return static_cast<int16_t>(read_uint16(ec));
}

inline int32_t binary_reader::read_int32(std::error_code& ec) noexcept {
	return static_cast<int32_t>(read_uint32(ec));
}

inline int64_t binary_reader::read_int64(std::error_code& ec) noexcept {
	return static_cast<int64_t>(read_uint64(ec));
}

inline float binary_reader::read_float(std::error_code& ec) noexcept {
	union float_to_uint32 {
		float f;
		uint32_t i;
	} f2i;
	
	f2i.i = read_uint32(ec);
	
	return f2i.f;
}

inline double binary_reader::read_double(std::error_code& ec) noexcept {
	union double_to_uint64 {
		double d;
		uint64_t i;
	} d2i;
	
	d2i.i = read_uint64(ec);
	
	return d2i.d;
}

inline bool binary_reader::read_bool(std::error_code& ec) noexcept {
	return read_uint8(ec) != 0;
}

inline uint32_t binary_reader::read_7bit_encoded_int(std::error_code& ec) noexcept {
	uint32_t result = 0;
	uint32_t bits_read = 0;
	uint32_t value = 0;

	do {
		value = read_uint8(ec);
		if (ec)
			return 0;
		
		result |= (value & 0x7f) << bits_read;
		bits_read += 7;
	} while (value & 0x80);

	return result;
}

inline uint32_t binary_reader::read_unicode_char(std::error_code& ec) noexcept {
	auto b0 = read_uint8(ec); if (ec) return 0;
	
	if (b0 < 0x80) {
		return b0;
	} else if ((b0 >> 5) == 0x06) {
		auto b1 = read_uint8(ec); if (ec) return 0;
		return ((b0 << 6) & 0x7FF) | (b1 & 0x3F);
	} else if ((b0 >> 4) == 0x0E) {
		auto b1 = read_uint8(ec); if (ec) return 0;
		auto b2 = read_uint8(ec); if (ec) return 0;
		return ((b0 << 12) & 0xFFFF) | ((b1 << 6) & 0xFFF) | (b2 & 0x3F);
	} else if ((b0 >> 3) == 0x1e) {
		auto b1 = read_uint8(ec); if (ec) return 0;
		auto b2 = read_uint8(ec); if (ec) return 0;
		auto b3 = read_uint8(ec); if (ec) return 0;
		return ((b0 << 18) & 0x1FFFFF) | ((b1 << 12) & 0x3FFFF) | ((b2 << 6) & 0xFFF) | (b3 & 0x3F);
	} else {
		ec = std::make_error_code(std::errc::illegal_byte_sequence);
		return 0;
	}
}

inline std::string binary_reader::read_c_string(std::error_code& ec) noexcept {
	std::string result;
	
	char c = read_uint8(ec);
	while (!ec && c) {
		result.append(1, c);
		c = read_uint8(ec);
	};
	
	return  result;
}

inline std::string binary_reader::read_pascal_string(std::error_code& ec) noexcept {
	std::string result;
	
	auto size = read_7bit_encoded_int(ec);
	result.reserve(size);
	
	for (int i = 0; i < size && !ec; ++i)
		result.append(1, read_uint8(ec));
	
	return result;
}

inline uint8_t binary_reader::read_uint8() {
	std::error_code ec;
	auto ret = read_uint8(ec);
	throw_if_error(ec);
	return ret;
}

inline uint16_t binary_reader::read_uint16() {
	std::error_code ec;
	auto ret = read_uint16(ec);
	throw_if_error(ec);
	return ret;
}

inline uint32_t binary_reader::read_uint32() {
	std::error_code ec;
	auto ret = read_uint32(ec);
	throw_if_error(ec);
	return ret;
}

inline uint64_t binary_reader::read_uint64() {
	std::error_code ec;
	auto ret = read_uint64(ec);
	throw_if_error(ec);
	return ret;
}

inline int8_t binary_reader::read_int8() {
	std::error_code ec;
	auto ret = read_int8(ec);
	throw_if_error(ec);
	return ret;
}

inline int16_t binary_reader::read_int16() {
	std::error_code ec;
	auto ret = read_int16(ec);
	throw_if_error(ec);
	return ret;
}

inline int32_t binary_reader::read_int32() {
	std::error_code ec;
	auto ret = read_int32(ec);
	throw_if_error(ec);
	return ret;
}

inline int64_t binary_reader::read_int64() {
	std::error_code ec;
	auto ret = read_int64(ec);
	throw_if_error(ec);
	return ret;
}

inline float binary_reader::read_float() {
	std::error_code ec;
	auto ret = read_float(ec);
	throw_if_error(ec);
	return ret;
}

inline double binary_reader::read_double() {
	std::error_code ec;
	auto ret = read_double(ec);
	throw_if_error(ec);
	return ret;
}

inline bool binary_reader::read_bool() {
	std::error_code ec;
	auto ret = read_bool(ec);
	throw_if_error(ec);
	return ret;
}

inline uint32_t binary_reader::read_7bit_encoded_int() {
	std::error_code ec;
	auto ret = read_7bit_encoded_int(ec);
	throw_if_error(ec);
	return ret;
}

inline uint32_t binary_reader::read_unicode_char() {
	std::error_code ec;
	auto ret = read_unicode_char(ec);
	throw_if_error(ec);
	return ret;
}

inline std::string binary_reader::read_c_string() {
	std::error_code ec;
	auto ret = read_c_string(ec);
	throw_if_error(ec);
	return ret;
}

inline std::string binary_reader::read_pascal_string() {
	std::error_code ec;
	auto ret = read_pascal_string(ec);
	throw_if_error(ec);
	return ret;
}

////////////////////////////////////////////////////////////////////////////////
// bitpack_writer
//

inline bitpack_writer::bitpack_writer(stream& stream) noexcept
	: _writer(&stream)
{
}

inline bitpack_writer::bitpack_writer(stream* stream) noexcept
	: _writer(stream)
{
}

inline bitpack_writer::~bitpack_writer() {
	std::error_code ec;
	flush(ec);
	BOOST_ASSERT(!ec);
}

inline void bitpack_writer::write_bits(uint32_t value, size_t bits, std::error_code& ec) noexcept {
	BOOST_ASSERT(bits <= 32);
	if (bits > 32) {
		ec = std::make_error_code(std::errc::argument_out_of_domain);
		return;
	}

	_scratch |= static_cast<uint64_t>(value & ((1 << bits) - 1)) << _scratch_bits;
	_scratch_bits += bits;

	if (_scratch_bits >= 32) {
		_writer.write(static_cast<uint32_t>(_scratch & 0xffffffff), ec);
		_scratch >>= 32;
		_scratch_bits -= 32;
	}
}

inline void bitpack_writer::flush(std::error_code& ec) {
	if (_scratch_bits > 0) {
		int bytes = (_scratch_bits >> 3) + (_scratch_bits & 0x7) ? 1 : 0;
		switch (bytes) {
		case 1:
			_writer.write(static_cast<uint8_t>(_scratch & 0xff), ec);
			break;
		case 2:
			_writer.write(static_cast<uint16_t>(_scratch & 0xff), ec);
			break;
		case 3:
			_writer.write(static_cast<uint8_t>( _scratch        & 0xff), ec); if (ec) return;
			_writer.write(static_cast<uint8_t>((_scratch >>  8) & 0xff), ec); if (ec) return;
			_writer.write(static_cast<uint8_t>((_scratch >> 16) & 0xff), ec);
			break;
		case 4:
			_writer.write(static_cast<uint32_t>(_scratch & 0xffffffff), ec);
			break;
		}
		_scratch = 0;
		_scratch_bits = 0;
	}
}

inline void bitpack_writer::write_bits(uint32_t value, size_t bits) {
	std::error_code ec;
	write_bits(value, bits, ec);
	throw_if_error(ec);
}

inline void bitpack_writer::flush() {
	std::error_code ec;
	flush(ec);
	throw_if_error(ec);
}

////////////////////////////////////////////////////////////////////////////////
// bitpack_reader
//

inline bitpack_reader::bitpack_reader(stream& stream) noexcept
	: _reader(stream)
{
}

inline bitpack_reader::bitpack_reader(stream* stream) noexcept
	: _reader(stream)
{
}

inline bitpack_reader::~bitpack_reader() noexcept {
	BOOST_ASSERT(_scratch == 0);
	BOOST_ASSERT(_scratch_bits == 0);
}

inline uint32_t bitpack_reader::read_bits(size_t bits, std::error_code& ec) noexcept {
	BOOST_ASSERT(bits <= 32);
	if (bits > 32) {
		ec = std::make_error_code(std::errc::argument_out_of_domain);
		return 0;
	}

	while (_scratch_bits < bits) {
		auto value = _reader.read_uint8(ec);
		_scratch |= value << _scratch_bits;
		_scratch_bits += 8;
	}

	uint32_t value = _scratch & ((1 << bits) - 1);

	_scratch >>= bits;
	_scratch_bits -= bits;

	return value;
}

inline void bitpack_reader::align(std::error_code& ec) noexcept {
	ec = std::error_code();
	
	BOOST_ASSERT(_scratch == 0);
	if (_scratch != 0) {
		ec = std::make_error_code(std::errc::illegal_byte_sequence);
		return;
	}
	
	_scratch_bits = 0;
}

inline uint32_t bitpack_reader::read_bits(size_t bits) {
	std::error_code ec;
	auto ret = read_bits(bits, ec);
	throw_if_error(ec);
	return ret;
}

inline void bitpack_reader::align() {
	std::error_code ec;
	align(ec);
	throw_if_error(ec);
}

////////////////////////////////////////////////////////////////////////////////
//
//

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
