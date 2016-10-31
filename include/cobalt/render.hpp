#pragma once

// Classes in this file:
//     color
//     color_value
//     colors
//     shader
//     shader_program
//     texture
//     vertex_position_color
//     vertex_position_texture
//     vertex_position_color_texture
//     vertex_position_normal_texture
//     vertex_position_normal_beta4_texture
//     vertex_position_normal_beta5_texture
//     buffer
//     sprite_batch
//     render_to_texture
//     renderer

#include "cool/common.hpp"
#include "cool/geometry.hpp"
#include "cool/io.hpp"
#include "cool/imaging.hpp"
#include "cool/application.hpp"
#include "cool/enum_traits.hpp"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <vector>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace render {

class color_value;

// color
class color {
public:
	color() : b(255), g(255), r(255), a(255) {}
	color(uint8_t b, uint8_t g, uint8_t r) : b(b), g(g), r(r), a(255) {}
	color(uint8_t b, uint8_t g, uint8_t r, uint8_t a) : b(b), g(g), r(r), a(a) {}

	void set(uint8_t blue, uint8_t green, uint8_t red, uint8_t alpha) {
		b = blue; g = green; r = red; a = alpha;
	}

	uint32_t value() const { return c; }

	uint8_t blue() const { return b; }
	uint8_t green() const { return g; }
	uint8_t red() const { return r; }
	uint8_t alpha() const { return a; }

	static color from_color_value(const color_value& c);

private:
	union {
		uint32_t c;
		struct {
			uint8_t b, g, r, a;
		};
	};
};

// color value
class color_value {
public:
	color_value() : b(1.0f), g(1.0f), r(1.0f), a(1.0f) {}
	color_value(float b, float g, float r) { set(b, g, r, 1.0f); }
	color_value(float b, float g, float r, float a) { set(b, g, r, a); }

	void set(float blue, float green, float red, float alpha) {
		// clamp values
		b = std::max(0.0f, std::min(blue, 1.0f));
		g = std::max(0.0f, std::min(green, 1.0f));
		r = std::max(0.0f, std::min(red, 1.0f));
		a = std::max(0.0f, std::min(alpha, 1.0f));
	}

	float blue() const { return b; }
	float green() const { return g; }
	float red() const { return r; }
	float alpha() const { return a; }

	static color_value from_color(const color& c);

private:
	float b, g, r, a;
};

// colors
class colors {
public:
	static color_value black();
	static color_value white();
	static color_value red();
	static color_value green();
	static color_value blue();
	static color_value cornflower_blue();
};

DEFINE_ENUM_CLASS(
	shader_type, uint32_t,
	vertex,
	fragment
)

DEFINE_ENUM_CLASS(
	attribute_location, uint32_t,
	a_position,
	a_normal,
	a_color,
	a_texcoord,
	a_texcoord2,
	a_texcoord3,
	a_texcoord4,
	a_blendweights,
	a_blendindices
)

typedef GLuint NativeShaderHandleType;

/// shader
class shader : public ref_counter<shader> {
public:
	shader();
	~shader();

	NativeShaderHandleType create(shader_type type, const char* sources[], size_t count);
	void destroy();

	NativeShaderHandleType get_native_handle() const { return _shader; }

private:
	NativeShaderHandleType _shader;

private:
	DISALLOW_COPY_AND_ASSIGN(shader)
};

typedef boost::intrusive_ptr<shader> shader_ptr;

typedef GLuint NativeShaderProgramHandleType;

/// shader program
class shader_program : public ref_counter<shader_program> {
public:
	shader_program();
	~shader_program();

	void add_define(const char* define);
	void clear_defines();

	NativeShaderProgramHandleType create(io::input_stream* vert_src, io::input_stream* frag_src);
	void destroy();

	NativeShaderProgramHandleType get_native_handle() const { return _program; }

	/// binds shader to device and sets all common uniforms
	void bind();
	/// unbinds shader from device
	void unbind();

	bool has_uniform(const char* name) const { return find_uniform(name) != nullptr; }

	void set_uniform(const char* name, const float* fv, size_t count);
	void set_uniform(const char* name, const int* iv, size_t count);

	void set_uniform(const char* name, float v)                                {                             set_uniform(name, &v, 1); }
	void set_uniform(const char* name, float v1, float v2)                     { float v[2] = {v1,v2};       set_uniform(name, v, 2); }
	void set_uniform(const char* name, float v1, float v2, float v3)           { float v[3] = {v1,v2,v3};    set_uniform(name, v, 3); }
	void set_uniform(const char* name, float v1, float v2, float v3, float v4) { float v[4] = {v1,v2,v3,v4}; set_uniform(name, v, 4); }

