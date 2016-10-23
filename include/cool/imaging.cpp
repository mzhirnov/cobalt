#include "pch.h"
#include "cool/common.hpp"
#include "cool/imaging.hpp"
#include "cool/logger.hpp"

#include <memory>

#include <boost/algorithm/string.hpp>
#include <boost/exception/all.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace imaging {

namespace {
	struct floyd_steinberg_error_info {
		int dx, dy;
		uint32_t factor;
	} const err_info[] = {
		              /* pixel */  { 1, 0, 7 },
		{ -1, 1, 3 }, { 0, 1, 5 }, { 1, 1, 1 }
	};
}

// image
//

static int stbi_stream_read(void* user, char* data, int size) {
	io::input_stream* s = reinterpret_cast<io::input_stream*>(user);
	try {
		return s->read(data, size);
	} catch (const std::exception&) {
		BOOST_ASSERT_MSG(false, "failed to read stream");
		return 0;
	}
}

static void stbi_stream_skip(void* user, int n) {
	io::input_stream* s = reinterpret_cast<io::input_stream*>(user);
	try {
		s->seek(n, io::origin::current);
	} catch (const std::exception&) {
		BOOST_ASSERT_MSG(false, "failed to seek stream");
	}
}

static int stbi_stream_eof(void* user) {
	io::input_stream* s = reinterpret_cast<io::input_stream*>(user);
	try {
		return s->eof() ? 1 : 0;
	} catch (const std::exception&) {
		BOOST_ASSERT_MSG(false, "failed to check eof");
		return 1;
	}
}

image::image()
	: _width(0)
	, _height(0)
	, _pixel_type(pixel_type::unknown)
	, _pixels(nullptr)
{
}

image::~image() {
	clear();
}

void image::swap(image& other) {
	boost::swap(_width, other._width);
	boost::swap(_height, other._height);
	boost::swap(_pixel_type, other._pixel_type);
	boost::swap(_pixels, other._pixels);
}

void image::clear() {
	if (_pixels) {
		::free(_pixels);
		_pixels = nullptr;
	}
	_width = 0;
	_height = 0;
	_pixel_type = pixel_type::unknown;
}

void image::create(size_t width, size_t height, pixel_type format) {
	BOOST_ASSERT(is_format_natural(format) || is_format_16bit_compound(format));

	clear();

	_width = width;
	_height = height;
	_pixel_type = format;
	_pixels = (uint8_t*)malloc(_width * _height * get_stride(format));

	if (!_pixels) {
		BOOST_THROW_EXCEPTION(std::runtime_error("failed to create image"));
	}
}

image* image::clone() const {
	BOOST_ASSERT(!!_width && !!_height && !!_pixels);
	std::unique_ptr<image> img(new image());
	img->create(_width, _height, _pixel_type);
	memcpy(img->get_pixels(), _pixels, _width * _height * get_stride(_pixel_type));
	return img.release();
}

int image::load_info(io::input_stream* stream) {
	io::input_stream_ptr guard(stream);

	clear();

	stbi_io_callbacks cb;
	cb.read = &stbi_stream_read;
	cb.skip = &stbi_stream_skip;
	cb.eof = &stbi_stream_eof;

	stream->seek(0);

	int components = 0;
	if (!stbi_info_from_callbacks(&cb, stream, (int*)&_width, (int*)&_height, &components)) {
		BOOST_THROW_EXCEPTION(std::runtime_error("failed to get image info"));
	}

	_pixel_type = get_suitable_format(components);
	
	return components;
}

void image::set_info(size_t width, size_t height, pixel_type format) {
	clear();
	
	_width = width;
	_height = height;
	_pixel_type = format;
}

void image::load(io::input_stream* stream, pixel_type format) {
	BOOST_ASSERT(stream != NULL);

	io::input_stream_ptr guard(stream);

	clear();

	stbi_io_callbacks cb;
	cb.read = &stbi_stream_read;
	cb.skip = &stbi_stream_skip;
	cb.eof = &stbi_stream_eof;

	int components = 0;
	if (format == pixel_type::unknown) {
		components = load_info(stream);
	} else {
		components = get_components(format);
	}

	if (format == pixel_type::unknown || has_alpha_channel(format)) {
		components += components % 2;
	}

	_pixel_type = get_suitable_format(components);

	stream->seek(0);
	_pixels = stbi_load_from_callbacks(&cb, stream, (int*)&_width, (int*)&_height, &components, components);
	if (!_pixels) {
		BOOST_THROW_EXCEPTION(std::runtime_error("failed to load image"));
	}

	if (format != pixel_type::unknown) {
		convert_inplace(format);
	}
}

