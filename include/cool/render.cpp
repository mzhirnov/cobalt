#include "pch.h"
#include "cool/render.hpp"
#include "cool/logger.hpp"

#include <boost/exception/all.hpp>
#include <boost/algorithm/string.hpp>

#include <EGL/egl.h>

#if defined(_DEBUG) && !defined(NDEBUG)

static std::string get_gl_error_string(GLenum err) {
	std::string error;
	switch (err) {
	case GL_NO_ERROR: error = "GL_NO_ERROR"; break;
	case GL_INVALID_ENUM: error = "GL_INVALID_ENUM"; break;
	case GL_INVALID_VALUE: error = "GL_INVALID_VALUE"; break;
	case GL_INVALID_OPERATION: error = "GL_INVALID_OPERATION"; break;
	case GL_INVALID_FRAMEBUFFER_OPERATION: error = "GL_INVALID_FRAMEBUFFER_OPERATION"; break;
	case GL_OUT_OF_MEMORY: error = "GL_OUT_OF_MEMORY"; break;
	}
	return error;
}

static void gl_check_error(const char* message, const char* file, int line) {
	bool error = false;
	for (GLenum err = glGetError(); err != GL_NO_ERROR; err = glGetError()) {
		error = true;
		LOGE("GL error occured: ") << get_gl_error_string(err);
	}
	if (error) {
		LOGE("\tat: ") << file << "(" << line << ")";
		LOGE("\tmsg: ") << message;
	}
	BOOST_ASSERT_MSG(!error, message);
}

#define GL_CLEAR_ERROR() \
	for (GLenum err = glGetError(); err != GL_NO_ERROR; err = glGetError()) { \
		LOGW("GL error while clear: ") << get_gl_error_string(err); \
	}

#define GL_CHECK_ERROR(message) gl_check_error(message, __FILE__, __LINE__)

