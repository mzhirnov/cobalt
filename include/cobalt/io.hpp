#ifndef COBALT_IO_HPP_INCLUDED
#define COBALT_IO_HPP_INCLUDED

#pragma once

#include <cobalt/io_fwd.hpp>

namespace cobalt { namespace io {

////////////////////////////////////////////////////////////////////////////////
// stream
//

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

////////////////////////////////////////////////////////////////////////////////
// stream_view
//

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
		break;
	case seek_origin::current: {
		auto position = tell(ec);
		if (position + offset < 0 || position + offset > _length)
			ec = std::make_error_code(std::errc::invalid_seek);
		else
			return _stream->seek(offset, origin, ec) - _offset;
		}
		break;
	case seek_origin::end:
		if (offset > 0 || -offset > _length)
			ec = std::make_error_code(std::errc::invalid_seek);
		else
			return _stream->seek(_offset + _length - offset, origin, ec) - _offset;
		break;
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

////////////////////////////////////////////////////////////////////////////////
// synchronized_stream
//

inline synchronized_stream::synchronized_stream(stream& stream) noexcept
	: _stream(&stream)
{
}

inline synchronized_stream::synchronized_stream(stream* stream)
	: _stream(stream)
	, _owning(true)
{
	counted_ptr<class stream> sp = _stream;
	
	BOOST_ASSERT(_stream != nullptr);
	if (!_stream)
		throw std::system_error(std::make_error_code(std::errc::invalid_argument), "stream");
	
	add_ref(_stream);
	
	sp.detach();
}

inline synchronized_stream::~synchronized_stream() noexcept {
	if (_owning)
		release(_stream);
}

inline size_t synchronized_stream::read(void* buffer, size_t size, std::error_code& ec) noexcept {
	std::lock_guard<std::mutex> lock(_mutex);
	return _stream->read(buffer, size, ec);
}

inline size_t synchronized_stream::write(const void* buffer, size_t size, std::error_code& ec) noexcept {
	std::lock_guard<std::mutex> lock(_mutex);
	return _stream->write(buffer, size, ec);
}

inline void synchronized_stream::flush(std::error_code& ec) const noexcept {
	std::lock_guard<std::mutex> lock(_mutex);
	_stream->flush(ec);
}

inline int64_t synchronized_stream::seek(int64_t offset, seek_origin origin, std::error_code& ec) noexcept {
	std::lock_guard<std::mutex> lock(_mutex);
	return _stream->seek(offset, origin, ec);
}

inline int64_t synchronized_stream::tell(std::error_code& ec) const noexcept {
	std::lock_guard<std::mutex> lock(_mutex);
	return _stream->tell(ec);
}

inline bool synchronized_stream::eof(std::error_code& ec) const noexcept {
	std::lock_guard<std::mutex> lock(_mutex);
	return _stream->eof(ec);
}

////////////////////////////////////////////////////////////////////////////////
// memory_stream
//

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
	