	void set_uniform(const char* name, int v)                          {                           set_uniform(name, &v, 1); }
	void set_uniform(const char* name, int v1, int v2)                 { int v[2] = {v1,v2};       set_uniform(name, v, 2); }
	void set_uniform(const char* name, int v1, int v2, int v3)         { int v[3] = {v1,v2,v3};    set_uniform(name, v, 3); }
	void set_uniform(const char* name, int v1, int v2, int v3, int v4) { int v[4] = {v1,v2,v3,v4}; set_uniform(name, v, 4); }

private:
	struct attribute {
		std::string name;
		int location;
		int size;
		uint32_t type;

		attribute() : location(-1), size(0), type(0) {}
		attribute(const char* name, int location, int size, uint32_t type) : name(name), location(location), size(size), type(type) {}
	};

	struct uniform {
		std::string name;
		int location;
		int size;
		uint32_t type;

		uniform() : location(-1), size(0), type(0) {}
		uniform(const char* name, int location, int size, uint32_t type) : name(name), location(location), size(size), type(type) {}
	};

	NativeShaderProgramHandleType _program;

	typedef std::vector<std::string> defines;
	defines _defines;

	typedef std::vector<attribute> attributes;
	attributes _attributes;

	typedef std::vector<uniform> uniforms;
	uniforms _uniforms;

	void fill_attributes();
	void fill_uniforms();

	const attribute* find_attribute(const char* name) const;
	const uniform* find_uniform(const char* name) const;

private:
	DISALLOW_COPY_AND_ASSIGN(shader_program)
};

typedef boost::intrusive_ptr<shader_program> shader_program_ptr;

typedef GLuint NativeTextureHandleType;

DEFINE_ENUM_CLASS(
	texture_format, uint32_t,
	l8,
	a8,
	la8,
	rgb8,
	rgba8,
	rgb565,
	rgba5551,
	rgba4444
)

DEFINE_ENUM_CLASS(
	texture_address, uint32_t,
	repeat,
	clamp_to_edge,
	mirrored_repeat
)

DEFINE_ENUM_CLASS(
	texture_filter, uint32_t,
	nearest,
	linear
)

DEFINE_ENUM_CLASS(
	vertex_element_type, uint32_t,
	byte,
	ubyte,
	short_,
	ushort,
	int_,
	uint,
	float_
)

struct vertex_element_description {
	attribute_location location;
	size_t size;
	vertex_element_type type;
	bool normalized;
	size_t stride;
	size_t offset;
};

#define BEGIN_VERTEX_DECLARATION(VertexType) \
static const vertex_element_description* decl() { \
	typedef VertexType ThisType; \
	static vertex_element_description descr[] = {

#define DECLARE_VERTEX_ELEMENT(Location, Size, Type, Normalized, Field) \
		{ Location, Size, Type, Normalized, sizeof(ThisType), offsetof(ThisType, Field) },

#define END_VERTEX_DECLARATION() \
		{ attribute_location::a_position, 0, vertex_element_type::float_, false, 0, 0 } \
	}; \
	return descr; \
}

struct vertex_position_color {
	float x, y, z;
	uint32_t color;

	BEGIN_VERTEX_DECLARATION(vertex_position_color)
		DECLARE_VERTEX_ELEMENT(attribute_location::a_position, 3, vertex_element_type::float_, false, x)
		DECLARE_VERTEX_ELEMENT(attribute_location::a_color, 4, vertex_element_type::ubyte, true, color)
	END_VERTEX_DECLARATION()
};

struct vertex_position_texture {
	float x, y, z;
	float tu, tv;

	BEGIN_VERTEX_DECLARATION(vertex_position_texture)
		DECLARE_VERTEX_ELEMENT(attribute_location::a_position, 3, vertex_element_type::float_, false, x)
		DECLARE_VERTEX_ELEMENT(attribute_location::a_texcoord, 2, vertex_element_type::float_, false, tu)
	END_VERTEX_DECLARATION()
};

struct vertex_position_color_texture {
	float x, y, z;
	uint32_t color;
	float tu, tv;