#define GL_VERIFY(expr) \
	(expr); \
	GL_CHECK_ERROR(#expr)

#else // !_DEBUG || NDEBUG

#define GL_CLEAR_ERROR()
#define GL_CHECK_ERROR(message)
#define GL_VERIFY(expr) (expr)

#endif // _DEBUG && !NDEBUG

namespace render {

// color
//

color color::from_color_value(const color_value& c) {
	return color(
		static_cast<uint8_t>(std::ceil(c.blue() * 255.0f)),
		static_cast<uint8_t>(std::ceil(c.green() * 255.0f)),
		static_cast<uint8_t>(std::ceil(c.red() * 255.0f)),
		static_cast<uint8_t>(std::ceil(c.alpha() * 255.0f))
	);
}

// color_value
//

color_value color_value::from_color(const color& c) {
	const float inv = 1.0f / 255.0f;
	return color_value(
		c.blue() * inv,
		c.green() * inv,
		c.red() * inv,
		c.alpha() * inv
	);
}

// colors
//

color_value colors::black() { return color_value(0.0f, 0.0f, 0.0f, 1.0f); }
color_value colors::white() { return color_value(1.0f, 1.0f, 1.0f, 1.0f); }
color_value colors::red() { return color_value(0.0f, 0.0f, 1.0f, 1.0f); }
color_value colors::green() { return color_value(0.0f, 1.0f, 0.0f, 1.0f); }
color_value colors::blue() { return color_value(1.0f, 0.0f, 0.0f, 1.0f); }
color_value colors::cornflower_blue() { return color_value(0.929f, 0.584f, 0.392f, 1.0f); }

// shader
//

shader::shader()
	: _shader(0)
{
}

shader::~shader() {
	destroy();
}

NativeShaderHandleType shader::create(shader_type type, const char* sources[], size_t count) {
	destroy();

	GLenum Type = (type == shader_type::vertex) ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER;

	_shader = glCreateShader(Type);
	GL_CHECK_ERROR("glCreateShader");
	
	if (!_shader) {
		BOOST_THROW_EXCEPTION(std::runtime_error("failed to create shader"));
	}

	GL_VERIFY(glShaderSource(_shader, count, sources, NULL));
	GL_VERIFY(glCompileShader(_shader));

	GLint compiled = 0;
	GL_VERIFY(glGetShaderiv(_shader, GL_COMPILE_STATUS, &compiled));

	if (!compiled) {
		std::ostringstream ss("failed to compile shader");

		GLint length = 0;
		GL_VERIFY(glGetShaderiv(_shader, GL_INFO_LOG_LENGTH, &length));
		if (length > 1) {
			std::vector<char> info_log(length);
			GL_VERIFY(glGetShaderInfoLog(_shader, length, NULL, &info_log.front()));
			ss << ": " << &info_log.front();
		}

		ss << std::endl << sources[count - 1];
		BOOST_THROW_EXCEPTION(std::runtime_error(ss.str()));
	}

	return _shader;
}

void shader::destroy() {
	if (_shader) {
		GL_VERIFY(glDeleteShader(_shader));
		_shader = 0;
	}
}

// shader_program
//

shader_program::shader_program()
	: _program(0)
{
}

shader_program::~shader_program() {
	destroy();
}

void shader_program::add_define(const char* define) {
	_defines.push_back(std::string("#define ") + define + "\n");
}

void shader_program::clear_defines() {
	defines().swap(_defines);
}

NativeShaderProgramHandleType shader_program::create(io::input_stream* vert_src, io::input_stream* frag_src) {
	io::input_stream_ptr guard1(vert_src), guard2(frag_src);

	destroy();

	_program = glCreateProgram();
	GL_CHECK_ERROR("glCreateProgram");

	if (!_program) {
		BOOST_THROW_EXCEPTION(std::runtime_error("failed to create shader program"));
	}

	shader vsh, fsh;

	std::vector<const char*> sources;

	sources.reserve(_defines.size() + 1);
	for (const auto& define: _defines) {
		sources.push_back(define.c_str());
	}

	std::vector<uint8_t> buffer;
	io::stream::read_all(vert_src, buffer);
	buffer.push_back(0);

	sources.push_back((const char*)&buffer.front());

	GL_VERIFY(glAttachShader(_program, vsh.create(shader_type::vertex, &sources.front(), sources.size())));

	buffer.clear();
	io::stream::read_all(frag_src, buffer);
	buffer.push_back(0);

	sources[sources.size() - 1] = (const char*)&buffer.front();

	GL_VERIFY(glAttachShader(_program, fsh.create(shader_type::fragment, &sources.front(), sources.size())));

	GL_VERIFY(glBindAttribLocation(_program, (GLuint)attribute_location::a_position, "a_position"));
	GL_VERIFY(glBindAttribLocation(_program, (GLuint)attribute_location::a_normal, "a_normal"));
	GL_VERIFY(glBindAttribLocation(_program, (GLuint)attribute_location::a_color, "a_color"));
	GL_VERIFY(glBindAttribLocation(_program, (GLuint)attribute_location::a_texcoord, "a_texcoord"));
	GL_VERIFY(glBindAttribLocation(_program, (GLuint)attribute_location::a_texcoord2, "a_texcoord2"));
	GL_VERIFY(glBindAttribLocation(_program, (GLuint)attribute_location::a_texcoord3, "a_texcoord3"));
	GL_VERIFY(glBindAttribLocation(_program, (GLuint)attribute_location::a_texcoord4, "a_texcoord4"));
	GL_VERIFY(glBindAttribLocation(_program, (GLuint)attribute_location::a_blendweights, "a_blendweights"));
	GL_VERIFY(glBindAttribLocation(_program, (GLuint)attribute_location::a_blendindices, "a_blendindices"));

	GL_VERIFY(glLinkProgram(_program));

	GLint linked = 0;
	GL_VERIFY(glGetProgramiv(_program, GL_LINK_STATUS, &linked));
	
	if (!linked) {
		std::ostringstream ss("failed to link shader program");

		GLint length = 0;
		GL_VERIFY(glGetProgramiv(_program, GL_INFO_LOG_LENGTH, &length));
		if (length > 1) {
			std::vector<char> info_log(length);
			GL_VERIFY(glGetProgramInfoLog(_program, length, NULL, &info_log.front()));
			ss << ": " << &info_log.front();
		}
		
		BOOST_THROW_EXCEPTION(std::runtime_error(ss.str()));
	}

	fill_attributes();
	fill_uniforms();

	GL_VERIFY(glUseProgram(_program));

	set_uniform("u_sampler", 0);
	set_uniform("u_sampler2", 1);
	set_uniform("u_sampler3", 2);
	set_uniform("u_sampler4", 3);

	GL_VERIFY(glUseProgram(0));

	return _program;
}

void shader_program::destroy() {
	if (_program) {
		GL_VERIFY(glDeleteProgram(_program));
		_program = 0;
	}
}

void shader_program::fill_attributes() {
	GLint attrib_count = 0;
	GL_VERIFY(glGetProgramiv(_program, GL_ACTIVE_ATTRIBUTES, &attrib_count));

	_attributes.clear();
	_attributes.reserve(attrib_count);

	for (int i = 0; i < attrib_count; ++i) {
		GLsizei length = 0;
		GLint size = 0;
		GLenum type = 0;
		GLchar name[256];
		
		GL_VERIFY(glGetActiveAttrib(_program, (GLuint)i, 256, &length, &size, &type, name));
		
		GLint location = glGetAttribLocation(_program, name);
		GL_CHECK_ERROR("glGetAttribLocation");
		
		_attributes.emplace_back(name, location, size, type);
	}
}

void shader_program::fill_uniforms() {
	GLint uniform_count = 0;
	GL_VERIFY(glGetProgramiv(_program, GL_ACTIVE_UNIFORMS, &uniform_count));

	_uniforms.clear();
	_uniforms.reserve(uniform_count);

	for (int i = 0; i < uniform_count; ++i) {
		GLsizei length = 0;
		GLint size = 0;
		GLenum type = 0;
		GLchar name[256];
		
		GL_VERIFY(glGetActiveUniform(_program, (GLuint)i, 256, &length, &size, &type, name));
		
		GLint location = glGetUniformLocation(_program, name);
		GL_CHECK_ERROR("glGetUniformLocation");
		
		_uniforms.emplace_back(name, location, size, type);
	}
}

const shader_program::attribute* shader_program::find_attribute(const char* name) const {
	for (const auto& a: _attributes) {
		if (a.name == name)
			return &a;
	}
	return nullptr;
}

const shader_program::uniform* shader_program::find_uniform(const char* name) const {
	for (const auto& u: _uniforms) {
		if (u.name == name)
			return &u;
	}
	return nullptr;
}

void shader_program::bind() {
	BOOST_ASSERT(_program);

	GL_VERIFY(glUseProgram(_program));

	renderer& r = renderer::get();

	set_uniform("u_color", 1.0f, 1.0f, 1.0f, 1.0f);

	if (has_uniform("u_model_view_matrix")) {
		set_uniform("u_model_view_matrix",
			glm::value_ptr(r.get_current_matrix(matrix::view) * r.get_current_matrix(matrix::model)), 16);
	}
	
	if (has_uniform("u_view_projection_matrix")) {
		set_uniform("u_view_projection_matrix",
			glm::value_ptr(r.get_current_matrix(matrix::projection) * r.get_current_matrix(matrix::view)), 16);
	}
	
	if (has_uniform("u_model_view_projection_matrix")) {
		set_uniform("u_model_view_projection_matrix",
			glm::value_ptr(r.get_current_matrix(matrix::projection) * r.get_current_matrix(matrix::view) * r.get_current_matrix(matrix::model)), 16);
	}
}

void shader_program::unbind() {
	GL_VERIFY(glUseProgram(0));
}

static inline size_t get_gl_uniform_type_size(GLenum type) {
	switch (type) {
	case GL_FLOAT: return 1;
	case GL_FLOAT_VEC2: return 2;
	case GL_FLOAT_VEC3: return 3;
	case GL_FLOAT_VEC4: return 4;
	case GL_INT: return 1;
	case GL_INT_VEC2: return 2;
	case GL_INT_VEC3: return 3;
	case GL_INT_VEC4: return 4;
	case GL_FLOAT_MAT2: return 4;
	case GL_FLOAT_MAT3: return 9;
	case GL_FLOAT_MAT4: return 16;
	case GL_SAMPLER_2D: return 1;
	case GL_SAMPLER_CUBE: return 1;
	default: BOOST_ASSERT_MSG(false, "Invalid uniform type"); return 0;
	}
}

void shader_program::set_uniform(const char* name, const float* fv, size_t count) {
	BOOST_ASSERT(_program);
	BOOST_ASSERT(name);
	BOOST_ASSERT(fv);
	BOOST_ASSERT(count);

	const uniform* uniform = find_uniform(name);
	if (!uniform)
		return;

	size_t uniform_size = get_gl_uniform_type_size(uniform->type);
	BOOST_ASSERT(!(count % uniform_size));

	GLsizei uniform_count = count / uniform_size;

	switch (uniform->type) {
	case GL_FLOAT:
		GL_VERIFY(glUniform1fv(uniform->location, uniform_count, fv));
		break;
	case GL_FLOAT_VEC2:
		GL_VERIFY(glUniform2fv(uniform->location, uniform_count, fv));
		break;
	case GL_FLOAT_VEC3:
		GL_VERIFY(glUniform3fv(uniform->location, uniform_count, fv));
		break;
	case GL_FLOAT_VEC4:
		GL_VERIFY(glUniform4fv(uniform->location, uniform_count, fv));
		break;
	case GL_FLOAT_MAT2:
		GL_VERIFY(glUniformMatrix2fv(uniform->location, uniform_count, GL_FALSE, fv));
		break;
	case GL_FLOAT_MAT3:
		GL_VERIFY(glUniformMatrix3fv(uniform->location, uniform_count, GL_FALSE, fv));
		break;
	case GL_FLOAT_MAT4:
		GL_VERIFY(glUniformMatrix4fv(uniform->location, uniform_count, GL_FALSE, fv));
		break;
	default:
		BOOST_ASSERT_MSG(false, "Invalid uniform type");
		break;
	}
}

void shader_program::set_uniform(const char* name, const int* iv, size_t count) {
	BOOST_ASSERT(_program);
	BOOST_ASSERT(name);
	BOOST_ASSERT(iv);
	BOOST_ASSERT(count);

	const uniform* uniform = find_uniform(name);
	if (!uniform)
		return;

	size_t uniform_size = get_gl_uniform_type_size(uniform->type);
	BOOST_ASSERT(!(count % uniform_size));

	GLsizei uniform_count = count / uniform_size;

	switch (uniform->type) {
	case GL_INT:
	case GL_SAMPLER_2D:
	case GL_SAMPLER_CUBE:
		GL_VERIFY(glUniform1iv(uniform->location, uniform_count, iv));
		break;
	case GL_INT_VEC2:
		GL_VERIFY(glUniform2iv(uniform->location, uniform_count, iv));
		break;
	case GL_INT_VEC3:
		GL_VERIFY(glUniform3iv(uniform->location, uniform_count, iv));
		break;
	case GL_INT_VEC4:
		GL_VERIFY(glUniform4iv(uniform->location, uniform_count, iv));
		break;
	default:
		BOOST_ASSERT_MSG(false, "Invalid uniform type");
		break;
	}
}

// buffer
//

buffer::buffer()
	: _type(buffer_type::vertex)
	, _decl()
	, _buffer()
	, _native_type()
{
}

buffer::~buffer() {
	destroy();
}

NativeBufferHandleType buffer::create(buffer_type type, const vertex_element_description* decl, size_t size, const void* data, usage usage) {
	destroy();

	_type = type;
	_decl = decl;
	_native_type = (type < buffer_type::index16 ? GL_ARRAY_BUFFER : GL_ELEMENT_ARRAY_BUFFER);

	GL_VERIFY(glGenBuffers(1, &_buffer));
	if (!_buffer) {
		BOOST_THROW_EXCEPTION(std::runtime_error("failed to create buffer"));
	}

	set_data(size, data, usage);

	return _buffer;
}

void buffer::destroy() {
	if (_buffer) {
		GL_VERIFY(glDeleteBuffers(1, &_buffer));
		_buffer = 0;
		_type = buffer_type::vertex;
		_decl = 0;
		_native_type = 0;
	}
}

void buffer::set_data(size_t size, const void* data, usage usage_) {
	BOOST_ASSERT(_buffer);
		
	GLenum Usage = GL_STATIC_DRAW;
	switch (usage_) {
	default:
		BOOST_ASSERT_MSG(false, "defaulting to static usage");
	case usage::static_:
		Usage = GL_STATIC_DRAW;
		break;
	case usage::dynamic_:
		Usage = GL_DYNAMIC_DRAW;
		break;
	case usage::stream:
		Usage = GL_STREAM_DRAW;
		break;
	}

	GLenum PName =
		_native_type == GL_ARRAY_BUFFER ?
			GL_ARRAY_BUFFER_BINDING : GL_ELEMENT_ARRAY_BUFFER_BINDING;

	GLint binding = 0;
	GL_VERIFY(glGetIntegerv(PName, &binding));
	if (_buffer != (GLuint)binding)
		GL_VERIFY(glBindBuffer(_native_type, _buffer));

	// size measures in elements
	size *= _decl == 0 ? sizeof(uint16_t) : _decl->stride;
	
	GL_VERIFY(glBufferData(_native_type, size, data, Usage));

	if (_buffer != (GLuint)binding)
		GL_VERIFY(glBindBuffer(_native_type, binding));
}

void buffer::set_subdata(size_t offset, size_t size, const void* data) {
	BOOST_ASSERT(_buffer);
	BOOST_ASSERT(data);

	GLenum PName =
		_native_type == GL_ARRAY_BUFFER ?
			GL_ARRAY_BUFFER_BINDING : GL_ELEMENT_ARRAY_BUFFER_BINDING;

	GLint binding = 0;
	GL_VERIFY(glGetIntegerv(PName, &binding));
	if (_buffer != (GLuint)binding)
		GL_VERIFY(glBindBuffer(_native_type, _buffer));

	bool fallback = true;

	// size measures in elements
	const size_t stride = _decl == 0 ? sizeof(uint16_t) : _decl->stride;
	size *= stride;
	offset *= stride;

	//if (glMapBufferRangeEXT) {
	//	void* address = glMapBufferRangeEXT(_native_type, offset, size, GL_MAP_WRITE_BIT_EXT);
	//	GL_CHECK_ERROR("glMapBufferRangeEXT");
	//	if (address) {
	//		memcpy(address, data, size);
	//		GL_VERIFY(glFlushMappedBufferRangeEXT(_native_type, offset, size));
	//		fallback = false;
	//	}
	//}

	//if (glMapBufferOES) {
	//	void *address = glMapBufferOES(_native_type, GL_WRITE_ONLY_OES);
	//	GL_CHECK_ERROR("glMapBufferOES");
	//	if (address) {
	//		memcpy((uint8_t*)address + offset, data, size);
	//		GLboolean result = glUnmapBufferOES(_native_type);
	//		GL_CHECK_ERROR("glUnmapBufferOES");
	//		BOOST_ASSERT(!!result);
	//		fallback = false;
	//	}
	//}

	if (fallback) {
		GL_VERIFY(glBufferSubData(_native_type, offset, size, data));
	}

	if (_buffer != (GLuint)binding)
		GL_VERIFY(glBindBuffer(_native_type, binding));
}

// texture
//

texture::texture()
	: _texture()
{
}

texture::~texture() {
	destroy();
}

static inline texture_format get_format_from_pixel_type(imaging::pixel_type pixel_type) {
	switch (pixel_type) {
	case imaging::pixel_type::luminance: return texture_format::l8;
	case imaging::pixel_type::alpha: return texture_format::a8;
	case imaging::pixel_type::luminance_alpha: return texture_format::la8;
	case imaging::pixel_type::rgb8: return texture_format::rgb8;
	case imaging::pixel_type::rgba8: return texture_format::rgba8;
	case imaging::pixel_type::rgb565: return texture_format::rgb565;
	case imaging::pixel_type::rgba4444: return texture_format::rgba4444;
	case imaging::pixel_type::rgba5551: return texture_format::rgba5551;
	default: BOOST_ASSERT_MSG(false, "invalid pixel type");
	}

	BOOST_THROW_EXCEPTION(std::runtime_error("invalid pixel type"));
}

static inline void get_gl_texture_format(texture_format format, GLenum& Format, GLenum& Type) {
	switch (format) {
	case texture_format::l8: Format = GL_LUMINANCE; Type = GL_UNSIGNED_BYTE; break;
	case texture_format::a8: Format = GL_ALPHA; Type = GL_UNSIGNED_BYTE; break;
	case texture_format::la8: Format = GL_LUMINANCE_ALPHA; Type = GL_UNSIGNED_BYTE; break;
	case texture_format::rgb8: Format = GL_RGB; Type = GL_UNSIGNED_BYTE; break;
	case texture_format::rgba8: Format = GL_RGBA; Type = GL_UNSIGNED_BYTE; break;
	case texture_format::rgb565: Format = GL_RGB; Type = GL_UNSIGNED_SHORT_5_6_5; break;
	case texture_format::rgba5551: Format = GL_RGBA; Type = GL_UNSIGNED_SHORT_5_5_5_1; break;
	case texture_format::rgba4444: Format = GL_RGBA; Type = GL_UNSIGNED_SHORT_4_4_4_4; break;
	default:
		BOOST_ASSERT_MSG(false, "invalid texture format");
		BOOST_THROW_EXCEPTION(std::runtime_error("invalid texture format"));
	}
}

void texture::destroy() {
	if (_texture) {
		GL_VERIFY(glDeleteTextures(1, &_texture));
		_texture = 0;
	}
}

// texture2d

texture2d::texture2d()
	: _width(0)
	, _height(0)
{
}

NativeTextureHandleType texture2d::create(texture_address address, texture_filter filter) {
	destroy();

	GL_VERIFY(glGenTextures(1, &_texture));
	GL_VERIFY(glBindTexture(GL_TEXTURE_2D, _texture));

	GLint Address = GL_REPEAT;
	switch (address) {
	default:
		BOOST_ASSERT_MSG(false, "defaulting to repeat");
	case texture_address::repeat:
		Address = GL_REPEAT;
		break;
	case texture_address::clamp_to_edge:
		Address = GL_CLAMP_TO_EDGE;
		break;
	case texture_address::mirrored_repeat:
		Address = GL_MIRRORED_REPEAT;
		break;
	}

	GL_VERIFY(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, Address));
	GL_VERIFY(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, Address));

	GLint Filter = GL_NEAREST;
	switch (filter) {
	default:
		BOOST_ASSERT_MSG(false, "defaulting to nearest");
	case texture_filter::nearest:
		Filter = GL_NEAREST;
		break;
	case texture_filter::linear:
		Filter = GL_LINEAR;
		break;
	}

	GL_VERIFY(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, Filter));
	GL_VERIFY(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, Filter));

	GL_VERIFY(glBindTexture(GL_TEXTURE_2D, 0));

	return _texture;
}