	ec = (count == size) ?
		std::error_code() :
		std::make_error_code(std::errc::result_out_of_range);
	
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

////////////////////////////////////////////////////////////////////////////////
// functions
//

template <typename OutputIterator>
inline size_t read_all(stream* istream, OutputIterator it, std::error_code& ec) noexcept {
	static_assert(sizeof(std::iterator_traits<OutputIterator>::value_type) == sizeof(stream::value_type), "");
	
	BOOST_ASSERT(istream != nullptr);
	if (!istream) {
		ec = std::make_error_code(std::errc::invalid_argument);
		return 0;
	}
	
	ec = std::error_code();
	
	size_t count = 0;
	constexpr size_t buffer_size = 65536;
	stream::value_type buffer[buffer_size];
	
	while (!istream->eof(ec)) {
		if (ec) break;
		
		auto read = istream->read(buffer, buffer_size, ec);
		if (ec) break;
		
		for (int i = 0; i < read; ++i)
			*it++ = static_cast<typename std::iterator_traits<OutputIterator>::value_type>(buffer[i]);
		
		count += read;
	}
	
	return count;
}

inline size_t read_all(stream* istream, stream* ostream, std::error_code& ec) noexcept {
	BOOST_ASSERT(istream != nullptr);
	if (!istream) {
		ec = std::make_error_code(std::errc::invalid_argument);
		return 0;
	}
	
	BOOST_ASSERT(ostream != nullptr);
	if (!ostream) {
		ec = std::make_error_code(std::errc::invalid_argument);
		return 0;
	}
	
	ec = std::error_code();
	
	size_t count = 0;
	constexpr size_t buffer_size = 65536;
	stream::value_type buffer[buffer_size];
	
	while (!istream->eof(ec)) {
		if (ec) break;
		
		auto read = istream->read(buffer, buffer_size, ec);
		if (ec) break;
		
		count += ostream->write(buffer, read, ec);
		if (ec) break;
	}
	
	return count;
}

template <typename OutputIterator>
inline size_t read_some(stream* istream, size_t size, OutputIterator it, std::error_code& ec) noexcept {
	static_assert(sizeof(std::iterator_traits<OutputIterator>::value_type) == sizeof(stream::value_type), "");
	
	BOOST_ASSERT(istream != nullptr);
	if (!istream) {
		ec = std::make_error_code(std::errc::invalid_argument);
		return 0;
	}
	
	ec = std::error_code();
	
	size_t count = 0;
	constexpr size_t buffer_size = 65536;
	stream::value_type buffer[buffer_size];
	
	while (size != 0 && !istream->eof(ec)) {
		if (ec) break;
		
		auto read = istream->read(buffer, std::min(size, buffer_size), ec);
		if (ec) break;
		
		for (int i = 0; i < read; ++i)
			*it++ = static_cast<typename std::iterator_traits<OutputIterator>::value_type>(buffer[i]);
			
		size -= read;
		count += read;
	}
	
	return count;
}

inline size_t read_some(stream* istream, size_t size, stream* ostream, std::error_code& ec) noexcept {
	BOOST_ASSERT(istream != nullptr);
	if (!istream) {
		ec = std::make_error_code(std::errc::invalid_argument);
		return 0;
	}
	
	BOOST_ASSERT(ostream != nullptr);
	if (!ostream) {
		ec = std::make_error_code(std::errc::invalid_argument);
		return 0;
	}
	
	ec = std::error_code();
	
	size_t count = 0;
	constexpr size_t buffer_size = 65536;
	stream::value_type buffer[buffer_size];
	
	while (size != 0 && !istream->eof(ec)) {
		if (ec) break;
		
		auto read = istream->read(buffer, std::min(size, buffer_size), ec);
		if (ec) break;
		
		size -= read;
		
		count += ostream->write(buffer, read, ec);
		if (ec) break;
	}
	
	return count;
}

template <typename InputIterator>
inline size_t write(InputIterator begin, InputIterator end, stream* ostream, std::error_code& ec) noexcept {
	static_assert(sizeof(std::iterator_traits<InputIterator>::value_type) == sizeof(stream::value_type), "");
	
	BOOST_ASSERT(ostream != nullptr);
	if (!ostream) {
		ec = std::make_error_code(std::errc::invalid_argument);
		return 0;
	}
	
	ec = std::error_code();
	
	size_t count = 0;
	
	for (auto it = begin; it != end; ++it) {
		count += ostream->write(&(*it), sizeof(stream::value_type), ec);
		if (ec) break;
	}
	
	return count;
}

template <typename OutputIterator>
inline size_t copy(stream* istream, OutputIterator it, std::error_code& ec) noexcept {
	static_assert(sizeof(std::iterator_traits<OutputIterator>::value_type) == sizeof(stream::value_type), "");
	
	BOOST_ASSERT(istream != nullptr);
	if (!istream) {
		ec = std::make_error_code(std::errc::invalid_argument);
		return 0;
	}
	
	BOOST_ASSERT(istream->can_seek());
	istream->seek(0, seek_origin::begin, ec);
	if (ec)
		return 0;
	
	return read_all(istream, it, ec);
}

inline size_t copy(stream* istream, stream* ostream, std::error_code& ec) noexcept {
	BOOST_ASSERT(istream != nullptr);
	if (!istream) {
		ec = std::make_error_code(std::errc::invalid_argument);
		return 0;
	}
	
	BOOST_ASSERT(ostream != nullptr);
	if (!ostream) {
		ec = std::make_error_code(std::errc::invalid_argument);
		return 0;
	}
	
	BOOST_ASSERT(istream->can_seek());
	istream->seek(0, seek_origin::begin, ec);
	if (ec)
		return 0;
	
	return read_all(istream, ostream, ec);
}

}} // namespace cobalt::io

#endif // COBALT_IO_HPP_INCLUDED