	BEGIN_VERTEX_DECLARATION(vertex_position_color_texture)
		DECLARE_VERTEX_ELEMENT(attribute_location::a_position, 3, vertex_element_type::float_, false, x)
		DECLARE_VERTEX_ELEMENT(attribute_location::a_color, 4, vertex_element_type::ubyte, true, color)
		DECLARE_VERTEX_ELEMENT(attribute_location::a_texcoord, 2, vertex_element_type::float_, false, tu)
	END_VERTEX_DECLARATION()
};

struct vertex_position_normal_texture {
	float x, y, z;
	float nx, ny, nz;
	float tu, tv;

	BEGIN_VERTEX_DECLARATION(vertex_position_normal_texture)
		DECLARE_VERTEX_ELEMENT(attribute_location::a_position, 3, vertex_element_type::float_, false, x)
		DECLARE_VERTEX_ELEMENT(attribute_location::a_normal, 3, vertex_element_type::float_, false, nx)
		DECLARE_VERTEX_ELEMENT(attribute_location::a_texcoord, 2, vertex_element_type::float_, false, tu)
	END_VERTEX_DECLARATION()
};

struct vertex_position_normal_beta4_texture {
	float x, y, z;
	float nx, ny, nz;
	union {
		float beta[4];
		struct {
			float weights[3];
			uint8_t indices[4];
		};
	};
	float tu, tv;

	BEGIN_VERTEX_DECLARATION(vertex_position_normal_beta4_texture)
		DECLARE_VERTEX_ELEMENT(attribute_location::a_position, 3, vertex_element_type::float_, false, x)
		DECLARE_VERTEX_ELEMENT(attribute_location::a_normal, 3, vertex_element_type::float_, false, nx)
		DECLARE_VERTEX_ELEMENT(attribute_location::a_blendweights, 3, vertex_element_type::float_, false, weights)
		DECLARE_VERTEX_ELEMENT(attribute_location::a_blendindices, 4, vertex_element_type::ubyte, false, indices)
		DECLARE_VERTEX_ELEMENT(attribute_location::a_texcoord, 2, vertex_element_type::float_, false, tu)
	END_VERTEX_DECLARATION()
};

struct vertex_position_normal_beta5_texture {
	float x, y, z;
	float nx, ny, nz;
	union {
		float beta[5];
		struct {
			float weights[4];
			uint8_t indices[4];
		};
	};
	float tu, tv;

	BEGIN_VERTEX_DECLARATION(vertex_position_normal_beta5_texture)
		DECLARE_VERTEX_ELEMENT(attribute_location::a_position, 3, vertex_element_type::float_, false, x)
		DECLARE_VERTEX_ELEMENT(attribute_location::a_normal, 3, vertex_element_type::float_, false, nx)
		DECLARE_VERTEX_ELEMENT(attribute_location::a_blendweights, 4, vertex_element_type::float_, false, weights)
		DECLARE_VERTEX_ELEMENT(attribute_location::a_blendindices, 4, vertex_element_type::ubyte, false, indices)
		DECLARE_VERTEX_ELEMENT(attribute_location::a_texcoord, 2, vertex_element_type::float_, false, tu)
	END_VERTEX_DECLARATION()
};

typedef GLenum NativeBufferType;
typedef GLuint NativeBufferHandleType;

DEFINE_ENUM_CLASS(
	buffer_type, uint32_t,
	vertex,
	index16
)

DEFINE_ENUM_CLASS(
	usage, uint32_t,
	static_,
	dynamic_,
	stream
)

/// device buffer
class buffer : public ref_counter<buffer> {
public:
	buffer();
	~buffer();

	NativeBufferHandleType create(buffer_type type, const vertex_element_description* decl, size_t size, const void* data, usage usage = usage::static_);
	void destroy();

	NativeBufferType get_native_type() const { return _native_type; }
	NativeBufferHandleType get_native_handle() const { return _buffer; }

	void set_data(size_t size, const void* data, usage usage = usage::static_);
	void set_subdata(size_t offset, size_t size, const void* data);

	buffer_type get_type() const { return _type; }
	const vertex_element_description* get_decl() const { return _decl; }

private:
	buffer_type _type;
	const vertex_element_description* _decl;
	NativeBufferType _native_type;
	NativeBufferHandleType _buffer;

private:
	DISALLOW_COPY_AND_ASSIGN(buffer)
};

typedef boost::intrusive_ptr<buffer> buffer_ptr;

/// texture
class texture : public ref_counter<texture> {
public:
	texture();
	virtual ~texture();

	void destroy();

