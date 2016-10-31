#pragma once

// Classes in this file:
//     image

#include "cool/common.hpp"
#include "cool/io.hpp"
#include "cool/enum_traits.hpp"

namespace imaging {

DEFINE_ENUM_CLASS(
	pixel_type, uint32_t,
	unknown,
	luminance,
	alpha,
	luminance_alpha,
	rgb8,
	rgba8,
	rgb565,
	rgba5551,
	rgba4444
)

// image
class image : public ref_counter<image> {
public:
	image();
	~image();

	void swap(image& other);

	void clear();

	/// creates raw image data
	void create(size_t width, size_t height, pixel_type fmt);

	// clones image
	image* clone() const;

	/// loads image dimentions and pixel type
	int load_info(io::input_stream* stream);

	/// sets image dimentions and pixel type
	void set_info(size_t width, size_t height, pixel_type format = pixel_type::rgba8);

	/// loads image from stream
	void load(io::input_stream* stream, pixel_type format = pixel_type::unknown);
	void load(io::input_stream* streamC, io::input_stream* streamA, pixel_type format = pixel_type::unknown);

	/// saves image to file
	void save(const char* filename);

	/// creates new image from given region
	image* get_subimage(size_t xoffset, size_t yoffset, size_t width, size_t height);

	bool can_convert(pixel_type to) const { return can_convert(_pixel_type, to); }
	image* convert(pixel_type to) const;
	void convert_inplace(pixel_type to);

	size_t get_width() const { return _width; }
	size_t get_height() const { return _height; }
	uint8_t* get_pixels() const { return _pixels; }

	pixel_type get_pixel_type() const { return _pixel_type; }
	void set_pixel_type(pixel_type fmt) { _pixel_type = fmt; }

	bool has_alpha() const { return has_alpha_channel(_pixel_type); }

	static image* from_stream(io::input_stream* stream, pixel_type format = pixel_type::unknown);
	static image* from_stream_separate(io::input_stream* streamC, io::input_stream* streamA, pixel_type format = pixel_type::unknown);

	static int get_components(pixel_type fmt);
	static int get_stride(pixel_type fmt);
	static bool has_alpha_channel(pixel_type fmt);
	static bool is_format_natural(pixel_type fmt);
	static bool is_format_16bit_compound(pixel_type fmt);
	static bool is_format_compressed(pixel_type fmt);
	static bool can_convert(pixel_type from, pixel_type to);
	static pixel_type get_suitable_format(int components);
	static pixel_type get_suitable_format_with_alpha(pixel_type fmt);
	static pixel_type get_suitable_16bit_format(pixel_type fmt);

private:
	size_t _width;
	size_t _height;
	pixel_type _pixel_type;
	uint8_t* _pixels;

private:
	DISALLOW_COPY_AND_ASSIGN(image)
};

typedef boost::intrusive_ptr<image> image_ptr;

} // namespace imaging

namespace boost
{
	template<>
	inline void swap(imaging::image& image1, imaging::image& image2) {
		image1.swap(image2);
	}
}
