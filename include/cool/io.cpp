#include "pch.h"
#include "cool/io.hpp"
#include "cool/platform.hpp"

#include <stdexcept>

#include <boost/exception/all.hpp>

#ifdef TARGET_PLATFORM_ANDROID

#include "cool/application.hpp"

#include <android/asset_manager.h>
#include <android_native_app_glue.h>

#endif // TARGET_PLATFORM_ANDROID

namespace io {

/// memory range input/output stream
class memory_range_stream : public stream {
public:
	memory_range_stream(void* begin, void* end, file_access access)
		: _begin(reinterpret_cast<uint8_t*>(begin))
		, _end(reinterpret_cast<uint8_t*>(end))
		, _cur(_begin)
		, _access(access)
	{
		BOOST_ASSERT(_begin < _end);
	}

	void seek(int pos, origin o = origin::beginning) {
		switch (o) {
		case origin::beginning:
			_cur = std::max(_begin, std::min(_begin + pos, _end));
			break;
		case origin::current:
			_cur = std::max(_begin, std::min(_cur + pos, _end));
			break;
		case origin::end:
			_cur = std::max(_begin, std::min(_end - std::abs(pos), _end));
			break;
		}
	}

	size_t tell() const {
		return _cur - _begin;
	}

	size_t read(void* buffer, size_t length) {
		size_t size = std::min((size_t)(_end - _cur), length);
		if (size > 0) {
			memcpy(buffer, _cur, size);
			_cur += size;
		}

		return size;
	}

	bool eof() const {
		return _cur == _end;
	}

	size_t write(const void* buffer, size_t length) {
		if (_access != file_access::read_write) {
			BOOST_THROW_EXCEPTION(std::runtime_error("cannot write to stream"));
		}

		size_t size = std::min((size_t)(_end - _cur), length);
		if (size > 0) {
			memcpy(_cur, buffer, size);
			_cur += size;
		}

		return size;
	}

	void flush() const {}

private:
	uint8_t* _begin;
	uint8_t* _end;
	mutable uint8_t* _cur;
	file_access _access;
};

/// dynamic memory input/output stream
class dynamic_memory_stream : public stream {
public:
	dynamic_memory_stream()
		: _pos(0)
	{
	}

	void seek(int pos, origin o = origin::beginning) {
		switch (o) {
		case origin::beginning:
			_pos = std::max(0u, std::min(std::abs(pos) + 0u, _data.size()));
			break;
		case origin::current:
			_pos = std::max(0u, std::min(_pos + pos, _data.size()));
			break;
		case origin::end:
			_pos = std::max(0u, std::min(_data.size() - std::abs(pos), _data.size()));
			break;
		}
	}

	size_t tell() const {
		return _pos;
	}

	size_t read(void* buffer, size_t length) {
		size_t size = std::min(_data.size() - _pos, length);
		if (size > 0) {
			memcpy(buffer, &_data[_pos], size);
			_pos += size;
		}

		return size;
	}

	bool eof() const {
		return _pos == _data.size();
	}

	size_t write(const void* buffer, size_t length) {
		size_t size = std::min(_data.size() - _pos, length);
		if (size > 0) {
			memcpy(&_data[_pos], buffer, size);
			_pos += size;
		}

		// new size beyond the end
		size = length - size;
		if (size > 0) {
			const uint8_t* p = reinterpret_cast<const uint8_t*>(buffer) + length - size;
			_data.insert(_data.end(), p, p + size);
			_pos += size;
		}

		return length;
	}

	void flush() const {}

private:
	std::vector<uint8_t> _data;
	mutable size_t _pos;
};

/// file input/output stream
class file_stream : public stream {
public:
	file_stream(const char* filename, file_mode open_mode = file_mode::open_existing, file_access access = file_access::read_write)
	{
		if (open_mode == file_mode::create_always && access == file_access::read_only) {
			BOOST_THROW_EXCEPTION(std::invalid_argument("cannot create read-only file"));
		}

		const char* Mode =
			access == file_access::read_only ? "rb" : /* read_write */
				open_mode == file_mode::open_existing ? "r+b" : /* create_always */ "w+b";

		_fp = fopen(filename, Mode);
		if (!_fp) {
			BOOST_THROW_EXCEPTION(std::runtime_error(strerror(errno)));
		}
	}

	~file_stream() {
		if (_fp) {
			fflush(_fp);
			fclose(_fp);
		}
	}

	void seek(int pos, origin o = origin::beginning) {
		int Origin =
			o == origin::beginning ? SEEK_SET :
			o == origin::current ? SEEK_CUR : SEEK_END;

		if (fseek(_fp, pos, Origin) != 0) {
			BOOST_THROW_EXCEPTION(std::runtime_error(strerror(errno)));
		}
	}


	size_t tell() const {
		return ftell(_fp);
	}

	size_t read(void* buffer, size_t length) {
		size_t size = fread(buffer, 1, length, _fp);
		if (ferror(_fp)) {
			BOOST_THROW_EXCEPTION(std::runtime_error(strerror(errno)));
		}

		return size;
	}