void image::load(io::input_stream* streamC, io::input_stream* streamA, pixel_type format) {
	BOOST_ASSERT(!!streamC);
	BOOST_ASSERT(!!streamA);

	io::input_stream_ptr guard1(streamC), guard2(streamA);

	stbi_io_callbacks cb;
	cb.read = &stbi_stream_read;
	cb.skip = &stbi_stream_skip;
	cb.eof = &stbi_stream_eof;

	int components = 0;
	if (format == pixel_type::unknown) {
		components = load_info(streamC);
	} else {
		components = get_components(format);
	}

	if (format == pixel_type::unknown || has_alpha_channel(format)) {
		components += components % 2;
	}

	_pixel_type = get_suitable_format(components);

	streamC->seek(0);
	_pixels = stbi_load_from_callbacks(&cb, streamC, (int*)&_width, (int*)&_height, &components, components);
	if (!_pixels) {
		BOOST_THROW_EXCEPTION(std::runtime_error("failed to load image"));
	}

	if (format == pixel_type::unknown || has_alpha_channel(format)) {
		streamA->seek(0);

		int w, h, c;
		stbi_uc* alpha = stbi_load_from_callbacks(&cb, streamA, &w, &h, &c, 1);
		if (!alpha) {
			BOOST_THROW_EXCEPTION(std::runtime_error("failed to load image"));
		}
	
		BOOST_ASSERT(w == (int)_width);
		BOOST_ASSERT(h == (int)_height);

		for (int y = 0; y < h; ++y) {
			for (int x = 0; x < w; ++x) {
				_pixels[(y * w + x + 1) * get_stride(_pixel_type) - 1] = alpha[y * w + x];
			}
		}

		stbi_image_free(alpha);
	}

	if (format != pixel_type::unknown) {
		convert_inplace(format);
	}
}

void image::save(const char* filename) {
	BOOST_ASSERT_MSG(boost::iends_with(filename, ".png"), "only .png supported");

	if (is_format_16bit_compound(_pixel_type)) {
		image_ptr img = convert(has_alpha() ? pixel_type::rgba8 : pixel_type::rgb8);
		stbi_write_png(filename, _width, _height, get_components(_pixel_type), img->get_pixels(), _width * get_stride(img->get_pixel_type()));
	} else {
		stbi_write_png(filename, _width, _height, get_components(_pixel_type), _pixels, _width * get_stride(_pixel_type));
	}
}

image* image::get_subimage(size_t xoffset, size_t yoffset, size_t width, size_t height) {
	if (!_pixels) {
		BOOST_THROW_EXCEPTION(std::runtime_error("image is not loaded"));
	}

	if (_width < xoffset + width || _height < yoffset + height) {
		BOOST_THROW_EXCEPTION(std::runtime_error("image dimentions don't match"));
	}

	std::unique_ptr<image> subimage(new image());
	subimage->create(width, height, _pixel_type);

	BOOST_ASSERT_MSG(_pixel_type == subimage->get_pixel_type(), "images must have the same pixel type");

	uint8_t* pixels = subimage->get_pixels();
	const int stride = get_stride(_pixel_type);

	for (size_t y = 0; y < height; ++y) {
		memcpy(pixels + y * width * stride, _pixels + ((y + yoffset) * _width + xoffset) * stride, width * stride);
	}

	return subimage.release();
}

static image* reinterpret_format(const image* img, pixel_type fmt) {
	std::unique_ptr<image> img2(img->clone());
	img2->set_pixel_type(fmt);
	return img2.release();
}