	NativeTextureHandleType get_native_handle() const { return _texture; }

protected:
	NativeTextureHandleType _texture;
	
private:
	DISALLOW_COPY_AND_ASSIGN(texture)
};

typedef boost::intrusive_ptr<texture> texture_ptr;

/// texture2d
class texture2d : public texture {
public:
	texture2d();

	NativeTextureHandleType create(texture_address address = texture_address::clamp_to_edge, texture_filter filter = texture_filter::linear);
	NativeTextureHandleType create(size_t width, size_t height, texture_format format = texture_format::rgba8, texture_address address = texture_address::clamp_to_edge, texture_filter filter = texture_filter::linear);
	NativeTextureHandleType create(imaging::image* image, texture_address address = texture_address::clamp_to_edge, texture_filter filter = texture_filter::linear);

	void set_image(imaging::image* image);
	void set_subimage(imaging::image* image, size_t xoffset, size_t yoffset);
	
	size_t get_width() const { return _width; }
	size_t get_height() const { return _height; }

private:
	size_t _width;
	size_t _height;
};

typedef boost::intrusive_ptr<texture2d> texture2d_ptr;

DEFINE_ENUM_CLASS(
	blend_mode, uint32_t,
	none,
	modulate,
	add,
	multiply,
	replace
)

DEFINE_ENUM_CLASS(
	cull_mode, uint32_t,
	none,
	front,
	back,
	front_and_back
)

DEFINE_ENUM_CLASS(
	sprite_sort, uint32_t,
	immediate,
	deferred,
	sort_by_texture,
	sort_by_blend,
	sort_back_to_front,
	sort_front_to_back
)

class sprite_batch : public ref_counter<sprite_batch> {
public:
	sprite_batch();

	void begin(sprite_sort mode = sprite_sort::deferred);
	void draw(texture2d* tex, blend_mode blend_mode, const rectf& src, const rectf& dest, uint32_t color = -1, float depth = 0.0f);
	void draw_rect(blend_mode blend_mode, const rectf& dest, uint32_t color = -1, float depth = 0.0f);
	void end();

private:
	enum {
		max_batch_size = 4096,
		min_batch_size = 64,
		initial_queue_size = 32,
		vertices_per_sprite = 4,
		indices_per_sprite = 6
	};

	typedef vertex_position_color_texture vertex_type;

	struct sprite_info {
		vertex_type quad[4];
		texture2d* tex;
		blend_mode blend_mode;
	};

	std::vector<sprite_info> _sprites;
	std::vector<const sprite_info*> _sorted_sprites;
	size_t _sprites_count;
	bool _in_begin_end_pair;
	sprite_sort _sort;

	class shared : public shared_object {
	public:
		buffer ib, vb;
		std::vector<vertex_type> vertices;
		size_t vertices_position;
		bool _in_immediate_mode;

		shared();
	
	private:
		DISALLOW_COPY_AND_ASSIGN(shared)
	};

	typedef boost::intrusive_ptr<class shared> shared_ptr;

	shared_ptr _shared;

private:
	void flush();
	void sort_sprites();
	void render_batch(texture* tex, blend_mode blend_mode, const sprite_info* const* sprites, size_t count);

private:
	DISALLOW_COPY_AND_ASSIGN(sprite_batch)
};

typedef boost::intrusive_ptr<sprite_batch> sprite_batch_ptr;

typedef GLuint NativeFramebufferObjectType;
typedef GLuint NativeRenderbufferObjectType;

// render to texture
class render_to_texture : public ref_counter<render_to_texture> {
public:
	render_to_texture() : _framebuffer(), _depth_renderbuffer() {}
	~render_to_texture();

	NativeFramebufferObjectType create(size_t width, size_t height, texture_format format = texture_format::rgba8, bool create_depth_buffer = false);
	void destroy();

	size_t get_width() const { return _texture ? _texture->get_width() : 0; }
	size_t get_height() const { return _texture ? _texture->get_height() : 0; }

	void begin();
	void end();

	texture2d* get_texture() const { return _texture.get(); }

private:
	texture2d_ptr _texture;
	NativeFramebufferObjectType _framebuffer;
	NativeRenderbufferObjectType _depth_renderbuffer;

	DISALLOW_COPY_AND_ASSIGN(render_to_texture)
};

typedef boost::intrusive_ptr<render_to_texture> render_to_texture_ptr;

DEFINE_ENUM_CLASS(
	matrix, uint32_t,
	model,
	view,
	projection
)