NativeTextureHandleType texture2d::create(imaging::image* image, texture_address address, texture_filter filter) {
	imaging::image_ptr guard(image);

	BOOST_ASSERT_MSG(image, "invalid image");
	BOOST_ASSERT_MSG(!!image->get_pixels(), "image not loaded");

	create(address, filter);
	set_image(image);

	return _texture;
}

NativeTextureHandleType texture2d::create(size_t width, size_t height, texture_format format, texture_address address, texture_filter filter) {
	BOOST_ASSERT(width > 0);
	BOOST_ASSERT(height > 0);

	create(address, filter);

	GLenum Format = 0, Type = 0;
	get_gl_texture_format(format, Format, Type);

	GL_VERIFY(glBindTexture(GL_TEXTURE_2D, _texture));
	GL_VERIFY(glTexImage2D(GL_TEXTURE_2D, 0, Format, width, height, 0, Format, Type, nullptr));
	GL_VERIFY(glBindTexture(GL_TEXTURE_2D, 0));

	_width = width;
	_height = height;

	return _texture;
}

void texture2d::set_image(imaging::image* image) {
	imaging::image_ptr guard(image);

	BOOST_ASSERT_MSG(image, "invalid image");
	BOOST_ASSERT_MSG(!!_texture, "texture not loaded");

	_width = image->get_width();
	_height = image->get_height();

	BOOST_ASSERT(_width > 0);
	BOOST_ASSERT(_height > 0);

	GLenum Format = 0, Type = 0;
	get_gl_texture_format(get_format_from_pixel_type(image->get_pixel_type()), Format, Type);

	GL_VERIFY(glBindTexture(GL_TEXTURE_2D, _texture));
	GL_VERIFY(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
	GL_VERIFY(glTexImage2D(GL_TEXTURE_2D, 0, Format, _width, _height, 0, Format, Type, image->get_pixels()));
	GL_VERIFY(glBindTexture(GL_TEXTURE_2D, 0));
}

void texture2d::set_subimage(imaging::image* image, size_t xoffset, size_t yoffset) {
	imaging::image_ptr guard(image);

	BOOST_ASSERT_MSG(image, "invalid image");
	BOOST_ASSERT_MSG(_width >= xoffset + image->get_width() && _height >= yoffset + image->get_height(), "image dimentions don't suit texture size");
	BOOST_ASSERT_MSG(!!image->get_pixels(), "image not loaded");
	BOOST_ASSERT_MSG(!!_texture, "texture not loaded");

	GLenum Format = 0, Type = 0;
	get_gl_texture_format(get_format_from_pixel_type(image->get_pixel_type()), Format, Type);

	GL_VERIFY(glBindTexture(GL_TEXTURE_2D, _texture));
	GL_VERIFY(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
	GL_VERIFY(glTexSubImage2D(GL_TEXTURE_2D, 0, xoffset, yoffset, image->get_width(), image->get_height(), Format, Type, image->get_pixels()));
	GL_VERIFY(glBindTexture(GL_TEXTURE_2D, 0));
}

// sprite_batch
//

sprite_batch::shared::shared()
	: vertices_position(0)
	, _in_immediate_mode(false)
{
	std::vector<uint16_t> indices;
	indices.reserve(max_batch_size * indices_per_sprite);
	for (uint16_t i = 0; i < max_batch_size * vertices_per_sprite; i += vertices_per_sprite) {
		indices.push_back(i + 0);
		indices.push_back(i + 1);
		indices.push_back(i + 2);
		indices.push_back(i + 0);
		indices.push_back(i + 2);
		indices.push_back(i + 3);
	}

	ib.create(buffer_type::index16, nullptr, max_batch_size * indices_per_sprite, &indices.front());
	vb.create(buffer_type::vertex, vertex_type::decl(), max_batch_size * vertices_per_sprite, nullptr, usage::dynamic_);

	vertices.resize(max_batch_size * vertices_per_sprite);
}

sprite_batch::sprite_batch()
	: _sprites_count()
	, _in_begin_end_pair(false)
	, _sort(sprite_sort::deferred)
{
	_shared = application::get_instance()->get_shared_object<shared>("sprite_batch");
	if (!_shared) {
		_shared = application::get_instance()->add_shared_object<shared>("sprite_batch", new shared());
	}
}

void sprite_batch::begin(sprite_sort mode) {
	if (_in_begin_end_pair) {
		BOOST_THROW_EXCEPTION(std::runtime_error("cannot neste begin() calls on a single sprite_batch"));
	}

	_in_begin_end_pair = true;
	_sort = mode;
	
	if (_sort == sprite_sort::immediate) {
		if (_shared->_in_immediate_mode) {
			BOOST_THROW_EXCEPTION(std::runtime_error("only one sprite_batch at a time can use immediate mode"));
		}
		_shared->_in_immediate_mode = true;
	}

	renderer& r = renderer::get();

	r.set_geometry(&_shared->vb, &_shared->ib);
}

void sprite_batch::draw(texture2d* tex, blend_mode blend_mode, const rectf& src, const rectf& dest, uint32_t color, float depth) {
	if (!_in_begin_end_pair) {
		BOOST_THROW_EXCEPTION(std::runtime_error("begin() must be called before draw()"));
	}

	if (_sprites_count >= _sprites.size()) {
		size_t new_size = std::max<size_t>(initial_queue_size, _sprites.size() * 2);
		_sprites.resize(new_size);
		_sorted_sprites.clear();
	}

	sprite_info* sprite = &_sprites[_sprites_count];

	sprite->tex = tex;
	sprite->blend_mode = blend_mode;

	const float width_inv = tex ? (1.0f / tex->get_width()) : 0.0f;
	const float height_inv = tex ? (1.0f / tex->get_height()) : 0.0f;

	const float tu01 = src.x * width_inv;
	const float tv03 = src.y * height_inv;
	const float tu23 = (src.x + src.width) * width_inv;
	const float tv12 = (src.y + src.height) * height_inv;

	sprite->quad[0].x = dest.x;
	sprite->quad[0].y = dest.y;
	sprite->quad[0].z = depth;
	sprite->quad[0].color = color;
	sprite->quad[0].tu = tu01;
	sprite->quad[0].tv = tv03;

	sprite->quad[1].x = dest.x;
	sprite->quad[1].y = dest.y + dest.height;
	sprite->quad[1].z = depth;
	sprite->quad[1].color = color;
	sprite->quad[1].tu = tu01;
	sprite->quad[1].tv = tv12;

	sprite->quad[2].x = dest.x + dest.width;
	sprite->quad[2].y = dest.y + dest.height;
	sprite->quad[2].z = depth;
	sprite->quad[2].color = color;
	sprite->quad[2].tu = tu23;
	sprite->quad[2].tv = tv12;

	sprite->quad[3].x = dest.x + dest.width;
	sprite->quad[3].y = dest.y;
	sprite->quad[3].z = depth;
	sprite->quad[3].color = color;
	sprite->quad[3].tu = tu23;
	sprite->quad[3].tv = tv03;

	if (_sort == sprite_sort::immediate) {
		render_batch(tex, blend_mode, &sprite, 1);
	} else {
		_sprites_count++;
	}
}

void sprite_batch::draw_rect(blend_mode blend_mode, const rectf& dest, uint32_t color, float depth) {
	draw(nullptr, blend_mode, rectf(), dest, color, depth);
}

void sprite_batch::end() {
	if (!_in_begin_end_pair) {
		BOOST_THROW_EXCEPTION(std::runtime_error("begin() must be called before end()"));
	}

	if (_sort == sprite_sort::immediate) {
		_shared->_in_immediate_mode = false;
	} else {
		if (_shared->_in_immediate_mode) {
			BOOST_THROW_EXCEPTION(std::runtime_error("cannot end one sprite_batch while another is using immediate mode"));
		}

		flush();
	}

	_in_begin_end_pair = false;

	renderer& r = renderer::get();

	r.set_geometry(0, 0);
	r.set_texture(0);
	r.set_blend_mode(blend_mode::none);
}

void sprite_batch::flush() {
	if (!_in_begin_end_pair) {
		BOOST_THROW_EXCEPTION(std::runtime_error("begin() must be called before flush()"));
	}

	if (!_sprites_count) {
		return;
	}

	sort_sprites();

	texture* batch_tex = _sorted_sprites[0]->tex;
	blend_mode batch_blend_mode = _sorted_sprites[0]->blend_mode;
	size_t batch_start = 0;

	for (size_t i = 0; i < _sprites_count; ++i) {
		texture* tex = _sorted_sprites[i]->tex;
		blend_mode blend_mode = _sorted_sprites[i]->blend_mode;

		if (blend_mode != batch_blend_mode || tex != batch_tex || tex && batch_tex && tex->get_native_handle() != batch_tex->get_native_handle()) {
			if (i > batch_start) {
				render_batch(batch_tex, batch_blend_mode, &_sorted_sprites[batch_start], i - batch_start);
			}

			batch_tex = tex;
			batch_blend_mode = blend_mode;
			batch_start = i;
		}
	}

	render_batch(batch_tex, batch_blend_mode, &_sorted_sprites[batch_start], _sprites_count - batch_start);
	_sprites_count = 0;

	if (_sort != sprite_sort::deferred) {
		_sorted_sprites.clear();
	}
}

void sprite_batch::sort_sprites() {
	if (_sorted_sprites.size() < _sprites_count) {
		size_t previous_size = _sorted_sprites.size();
		
		_sorted_sprites.resize(_sprites_count);		
		for (size_t i = previous_size; i < _sprites_count; ++i) {
			_sorted_sprites[i] = &_sprites[i];
		}
	}

	switch (_sort) {
	case sprite_sort::sort_by_texture:
		std::sort(_sorted_sprites.begin(), _sorted_sprites.begin() + _sprites_count,
			[](const sprite_info* s1, const sprite_info* s2) {
				if (!s1->tex) return true;
				if (!s2->tex) return false;
				return s1->tex->get_native_handle() < s2->tex->get_native_handle();
			});
		break;
	case sprite_sort::sort_by_blend:
		std::sort(_sorted_sprites.begin(), _sorted_sprites.begin() + _sprites_count,
			[](const sprite_info* s1, const sprite_info* s2) {
				return s1->blend_mode < s2->blend_mode;
			});
		break;
	case sprite_sort::sort_back_to_front:
		std::sort(_sorted_sprites.begin(), _sorted_sprites.begin() + _sprites_count,
			[](const sprite_info* s1, const sprite_info* s2) {
				return s1->quad[0].z < s2->quad[0].z;
			});
		break;
	case sprite_sort::sort_front_to_back:
		std::sort(_sorted_sprites.begin(), _sorted_sprites.begin() + _sprites_count,
			[](const sprite_info* s1, const sprite_info* s2) {
				return s1->quad[0].z > s2->quad[0].z;
			});
		break;
	}
}

void sprite_batch::render_batch(texture* tex, blend_mode blend_mode, const sprite_info* const* sprites, size_t count) {
	if (!count) {
		return;
	}

	renderer& r = renderer::get();

	r.set_texture(tex);
	r.set_blend_mode(blend_mode);

	while (count > 0) {
		size_t batch_size = count;
		size_t remaining_space = max_batch_size - _shared->vertices_position;

		if (batch_size > remaining_space) {
			if (remaining_space < max_batch_size) {
				_shared->vertices_position = 0;
				batch_size = std::min<size_t>(count, max_batch_size);
			} else {
				batch_size = remaining_space;
			}
		}

		const size_t start_vertex = _shared->vertices_position * vertices_per_sprite;
		const size_t vertex_count = batch_size * vertices_per_sprite;
		
		const size_t start_index = _shared->vertices_position * indices_per_sprite;
		const size_t index_count = batch_size * indices_per_sprite;

		vertex_position_color_texture* vertices = &_shared->vertices[start_vertex];
		for (size_t i = 0; i < batch_size; ++i) {
			memcpy(vertices, sprites[i]->quad, sizeof(vertex_type) * vertices_per_sprite);
			vertices += vertices_per_sprite;
		}

		_shared->vb.set_subdata(start_vertex, vertex_count, &_shared->vertices[start_vertex]);
		
		r.draw_elements(primitives::triangles, index_count, start_index);

		_shared->vertices_position += batch_size;

		sprites += batch_size;
		count -= batch_size;
	}
}

// render to texture
//

render_to_texture::~render_to_texture() {
	destroy();
}

NativeFramebufferObjectType render_to_texture::create(size_t width, size_t height, texture_format format, bool create_depth_buffer) {
	clear();

	_texture.reset(new texture2d());
	_texture->create(width, height, format);

	if (create_depth_buffer) {
		GL_VERIFY(glGenRenderbuffers(1, &_depth_renderbuffer));
		GL_VERIFY(glBindRenderbuffer(GL_RENDERBUFFER, _depth_renderbuffer));
		GL_VERIFY(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, _texture->get_width(), _texture->get_height()));
		GL_VERIFY(glBindRenderbuffer(GL_RENDERBUFFER, 0));
	}

	// save current FBO
	GLint fbo = 0;
	GL_VERIFY(glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo));

	GL_VERIFY(glGenFramebuffers(1, &_framebuffer));
	GL_VERIFY(glBindFramebuffer(GL_FRAMEBUFFER, _framebuffer));
	GL_VERIFY(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _texture->get_native_handle(), 0));
	if (create_depth_buffer) {
		GL_VERIFY(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, _depth_renderbuffer));
	}

	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	GL_CHECK_ERROR("glCheckFramebufferStatus");
	if (status != GL_FRAMEBUFFER_COMPLETE) {
		BOOST_THROW_EXCEPTION(std::runtime_error("failed to create framebuffer object"));
	}

	// restore previous FBO
	GL_VERIFY(glBindFramebuffer(GL_FRAMEBUFFER, fbo));

	return _framebuffer;
}