static image* convert_from_rgb8_to_rgb565(const image* img, pixel_type fmt) {
	std::unique_ptr<image> img2(new image());
	img2->create(img->get_width(), img->get_height(), fmt);

	uint8_t* p = img->get_pixels();
	uint8_t* p2 = img2->get_pixels();

	const int w = img->get_width();
	const int h = img->get_height();

	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x) {
			uint8_t b = p[(y * w + x) * 3 + 0];
			uint8_t g = p[(y * w + x) * 3 + 1];
			uint8_t r = p[(y * w + x) * 3 + 2];
			
			*(uint16_t*)&p2[(y * w + x) * 2] = r >> 3 | (g >> 2) << 5 | (b >> 3) << 11;
			
			// push quantization error of a pixel onto its neighboring pixels
			b &= 0x07; g &= 0x03; r &= 0x07;
			for (auto it = std::begin(err_info), end = std::end(err_info); it != end; ++it) {
				if (x + it->dx < 0 || x + it->dx >= w || y + it->dy < 0 || y + it->dy >= h) {
					continue;
				}
				const uint32_t addr = ((y + it->dy) * w + x + it->dx) * 3;
				p[addr + 0] = (uint8_t)std::min(255u, p[addr + 0] + ((b * it->factor) >> 4));
				p[addr + 1] = (uint8_t)std::min(255u, p[addr + 1] + ((g * it->factor) >> 4));
				p[addr + 2] = (uint8_t)std::min(255u, p[addr + 2] + ((r * it->factor) >> 4));
			}
		}
	}

	return img2.release();
}

static image* convert_from_rgb8_to_rgba8(const image* img, pixel_type fmt) {
	std::unique_ptr<image> img2(new image());
	img2->create(img->get_width(), img->get_height(), fmt);

	uint8_t* p = img->get_pixels();
	uint8_t* p2 = img2->get_pixels();

	const int w = img->get_width();
	const int h = img->get_height();

	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x) {
			p2[(y * w + x) * 4 + 0] = p[(y * w + x) * 3 + 0];
			p2[(y * w + x) * 4 + 1] = p[(y * w + x) * 3 + 1];
			p2[(y * w + x) * 4 + 2] = p[(y * w + x) * 3 + 2];
			p2[(y * w + x) * 4 + 3] = 255;
		}
	}

	return img2.release();
}

static image* convert_from_rgba8_to_rgb8(const image* img, pixel_type fmt) {
	std::unique_ptr<image> img2(new image());
	img2->create(img->get_width(), img->get_height(), fmt);

	uint8_t* p = img->get_pixels();
	uint8_t* p2 = img2->get_pixels();

	const int w = img->get_width();
	const int h = img->get_height();

	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x) {
			p2[(y * w + x) * 3 + 0] = p[(y * w + x) * 4 + 0];
			p2[(y * w + x) * 3 + 1] = p[(y * w + x) * 4 + 1];
			p2[(y * w + x) * 3 + 2] = p[(y * w + x) * 4 + 2];
		}
	}

	return img2.release();
}

static image* convert_from_rgba8_to_alpha(const image* img, pixel_type fmt) {
	std::unique_ptr<image> img2(new image());
	img2->create(img->get_width(), img->get_height(), fmt);

	uint8_t* p = img->get_pixels();
	uint8_t* p2 = img2->get_pixels();

	const int w = img->get_width();
	const int h = img->get_height();

	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x) {
			p2[y * w + x] = p[(y * w + x) * 4 + 3];
		}
	}

	return img2.release();
}

static image* convert_from_rgba8_to_rgba5551(const image* img, pixel_type fmt) {
	std::unique_ptr<image> img2(new image());
	img2->create(img->get_width(), img->get_height(), fmt);

	uint8_t* p = img->get_pixels();
	uint8_t* p2 = img2->get_pixels();

	const int w = img->get_width();
	const int h = img->get_height();

	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x) {
			uint8_t b = p[(y * w + x) * 4 + 0];
			uint8_t g = p[(y * w + x) * 4 + 1];
			uint8_t r = p[(y * w + x) * 4 + 2];
			uint8_t a = p[(y * w + x) * 4 + 3];
			
			*(uint16_t*)&p2[(y * w + x) * 2] = a >> 7 | (r >> 3) << 1 | (g >> 3) << 6 | (b >> 3) << 11;
			
			// push quantization error of a pixel onto its neighboring pixels
			a &= 0x07; r &= 0x07; g &= 0x07; b &= 0x07;
			for (auto it = std::begin(err_info), end = std::end(err_info); it != end; ++it) {
				if (x + it->dx < 0 || x + it->dx >= w || y + it->dy < 0 || y + it->dy >= h) {
					continue;
				}
				const uint32_t addr = ((y + it->dy) * w + x + it->dx) * 4;
				p[addr + 0] = (uint8_t)std::min(255u, p[addr + 0] + ((b * it->factor) >> 4));
				p[addr + 1] = (uint8_t)std::min(255u, p[addr + 1] + ((g * it->factor) >> 4));
				p[addr + 2] = (uint8_t)std::min(255u, p[addr + 2] + ((r * it->factor) >> 4));
				p[addr + 3] = (uint8_t)std::min(255u, p[addr + 3] + ((a * it->factor) >> 4));
			}
		}
	}

	return img2.release();
}