DEFINE_ENUM_CLASS(
	primitives, uint32_t,
	points,
	lines,
	line_loop,
	line_strip,
	triangles,
	triangle_strip,
	triangle_fan
)

DEFINE_ENUM_CLASS_FLAGS(
	clear, uint32_t,
	color   = 1 << 0,
	depth   = 1 << 1,
	stencil = 1 << 2,
	all = color | depth | stencil
)

/// stateful renderer
class renderer : public shared_object {
public:
	static renderer& get();

	bool has_extension(const std::string& extension) const {
		return std::binary_search(_extensions.begin(), _extensions.end(), extension);
	}

	void enable_texturing(bool enable);
	bool is_texturing_enabled() const { return _enable_texturing; }

	void enable_depth_test(bool enable);
	bool is_depth_test_enabled() const { return _enable_depth_test; }

	void enable_depth_write(bool enable);
	bool is_depth_write_enabled() const { return _enable_depth_write; }

	void enable_color_write(bool red, bool green, bool blue, bool alpha);

	void enable_stencil_test(bool enable);
	bool is_stencil_test_enabled() const { return _enable_stencil_test; }

	void enable_scissor_test(bool enable);
	bool is_scissor_enabled() const { return _enable_scissor_test; }

	void set_blend_mode(blend_mode blend_mode);
	blend_mode get_blend_mode() const { return _blend_mode; }

	void set_cull_mode(cull_mode cull_mode);
	cull_mode get_cull_mode() const { return _cull_mode; }

	void matrix_push(matrix type);
	void matrix_pop(matrix type);
	void matrix_set(matrix type, const glm::mat4& m);
	void matrix_mult(matrix type, const glm::mat4& m);
	void matrix_mult_local(matrix type, const glm::mat4& m);	
	void matrix_translate(matrix type, const glm::vec3& d);
	void matrix_translate(matrix type, float dx, float dy, float dz);
	void matrix_scale(matrix type, const glm::vec3& s);
	void matrix_scale(matrix type, float sx, float sy, float sz);
	void matrix_scale(matrix type, float s);

	const glm::mat4& get_current_matrix(matrix type) const;

	void push_state();
	void pop_state();

	void set_viewport(int x, int y, int width, int height);
	void set_viewport(const rect& r) { set_viewport(r.x, r.y, r.width, r.height); }
	void set_scissor_rect(int x, int y, int width, int height);
	void set_scissor_rect(const rect& r) { set_scissor_rect(r.x, r.y, r.width, r.height); }

	void set_geometry(const buffer* vertices, const buffer* indices); // -1 to keep old state
	void set_texture(const texture* tex, uint32_t stage = 0);
	void set_shader_program(const shader_program* program);

	void clear_color_value(const color_value& c);
	void clear_depth_value(float d);
	void clear_stencil_value(int s);
	void clear(clear mode);

	void draw(primitives mode, size_t elements_count, size_t offset);
	void draw_elements(primitives mode, size_t elements_count, size_t offset);

private:
	renderer();

	static void init_extensions();

	static void bind_vertex_attributes(const vertex_element_description* decl);
	static void unbind_vertex_attributes(const vertex_element_description* decl);
	static void unbind_all_vertex_attributes();

private:
	std::vector<std::string> _extensions;

	bool _enable_texturing;
	bool _enable_depth_test;
	bool _enable_depth_write;
	bool _enable_stencil_test;
	bool _enable_scissor_test;

	blend_mode _blend_mode;
	cull_mode _cull_mode;

	std::vector<glm::mat4> _matrix_stack[enum_traits<matrix>::num_items];

	struct state {
		GLint viewport[4];
		GLint framebuffer;
		GLboolean color_writemask[4];
		GLboolean depth_writemask;
		GLboolean depth_test;
		GLfloat color_clear_value[4];
		GLfloat depth_clear_value;
	};

	std::vector<state> _state_stack;

private:
	DISALLOW_COPY_AND_ASSIGN(renderer)
};

typedef boost::intrusive_ptr<renderer> renderer_ptr;

// GL extensions

extern PFNGLMAPBUFFEROESPROC glMapBufferOES;
extern PFNGLUNMAPBUFFEROESPROC glUnmapBufferOES;
extern PFNGLGETBUFFERPOINTERVOESPROC glGetBufferPointervOES;

extern PFNGLMAPBUFFERRANGEEXTPROC glMapBufferRangeEXT;
extern PFNGLFLUSHMAPPEDBUFFERRANGEEXTPROC glFlushMappedBufferRangeEXT;

} // namespace render