void render_to_texture::destroy() {
	if (_depth_renderbuffer) {
		GL_VERIFY(glDeleteRenderbuffers(1, &_depth_renderbuffer));
		_depth_renderbuffer = 0;
	}

	if (_framebuffer) {
		GL_VERIFY(glDeleteFramebuffers(1, &_framebuffer));
		_framebuffer = 0;
	}

	_texture.reset();
}

void render_to_texture::begin() {
	render::renderer& r = render::renderer::get();
	r.push_state();

	GL_VERIFY(glBindFramebuffer(GL_FRAMEBUFFER, _framebuffer));

	r.set_viewport(0, 0, _texture->get_width(), _texture->get_height());
}

void render_to_texture::end() {
	render::renderer& r = render::renderer::get();
	r.pop_state();
}

// renderer
//

renderer& renderer::get() {
	renderer_ptr instance = application::get_instance()->get_shared_object<renderer>("renderer");
	if (!instance) {
		instance = application::get_instance()->add_shared_object<renderer>("renderer", new renderer());
	}

	return *instance;
}

renderer::renderer()
	: _enable_texturing(true)
	, _enable_depth_test(false)
	, _enable_depth_write(true)
	, _enable_stencil_test(false)
	, _enable_scissor_test(false)
	, _cull_mode(cull_mode::none)
	, _blend_mode(blend_mode::none)
{
	init_extensions();

	GL_CLEAR_ERROR();

	LOGI("GL vendor: ") << glGetString(GL_VENDOR);
	GL_CHECK_ERROR("glGetString");
	
	LOGI("GL renderer: ") << glGetString(GL_RENDERER);
	GL_CHECK_ERROR("glGetString");
	
	LOGI("GL version: ") << glGetString(GL_VERSION);
	GL_CHECK_ERROR("glGetString");
	
	LOGI("GL shader version: ") << glGetString(GL_SHADING_LANGUAGE_VERSION);
	GL_CHECK_ERROR("glGetString");
	
	LOGI("GL extensions: ") << glGetString(GL_EXTENSIONS);
	GL_CHECK_ERROR("glGetString");

	std::string extensions = (const char*)glGetString(GL_EXTENSIONS);
	boost::split(_extensions, extensions, boost::is_any_of(" "));
	std::sort(_extensions.begin(), _extensions.end());

#define LOG_PARAM(param) { GLint n = 0; GL_VERIFY(glGetIntegerv(param, &n)); LOGI(#param ": ") << n; }
	LOG_PARAM(GL_MAX_TEXTURE_SIZE);
	LOG_PARAM(GL_MAX_TEXTURE_IMAGE_UNITS);
	LOG_PARAM(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS);
	LOG_PARAM(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS);
	LOG_PARAM(GL_MAX_RENDERBUFFER_SIZE);
	LOG_PARAM(GL_MAX_VERTEX_UNIFORM_VECTORS);
	LOG_PARAM(GL_MAX_FRAGMENT_UNIFORM_VECTORS);
	LOG_PARAM(GL_NUM_COMPRESSED_TEXTURE_FORMATS);
#undef LOG_PARAM

	for (auto i = 0; i < enum_traits<matrix>::num_items; ++i) {
		_matrix_stack[i].emplace_back();
	}
}