static image* convert_from_rgba8_to_rgba4444(const image* img, pixel_type fmt) {
	std::unique_ptr<image> img2(new image());
	img2->create(img->get_width(), img->get_height(), fmt);

	uint8_t* p = img->get_pixels();
	uint8_t* p2 = img2->get_pixels();

	const int w = img->get_width();
	const int h = img->get_height();

	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x) {
			uint8_t b = p[(y * w + x) * 4 + 0];
			uint8_t g = p[(y * w + x) * 4 + 1];
			uint8_t r = p[(y * w + x) * 4 + 2];
			uint8_t a = p[(y * w + x) * 4 + 3];
			
			*(uint16_t*)&p2[(y * w + x) * 2] = a >> 4 | (r >> 4) << 4 | (g >> 4) << 8 | (b >> 4) << 12;
			
			// push quantization error of a pixel onto its neighboring pixels
			b &= 0x0f; g &= 0x0f; r &= 0x0f; a &= 0x0f;
			for (auto it = std::begin(err_info), end = std::end(err_info); it != end; ++it) {
				if (x + it->dx < 0 || x + it->dx >= w || y + it->dy < 0 || y + it->dy >= h) {
					continue;
				}
				const uint32_t addr = ((y + it->dy) * w + x + it->dx) * 4;
				p[addr + 0] = (uint8_t)std::min(255u, p[addr + 0] + ((b * it->factor) >> 4));
				p[addr + 1] = (uint8_t)std::min(255u, p[addr + 1] + ((g * it->factor) >> 4));
				p[addr + 2] = (uint8_t)std::min(255u, p[addr + 2] + ((r * it->factor) >> 4));
				p[addr + 3] = (uint8_t)std::min(255u, p[addr + 3] + ((a * it->factor) >> 4));
			}
		}
	}

	return img2.release();
}

static image* convert_from_rgba5551_to_rgba8(const image* img, pixel_type fmt) {
	std::unique_ptr<image> img2(new image());
	img2->create(img->get_width(), img->get_height(), fmt);

	uint8_t* p = img->get_pixels();
	uint8_t* p2 = img2->get_pixels();

	const int w = img->get_width();
	const int h = img->get_height();

	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x) {
			const uint16_t c = *(uint16_t*)&p[(y * w + x) * 2];
			p2[(y * w + x) * 4 + 0] = (c << 0) >> 11 << 3;
			p2[(y * w + x) * 4 + 1] = (c << 5) >> 11 << 3;
			p2[(y * w + x) * 4 + 2] = (c << 10) >> 11 << 3;
			p2[(y * w + x) * 4 + 3] = (c & 1) ? 255 : 0;
		}
	}

	return img2.release();
}

static image* convert_from_rgba4444_to_rgba8(const image* img, pixel_type fmt) {
	std::unique_ptr<image> img2(new image());
	img2->create(img->get_width(), img->get_height(), fmt);

	uint8_t* p = img->get_pixels();
	uint8_t* p2 = img2->get_pixels();

	const int w = img->get_width();
	const int h = img->get_height();

	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x) {
			const uint16_t c = *(uint16_t*)&p[(y * w + x) * 2];
			p2[(y * w + x) * 4 + 0] = (c << 0) >> 12 << 4;
			p2[(y * w + x) * 4 + 1] = (c << 4) >> 12 << 4;
			p2[(y * w + x) * 4 + 2] = (c << 8) >> 12 << 4;
			p2[(y * w + x) * 4 + 3] = (c << 12) >> 8;
		}
	}

	return img2.release();
}