	bool eof() const {
		return !!feof(_fp);
	}

	size_t write(const void* buffer, size_t length) {
		size_t size = fwrite(buffer, 1, length, _fp);
		if (ferror(_fp)) {
			BOOST_THROW_EXCEPTION(std::runtime_error(strerror(errno)));
		}

		return size;
	}

	void flush() const {
		fflush(_fp);
		if (ferror(_fp)) {
			BOOST_THROW_EXCEPTION(std::runtime_error(strerror(errno)));
		}
	}

private:
	FILE* _fp;

private:
	DISALLOW_COPY_AND_ASSIGN(file_stream)
};

#ifdef TARGET_PLATFORM_ANDROID

/// asset input/output stream
class android_asset_stream : public input_stream {
public:
	android_asset_stream(const char* filename)
		: _asset(NULL)
		, _eof(false)
	{
		android_app* app = reinterpret_cast<android_app*>(application::get_instance()->get_context());
		_asset = AAssetManager_open(app->activity->assetManager, filename, AASSET_MODE_STREAMING);
	}

	~android_asset_stream() {
		AAsset_close(_asset);
	}

	void seek(int pos, origin o = origin::beginning) {
		int Origin =
			o == origin::beginning ? SEEK_SET :
			o == origin::current ? SEEK_CUR : SEEK_END;

		if (AAsset_seek(_asset, pos, Origin) == (size_t)-1) {
			BOOST_THROW_EXCEPTION(std::runtime_error("failed to seek asset"));
		}
		_eof = false;
	}


	size_t tell() const {
		return AAsset_getLength(_asset) - AAsset_getRemainingLength(_asset);
	}

	size_t read(void* buffer, size_t length) {
		int size = AAsset_read(_asset, buffer, length);
		if (size < 0) {
			BOOST_THROW_EXCEPTION(std::runtime_error("failed to read from asset"));
		}
		if (!size) {
			_eof = true;
		}
		return (size_t)size;
	}