void renderer::enable_texturing(bool enable) {
	_enable_texturing = enable;
}

void renderer::enable_depth_test(bool enable) {
	if (_enable_depth_test != enable) {
		_enable_depth_test = enable;
		if (enable) {
			GL_VERIFY(glEnable(GL_DEPTH_TEST));
		} else {
			GL_VERIFY(glDisable(GL_DEPTH_TEST));
		}
	}
}

void renderer::enable_depth_write(bool enable) {
	if (_enable_depth_write != enable) {
		_enable_depth_write = enable;
		GL_VERIFY(glDepthMask(enable));
	}
}

void renderer::enable_color_write(bool red, bool green, bool blue, bool alpha) {
	GL_VERIFY(glColorMask(red, green, blue, alpha));
}

void renderer::enable_stencil_test(bool enable) {
	if (_enable_stencil_test != enable) {
		_enable_stencil_test = enable;
		if (enable) {
			GL_VERIFY(glEnable(GL_STENCIL_TEST));
		} else {
			GL_VERIFY(glDisable(GL_STENCIL_TEST));
		}
	}
}

void renderer::enable_scissor_test(bool enable) {
	if (_enable_scissor_test != enable) {
		_enable_scissor_test = enable;
		if (enable) {
			GL_VERIFY(glEnable(GL_SCISSOR_TEST));
		} else {
			GL_VERIFY(glDisable(GL_SCISSOR_TEST));
		}
	}
}