static image* convert_from_rgb565_to_rgb8(const image* img, pixel_type fmt) {
	std::unique_ptr<image> img2(new image());
	img2->create(img->get_width(), img->get_height(), fmt);

	uint8_t* p = img->get_pixels();
	uint8_t* p2 = img2->get_pixels();

	const int w = img->get_width();
	const int h = img->get_height();

	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x) {
			const uint16_t c = *(uint16_t*)&p[(y * w + x) * 2];
			p2[(y * w + x) * 3 + 0] = (c << 0) >> 11 << 3;
			p2[(y * w + x) * 3 + 1] = (c << 5) >> 10 << 2;
			p2[(y * w + x) * 3 + 2] = (c << 11) >> 8;
		}
	}

	return img2.release();
}

static image* convert_from_rgb565_to_rgba8(const image* img, pixel_type fmt) {
	std::unique_ptr<image> img2(new image());
	img2->create(img->get_width(), img->get_height(), fmt);

	uint8_t* p = img->get_pixels();
	uint8_t* p2 = img2->get_pixels();

	const int w = img->get_width();
	const int h = img->get_height();

	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x) {
			const uint16_t c = *(uint16_t*)&p[(y * w + x) * 2];
			p2[(y * w + x) * 4 + 0] = (c << 0) >> 11 << 3;
			p2[(y * w + x) * 4 + 1] = (c << 5) >> 10 << 2;
			p2[(y * w + x) * 4 + 2] = (c << 11) >> 8;
			p2[(y * w + x) * 4 + 3] = 255;
		}
	}

	return img2.release();
}

struct convert_table {
	pixel_type from;
	pixel_type to;
	image* (*convert)(const image*, pixel_type);
} _convert_table[] = {
	{ pixel_type::luminance, pixel_type::alpha, reinterpret_format },
	{ pixel_type::alpha, pixel_type::luminance, reinterpret_format },
	{ pixel_type::rgb8, pixel_type::rgb565, convert_from_rgb8_to_rgb565 },
	{ pixel_type::rgb8, pixel_type::rgba8, convert_from_rgb8_to_rgba8 },
	{ pixel_type::rgba8, pixel_type::rgb8, convert_from_rgba8_to_rgb8 },
	{ pixel_type::rgba8, pixel_type::alpha, convert_from_rgba8_to_alpha },
	{ pixel_type::rgba8, pixel_type::rgba5551, convert_from_rgba8_to_rgba5551 },
	{ pixel_type::rgba8, pixel_type::rgba4444, convert_from_rgba8_to_rgba4444 },
	{ pixel_type::rgba5551, pixel_type::rgba8, convert_from_rgba5551_to_rgba8 },
	{ pixel_type::rgba4444, pixel_type::rgba8, convert_from_rgba4444_to_rgba8 },
	{ pixel_type::rgb565, pixel_type::rgb8, convert_from_rgb565_to_rgb8 },
	{ pixel_type::rgb565, pixel_type::rgba8, convert_from_rgb565_to_rgba8 },
};

image* image::convert(pixel_type to) const {
	if (_pixel_type == to) {
		return clone();
	}

	for (int i = 0; i < sizeof(_convert_table) / sizeof(_convert_table[0]); ++i) {
		if (_convert_table[i].from == _pixel_type && _convert_table[i].to == to) {
			return _convert_table[i].convert(this, to);
		}
	}

	BOOST_THROW_EXCEPTION(std::runtime_error("conversion not supported"));
}

void image::convert_inplace(pixel_type to) {
	if (_pixel_type == to) {
		return;
	}

	if ((_pixel_type == pixel_type::luminance || _pixel_type == pixel_type::alpha) && (to == pixel_type::luminance || to == pixel_type::alpha)) {
		set_pixel_type(to);
		return;
	}

	for (int i = 0; i < sizeof(_convert_table) / sizeof(_convert_table[0]); ++i) {
		if (_convert_table[i].from == _pixel_type && _convert_table[i].to == to) {
			image_ptr img2 = _convert_table[i].convert(this, to);
			swap(*img2.get());
			return;
		}
	}

	BOOST_THROW_EXCEPTION(std::runtime_error("conversion not supported"));
}