	bool eof() const {
		return _eof;
	}

private:
	AAsset* _asset;
	bool _eof;
};

#endif // TARGET_PLATFORM_ANDROID

// stream static constructors
//

stream* stream::from_memory() {
	return new dynamic_memory_stream();
}

stream* stream::from_memory(void* begin, void* end, file_access access) {
	return new memory_range_stream(begin, end, access);
}

stream* stream::from_file(const char* filename, file_mode open_mode, file_access access) {
	return new file_stream(filename, open_mode, access);
}

input_stream* stream::from_asset(const char* filename) {
#ifdef TARGET_PLATFORM_ANDROID
	return new android_asset_stream(filename);
#else
	return from_file((std::string("assets/") + filename).c_str());
#endif
}

// stream static methods
//

void stream::read_all(input_stream* istream, std::vector<uint8_t>& data) {
	BOOST_ASSERT(istream);

	uint8_t buffer[BUFSIZ];
	while (!istream->eof()) {
		size_t read = istream->read(buffer, sizeof(buffer));
		data.insert(data.end(), buffer, buffer + read);
	}
}

void stream::write_all(output_stream* ostream, const std::vector<uint8_t>& data) {
	BOOST_ASSERT(ostream);

	if (!data.empty()) {
		ostream->write(&data.front(), data.size());
		ostream->flush();
	}
}

void stream::copy(input_stream* istream, output_stream* ostream) {
	BOOST_ASSERT(istream);
	BOOST_ASSERT(ostream);

	uint8_t buffer[BUFSIZ];
	while (!istream->eof()) {
		size_t read = istream->read(buffer, sizeof(buffer));
		if (ostream->write(buffer, read) != read) {
			BOOST_THROW_EXCEPTION(std::runtime_error("failed to copy stream"));
		}
	}

	ostream->flush();
}

// binary_reader
//

binary_reader::binary_reader(input_stream* stream)
	: _stream(stream)
{
	BOOST_ASSERT(stream);
}

uint8_t binary_reader::read_uint8() {
	uint8_t result = 0;
	if (_stream->read(&result, sizeof(uint8_t)) != sizeof(uint8_t)) {
		BOOST_THROW_EXCEPTION(std::runtime_error("failed to read stream"));
	}
	return result;
}

uint16_t binary_reader::read_uint16() {
	uint16_t b1 = read_uint8();
	uint16_t b2 = read_uint8();
	return b1 | b2 << 8;
}

uint32_t binary_reader::read_uint32() {
	uint32_t w1 = read_uint16();
	uint32_t w2 = read_uint16();
	return w1 | w2 << 16;
}

uint64_t binary_reader::read_uint64() {
	uint64_t dw1 = read_uint32();
	uint64_t dw2 = read_uint32();
	return dw1 | dw2 << 32;
}

int8_t binary_reader::read_int8() {
	return static_cast<int8_t>(read_uint8());
}

int16_t binary_reader::read_int16() {
	return static_cast<int16_t>(read_uint16());
}

int32_t binary_reader::read_int32() {
	return static_cast<int32_t>(read_uint32());
}

int64_t binary_reader::read_int64() {
	return static_cast<int64_t>(read_uint64());
}

float binary_reader::read_float() {
	uint32_t result = read_uint32();
	return *reinterpret_cast<float*>(&result);
}

double binary_reader::read_double() {
	uint64_t result = read_uint64();
	return *reinterpret_cast<double*>(&result);
}

bool binary_reader::read_boolean() {
	return !!read_uint8();
}

uint32_t binary_reader::read_7bit_encoded_uint32() {
	uint32_t result = 0;
	uint32_t bits_read = 0;
	uint32_t value = 0;

	do {
		value = read_uint8();
		result |= (value & 0x7f) << bits_read;
		bits_read += 7;
	} while (value & 0x80);

	return result;
}

uint32_t binary_reader::read_unicode_char() {
	uint32_t result = read_uint8();

	if (result & 0x80) {
		int byte_count = 1;
		while (result & (0x80 >> byte_count)) {
			byte_count++;
		}

		result &= (1 << (8 - byte_count)) - 1;
		
		while (--byte_count) {
			result <<= 6;
			result |= read_uint8() & 0x3F;
		}
	}

	return result;
}

std::string binary_reader::read_zero_string() {
	std::ostringstream ss;
	
	int8_t result = read_int8();	
	while (result) {
		ss << result;
		result = read_int8();
	}
	
	return ss.str();
}

std::string binary_reader::read_pascal_string() {
	std::ostringstream ss;
	
	int32_t length = read_int32();	
	for (int32_t i = 0; i < length; ++i) {
		ss << read_int8();
	}
	
	return ss.str();
}

// binary_writer
//

binary_writer::binary_writer(output_stream* stream)
	: _stream(stream)
{
	BOOST_ASSERT(stream);
}

void binary_writer::write_uint8(uint8_t i) {
	if (_stream->write(&i, sizeof(uint8_t)) != sizeof(uint8_t)) {
		BOOST_THROW_EXCEPTION(std::runtime_error("failed to write stream"));
	}
}

void binary_writer::write_uint16(uint16_t i) {
	write_uint8(static_cast<uint8_t>(i & 0xff));
	write_uint8(static_cast<uint8_t>((i >> 8) & 0xff));
}

void binary_writer::write_uint32(uint32_t i) {
	write_uint16(static_cast<uint16_t>(i & 0xffff));
	write_uint16(static_cast<uint16_t>((i >> 16) & 0xffff));
}

void binary_writer::write_uint64(uint64_t i) {
	write_uint32(static_cast<uint32_t>(i & 0xffffffff));
	write_uint32(static_cast<uint32_t>((i >> 32) & 0xffffffff));
}

void binary_writer::write_int8(int8_t i) {
	write_uint8(static_cast<uint8_t>(i));
}

void binary_writer::write_int16(int16_t i) {
	write_uint16(static_cast<uint16_t>(i));
}

void binary_writer::write_int32(int32_t i) {
	write_uint32(static_cast<uint32_t>(i));
}

void binary_writer::write_int64(int64_t i) {
	write_uint64(static_cast<uint64_t>(i));
}

void binary_writer::write_float(float f) {
	write_uint32(*reinterpret_cast<uint32_t*>(&f));
}

void binary_writer::write_double(double f) {
	write_uint64(*reinterpret_cast<uint64_t*>(&f));
}

void binary_writer::write_boolean(bool b) {
	write_uint8(b ? 1 : 0);
}

void binary_writer::write_7bit_encoded_uint32(uint32_t i) {
	while (i >= 0x80) {
		write_uint8(static_cast<uint8_t>(i | 0x80));
		i >>= 7;
	}
	
	write_uint8(static_cast<uint8_t>(i));
}

void binary_writer::write_unicode_char(uint32_t c) {
	if (c <= 0x7F) {
		write_uint8(static_cast<uint8_t>(c));
	} else if (c <= 0x7FF) {
		write_uint8(static_cast<uint8_t>(0xC0 | (c >> 6)));
		write_uint8(static_cast<uint8_t>(0x80 | (c & 0x3F)));
	} else {
		write_uint8(static_cast<uint8_t>(0xE0 | (c >> 12)));
		write_uint8(static_cast<uint8_t>(0x80 | ((c >> 6) & 0x3F)));
		write_uint8(static_cast<uint8_t>(0x80 | (c & 0x3F)));
	}
}

void binary_writer::write_zero_string(const std::string& str) {
	for (std::string::const_iterator it = str.begin(); it != str.end(); ++it) {
		write_uint8(*it);
	}
	
	write_uint8(0);
}

void binary_writer::write_pascal_string(const std::string& str) {
	write_uint32(str.size());

	for (std::string::const_iterator it = str.begin(); it != str.end(); ++it) {
		write_uint8(*it);
	}
}


} // namespace io