void renderer::set_blend_mode(blend_mode blend_mode) {
	if (_blend_mode != blend_mode) {
		_blend_mode = blend_mode;
		if (blend_mode != blend_mode::none) {
			GL_VERIFY(glEnable(GL_BLEND));
			GLenum Equation = GL_FUNC_ADD;
			GLenum srcRGB = GL_ONE, dstRGB = GL_ZERO, srcAlpha = GL_ONE, dstAlpha = GL_ZERO;
			switch (blend_mode) {
			case blend_mode::modulate:
				srcRGB = GL_SRC_ALPHA; dstRGB = GL_ONE_MINUS_SRC_ALPHA;
				srcAlpha = GL_ONE_MINUS_DST_ALPHA; dstAlpha = GL_ONE;
				break;
			case blend_mode::add:
				srcRGB = GL_SRC_ALPHA; dstRGB = GL_ONE;
				srcAlpha = GL_ONE_MINUS_DST_ALPHA; dstAlpha = GL_ONE;
				break;
			case blend_mode::multiply:
				srcRGB = GL_DST_COLOR; dstRGB = GL_ZERO;
				srcAlpha = GL_ONE_MINUS_DST_ALPHA; dstAlpha = GL_ONE;
				break;
			case blend_mode::replace:
				srcRGB = GL_ONE; dstRGB = GL_ZERO;
				srcAlpha = GL_ONE_MINUS_DST_ALPHA; dstAlpha = GL_ONE;
				break;
			}
			GL_VERIFY(glBlendEquation(Equation));
			GL_VERIFY(glBlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha));
		} else {
			GL_VERIFY(glDisable(GL_BLEND));
		}
	}
}