bool image::has_alpha_channel(pixel_type fmt) {
	switch (fmt) {
	case pixel_type::luminance:
	case pixel_type::rgb8:
	case pixel_type::rgb565:
		return false;
	case pixel_type::alpha:
	case pixel_type::luminance_alpha:
	case pixel_type::rgba8:
	case pixel_type::rgba5551:
	case pixel_type::rgba4444:
		return true;
	default:
		BOOST_ASSERT_MSG(false, "invalid format");
		return false;
	}
}

bool image::is_format_natural(pixel_type fmt) {
	switch (fmt) {
	case pixel_type::luminance:
	case pixel_type::alpha:
	case pixel_type::luminance_alpha:
	case pixel_type::rgb8:
	case pixel_type::rgba8:
		return true;
	default:
		return false;
	}
}

bool image::is_format_16bit_compound(pixel_type fmt) {
	switch (fmt) {
	case pixel_type::rgb565:
	case pixel_type::rgba5551:
	case pixel_type::rgba4444:
		return true;
	default:
		return false;
	}
}

bool image::is_format_compressed(pixel_type /*fmt*/) {
	return false;
}

bool image::can_convert(pixel_type from, pixel_type to) {
	if (from == to) {
		return true;
	}

	for (int i = 0; i < sizeof(_convert_table) / sizeof(_convert_table[0]); ++i) {
		if (_convert_table[i].from == from && _convert_table[i].to == to) {
			return true;
		}
	}

	return false;
}

int image::get_components(pixel_type fmt) {
	switch (fmt) {
	case pixel_type::luminance:
	case pixel_type::alpha:
		return 1;
	case pixel_type::luminance_alpha:
		return 2;
	case pixel_type::rgb8:
	case pixel_type::rgb565:
		return 3;
	case pixel_type::rgba8:
	case pixel_type::rgba5551:
	case pixel_type::rgba4444:
		return 4;
	default:
		BOOST_ASSERT_MSG(false, "invalid format");
		return 0;
	}
}

int image::get_stride(pixel_type fmt) {
	switch (fmt) {
	case pixel_type::luminance:
	case pixel_type::alpha:
		return 1;
	case pixel_type::luminance_alpha:
	case pixel_type::rgb565:
	case pixel_type::rgba5551:
	case pixel_type::rgba4444:
		return 2;
	case pixel_type::rgb8:
		return 3;
	case pixel_type::rgba8:
		return 4;
	default:
		BOOST_ASSERT_MSG(false, "invalid format");
		return 0;
	}
}

pixel_type image::get_suitable_format(int components) {
	switch (components) {
	case 1:
		return pixel_type::luminance;
	case 2:
		return pixel_type::luminance_alpha;
	case 3:
		return pixel_type::rgb8;
	case 4:
		return pixel_type::rgba8;
	default:
		BOOST_ASSERT_MSG(false, "invalid format");
		return pixel_type::luminance; // 8 bit just in case
	}
}

pixel_type image::get_suitable_format_with_alpha(pixel_type fmt) {
	switch (fmt) {
	case pixel_type::alpha:
	case pixel_type::luminance:
	case pixel_type::luminance_alpha:
		return pixel_type::luminance_alpha;
	case pixel_type::rgb8:
	case pixel_type::rgba8:
		return pixel_type::rgba8;
	case pixel_type::rgb565:
	case pixel_type::rgba4444:
		return pixel_type::rgba4444;
	case pixel_type::rgba5551:
		return pixel_type::rgba5551;
	default:
		BOOST_ASSERT_MSG(false, "invalid format");
		return pixel_type::rgba8;
	}
}

pixel_type image::get_suitable_16bit_format(pixel_type fmt) {
	switch (fmt) {
	case pixel_type::rgb8:
		return pixel_type::rgb565;
	case pixel_type::rgba8:
		return pixel_type::rgba4444;
	default:
		return fmt;
	}
}

image* image::from_stream(io::input_stream* stream, pixel_type fmt) {
	io::input_stream_ptr guard(stream);
	std::unique_ptr<image> img(new image());
	img->load(stream, fmt);	
	return img.release();
}

image* image::from_stream_separate(io::input_stream* streamC, io::input_stream* streamA, pixel_type fmt) {
	io::input_stream_ptr guard1(streamC), guard2(streamA);	
	std::unique_ptr<image> img(new image());
	img->load(streamC, streamA, fmt);	
	return img.release();
}

} // namespace imaging