void renderer::set_cull_mode(cull_mode cull_mode) {
	if (_cull_mode != cull_mode) {
		_cull_mode = cull_mode;
		if (_cull_mode != cull_mode::none) {
			GL_VERIFY(glEnable(GL_CULL_FACE));
			switch (cull_mode) {
			case cull_mode::front:
				GL_VERIFY(glCullFace(GL_FRONT));
				break;
			case cull_mode::back:
				GL_VERIFY(glCullFace(GL_BACK));
				break;
			case cull_mode::front_and_back:
				GL_VERIFY(glCullFace(GL_FRONT_AND_BACK));
				break;
			default:
				GL_VERIFY(glDisable(GL_CULL_FACE));
			}
		} else {
			GL_VERIFY(glDisable(GL_CULL_FACE));
		}
	}
}

void renderer::push_state() {
	_state_stack.emplace_back();
	state& s = _state_stack.back();

	GL_VERIFY(glGetIntegerv(GL_VIEWPORT, s.viewport));
	GL_VERIFY(glGetIntegerv(GL_FRAMEBUFFER_BINDING, &s.framebuffer));
	GL_VERIFY(glGetBooleanv(GL_COLOR_WRITEMASK, s.color_writemask));
	GL_VERIFY(glGetBooleanv(GL_DEPTH_WRITEMASK, &s.depth_writemask));
	GL_VERIFY(glGetBooleanv(GL_DEPTH_TEST, &s.depth_test));
	GL_VERIFY(glGetFloatv(GL_COLOR_CLEAR_VALUE, s.color_clear_value));
	GL_VERIFY(glGetFloatv(GL_DEPTH_CLEAR_VALUE, &s.depth_clear_value));
}

void renderer::pop_state() {
	BOOST_ASSERT(!_state_stack.empty());
	state& s = _state_stack.back();

	GL_VERIFY(glViewport(s.viewport[0], s.viewport[1], s.viewport[2], s.viewport[3]));
	GL_VERIFY(glBindFramebuffer(GL_FRAMEBUFFER, s.framebuffer));
	GL_VERIFY(glColorMask(s.color_writemask[0], s.color_writemask[1], s.color_writemask[2], s.color_writemask[3]));
	GL_VERIFY(glDepthMask(s.depth_writemask));
	GL_VERIFY(glClearColor(s.color_clear_value[0], s.color_clear_value[1], s.color_clear_value[2], s.color_clear_value[3]));
	GL_VERIFY(glClearDepthf(s.depth_clear_value));
	if (s.depth_test) {
		GL_VERIFY(glEnable(GL_DEPTH_TEST));
	} else {
		GL_VERIFY(glDisable(GL_DEPTH_TEST));
	}
	_state_stack.pop_back();
}

void renderer::set_viewport(int x, int y, int width, int height) {
	GL_VERIFY(glViewport(x, y, width, height));
}

void renderer::set_scissor_rect(int x, int y, int width, int height) {
	GL_VERIFY(glScissor(x, y, width, height));
}

void renderer::set_geometry(const buffer* vertices, const buffer* indices) {
	if (vertices > 0) {
		BOOST_ASSERT(vertices->get_type() < buffer_type::index16);
		GL_VERIFY(glBindBuffer(GL_ARRAY_BUFFER, vertices->get_native_handle()));
		bind_vertex_attributes(vertices->get_decl());
	} else if (!vertices) {
		GL_VERIFY(glBindBuffer(GL_ARRAY_BUFFER, 0));
		unbind_all_vertex_attributes();
	}

	if (indices > 0) {
		BOOST_ASSERT(indices->get_type() == buffer_type::index16);
		GL_VERIFY(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices->get_native_handle()));
	} else if (!indices) {
		GL_VERIFY(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
	}
}

static inline GLenum get_gl_type_from_element_type(vertex_element_type type) {
	switch (type) {
	case vertex_element_type::byte: return GL_BYTE;
	case vertex_element_type::ubyte: return GL_UNSIGNED_BYTE;
	case vertex_element_type::short_: return GL_SHORT;
	case vertex_element_type::ushort: return GL_UNSIGNED_SHORT;
	case vertex_element_type::int_: return GL_INT;
	case vertex_element_type::uint: return GL_UNSIGNED_INT;
	case vertex_element_type::float_: return GL_FLOAT;
	default: BOOST_ASSERT("Invalid vertex element type"); return GL_FLOAT;
	}
}

void renderer::bind_vertex_attributes(const vertex_element_description* decl) {
	for (const vertex_element_description* p = decl; p->size; ++p) {
		GL_VERIFY(glEnableVertexAttribArray((GLuint)p->location));
		GL_VERIFY(glVertexAttribPointer((GLuint)p->location, p->size, get_gl_type_from_element_type(p->type), p->normalized, p->stride, (const GLvoid*)p->offset));
	}

}

void renderer::unbind_vertex_attributes(const vertex_element_description* decl) {
	for (const vertex_element_description* p = decl; p->size; ++p) {
		GL_VERIFY(glDisableVertexAttribArray((GLuint)p->location));
	}
}

void renderer::unbind_all_vertex_attributes() {
	// disable all attributes
	for (GLuint index = 0; index < (GLuint)enum_traits<attribute_location>::num_items; ++index) {
		GL_VERIFY(glDisableVertexAttribArray(index));
	}
}

void renderer::set_texture(const texture* tex, uint32_t stage) {
	GL_VERIFY(glActiveTexture(GL_TEXTURE0 + stage));
	GL_VERIFY(glBindTexture(GL_TEXTURE_2D, (_enable_texturing && tex) ? tex->get_native_handle() : 0));
}

void renderer::set_shader_program(const shader_program* program) {
	GL_VERIFY(glUseProgram(program ? program->get_native_handle() : 0));
}

void renderer::clear_color_value(const color_value& c) {
	GL_VERIFY(glClearColor(c.red(), c.green(), c.blue(), c.alpha()));
}

void renderer::clear_depth_value(float d) {
	GL_VERIFY(glClearDepthf(d));
}

void renderer::clear_stencil_value(int s) {
	GL_VERIFY(glClearStencil(s));
}

void renderer::clear(render::clear mode) {
	GLbitfield Mask = 0;
	
	if ((uint32_t)mode & (uint32_t)clear::color)
		Mask |= GL_COLOR_BUFFER_BIT;
	
	if ((uint32_t)mode & (uint32_t)clear::depth)
		Mask |= GL_DEPTH_BUFFER_BIT;
	
	if ((uint32_t)mode & (uint32_t)clear::stencil)
		Mask |= GL_STENCIL_BUFFER_BIT;

	GL_VERIFY(glClear(Mask));
}

static inline GLenum get_gl_primitives_type(primitives mode) {
	switch (mode) {
	case primitives::points: return GL_POINTS;
	case primitives::lines: return GL_LINES;
	case primitives::line_loop: return GL_LINE_LOOP;
	case primitives::line_strip: return GL_LINE_STRIP;
	case primitives::triangles: return GL_TRIANGLES;
	case primitives::triangle_strip: return GL_TRIANGLE_STRIP;
	case primitives::triangle_fan: return GL_TRIANGLE_FAN;
	default: BOOST_ASSERT_MSG(false, "Unknown primitives mode"); return GL_TRIANGLES;
	};
}

void renderer::draw(primitives mode, size_t elements_count, size_t offset) {
	GL_VERIFY(glDrawArrays(get_gl_primitives_type(mode), offset, elements_count));
}

void renderer::draw_elements(primitives mode, size_t elements_count, size_t offset) {
	GL_VERIFY(glDrawElements(get_gl_primitives_type(mode), elements_count, GL_UNSIGNED_SHORT, (const GLvoid*)(offset * sizeof(uint16_t))));
}

void renderer::matrix_push(matrix type) {
	_matrix_stack[(size_t)type].push_back(_matrix_stack[(size_t)type].back());
}

void renderer::matrix_pop(matrix type) {
	_matrix_stack[(size_t)type].pop_back();
}

void renderer::matrix_set(matrix type, const glm::mat4& m) {
	_matrix_stack[(size_t)type].back() = m;
}

void renderer::matrix_mult(matrix type, const glm::mat4& m) {
	_matrix_stack[(size_t)type].back() = _matrix_stack[(size_t)type].back() * m;
}

void renderer::matrix_mult_local(matrix type, const glm::mat4& m) {
	_matrix_stack[(size_t)type].back() = m * _matrix_stack[(size_t)type].back();
}

void renderer::matrix_translate(matrix type, const glm::vec3& d) {
	_matrix_stack[(size_t)type].back() = glm::translate(_matrix_stack[(size_t)type].back(), d);
}

void renderer::matrix_translate(matrix type, float dx, float dy, float dz) {
	_matrix_stack[(size_t)type].back() = glm::translate(_matrix_stack[(size_t)type].back(), glm::vec3(dx, dy, dz));
}

void renderer::matrix_scale(matrix type, const glm::vec3& s) {
	_matrix_stack[(size_t)type].back() = glm::scale(_matrix_stack[(size_t)type].back(), s);
}

void renderer::matrix_scale(matrix type, float sx, float sy, float sz) {
	_matrix_stack[(size_t)type].back() = glm::scale(_matrix_stack[(size_t)type].back(), glm::vec3(sx, sy, sz));
}

void renderer::matrix_scale(matrix type, float s) {
	_matrix_stack[(size_t)type].back() = glm::scale(_matrix_stack[(size_t)type].back(), glm::vec3(s));
}

const glm::mat4& renderer::get_current_matrix(matrix type) const {
	return _matrix_stack[(size_t)type].back();
}

void renderer::init_extensions() {
	glMapBufferOES = (PFNGLMAPBUFFEROESPROC)eglGetProcAddress("glMapBufferOES");
	glUnmapBufferOES = (PFNGLUNMAPBUFFEROESPROC)eglGetProcAddress("glUnmapBufferOES");
	glGetBufferPointervOES = (PFNGLGETBUFFERPOINTERVOESPROC)eglGetProcAddress("glGetBufferPointervOES");

	glMapBufferRangeEXT = (PFNGLMAPBUFFERRANGEEXTPROC)eglGetProcAddress("glMapBufferRangeEXT");
	glFlushMappedBufferRangeEXT = (PFNGLFLUSHMAPPEDBUFFERRANGEEXTPROC)eglGetProcAddress("glFlushMappedBufferRangeEXT");
}

PFNGLMAPBUFFEROESPROC glMapBufferOES = 0;
PFNGLUNMAPBUFFEROESPROC glUnmapBufferOES = 0;
PFNGLGETBUFFERPOINTERVOESPROC glGetBufferPointervOES = 0;

PFNGLMAPBUFFERRANGEEXTPROC glMapBufferRangeEXT = 0;
PFNGLFLUSHMAPPEDBUFFERRANGEEXTPROC glFlushMappedBufferRangeEXT = 0;

} // namespace render
