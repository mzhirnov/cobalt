#pragma once

// Classes in this file:
//     material
//     bounding_box
//     bounding_sphere
//     bone
//     mesh
//     skinned_mesh
//     animation_channel
//     animation
//     model
//     animation_player
//
// Global functions:
//     decompose_matrix()
//     compose_matrix()

#include "cool/common.hpp"
#include "cool/render.hpp"
#include "cool/geometry.hpp"
#include "cool/io.hpp"
#include "cool/enum_traits.hpp"

#include <glm/glm.hpp>

#include <pugixml.hpp>

namespace scenegraph {

void decompose_matrix(const glm::mat4& m, glm::vec3& scaling, glm::quat& rotation, glm::vec3& translation);
glm::mat4 compose_matrix(const glm::vec3& scaling, const glm::quat& rotation, const glm::vec3& translation);

// material
class material : public ref_counter<material> {
public:
	material() : _power(50.0f) {}

	const render::color_value& get_duffuse_color() const { return _diffuse; }
	const render::color_value& get_specular_color() const { return _specular; }
	const render::color_value& get_emissive_color() const { return _emissive; }

	void set_duffuse_color(const render::color_value& value) { _diffuse = value; }
	void set_specular_color(const render::color_value& value) { _specular = value; }
	void set_emissive_color(const render::color_value& value) { _emissive = value; }

	float get_specular_power() const { return _power; }
	void set_specular_power(float power) { _power = power; }
	
	const render::texture* get_texture() const { return _texture.get(); }
	void set_texture(render::texture* texture) { _texture = texture; }

private:
	render::color_value _diffuse;
	render::color_value _specular;
	render::color_value _emissive;

	float _power;

	render::texture_ptr _texture;
};

typedef boost::intrusive_ptr<material> material_ptr;

// bounding box
class bounding_box {
public:
	bounding_box();

	void add_internal_point(const glm::vec3& p);

	const glm::vec3& get_min_point() const { return _min; }
	const glm::vec3& get_max_point() const { return _max; }

private:
	glm::vec3 _min;
	glm::vec3 _max;
};

// bounding sphere
class bounding_sphere {
public:
	bounding_sphere() : _radius() {}
	bounding_sphere(const glm::vec3& center, float radius) : _center(center), _radius(radius) {}

	const glm::vec3& get_center() const { return _center; }
	float get_radius() const { return _radius; }

	static bounding_sphere from_bounding_box(const bounding_box& bbox);

private:
	glm::vec3 _center;
	float _radius;
};

typedef boost::intrusive_ptr<class bone> bone_ptr;

// bone
class bone : public ref_counter<bone> {
public:
	bone() : _parent(), _index() {}
	explicit bone(bone* parent) : _parent(parent), _index() {}

	const bone* get_parent() const { return _parent; }

	void add_child(bone* child);

	uint32_t get_index() const { return _index; }
	void set_index(uint32_t index) { _index = index; }

	const glm::mat4& get_transform() const { return _transform; }
	void set_transform(const glm::mat4& m) { _transform = m; }

	glm::mat4 compute_absolute_transform() const;

private:
	bone* _parent;
	std::vector<bone*> _children;
	uint32_t _index;
	glm::mat4 _transform;

private:
	DISALLOW_COPY_AND_ASSIGN(bone)
};

DEFINE_ENUM_CLASS(
	mesh_type, uint32_t,
	regular,
	skinned
)

// mesh
class mesh : public ref_counter<mesh> {
public:
	mesh();

	virtual ~mesh() {}

	mesh_type get_type() const { return _type; }

	const bone* get_bone() const { return _bone; }
	void set_bone(bone* b) { _bone = b; }

	const material* get_material() const { return _material; }
	void set_material(material* mat) { _material = mat; }

	void set_vertex_data(const render::vertex_element_description* decl, const void* data, size_t size);
	void set_index_data(const void* data, size_t size);

	void add_mesh_part(size_t count, size_t offset) { _mesh_parts.emplace_back(count, offset); }

	void draw();

protected:
	mesh(mesh_type type);

	virtual void pre_draw();
	virtual void post_draw();

private:
	void init();
	void compute_bounding_sphere(const void* data, size_t size, size_t stride);

protected:
	mesh_type _type;
	bone* _bone;
	material* _material;
	bounding_sphere _bsphere;
	render::buffer _vertices;
	render::buffer _indices;
	render::shader_program _shader;

	typedef std::vector<std::pair<size_t, size_t>> MeshParts;
	MeshParts _mesh_parts;

private:
	DISALLOW_COPY_AND_ASSIGN(mesh)
};

typedef boost::intrusive_ptr<mesh> mesh_ptr;

// skinned mesh
class skinned_mesh : public mesh {
public:
	skinned_mesh() : mesh(mesh_type::skinned) {}

	void reserve_bone_influences(size_t size) { _bone_influences.reserve(size); }
	void add_bone_influence(bone* b, const glm::mat4& m) { _bone_influences.emplace_back(b, m); }

	size_t get_influent_bone_count() const { return _bone_influences.size(); }
	const bone* get_influent_bone(size_t index) const { return _bone_influences[index].first; }
	const glm::mat4& get_influent_bone_offset(size_t index) const { return _bone_influences[index].second; }

protected:
	void pre_draw();

private:
	typedef std::vector<std::pair<bone*, glm::mat4>> BoneInfluences;
	BoneInfluences _bone_influences;

	typedef std::vector<glm::mat4> Matrices;
	mutable Matrices _matrices;
};

typedef boost::intrusive_ptr<skinned_mesh> skinned_mesh_ptr;

// animation channel
class animation_channel : public ref_counter<animation_channel> {
public:
	animation_channel() : _bone() {}

	const bone* get_bone() const { return _bone; }
	void set_bone(bone* b) { _bone = b; }

	void get_srt(float time, glm::vec3& scaling, glm::quat& rotation, glm::vec3& translation) const;

private:
	friend class model;

	void reserve_scalings(size_t size) { _scaling_keys.reserve(size); }
	void reserve_rotations(size_t size) { _rotation_keys.reserve(size); }
	void reserve_translations(size_t size) { _translation_keys.reserve(size); }

	void add_scaling(float time, const glm::vec3& scaling) { _scaling_keys.emplace_back(time, scaling); }
	void add_rotation(float time, const glm::quat& rotation) { _rotation_keys.emplace_back(time, rotation); }
	void add_translation(float time, const glm::vec3& translation) { _translation_keys.emplace_back(time, translation); }

private:
	typedef std::vector<std::pair<float, glm::vec3>> VectorKeys;
	typedef std::vector<std::pair<float, glm::quat>> QuaternionKeys;
	//typedef std::vector<std::pair<float, glm::mat4>> MatrixKeys;

	VectorKeys _scaling_keys;
	QuaternionKeys _rotation_keys;
	VectorKeys _translation_keys;

	bone* _bone;

private:
	DISALLOW_COPY_AND_ASSIGN(animation_channel)
};

typedef boost::intrusive_ptr<animation_channel> animation_channel_ptr;

// animation
class animation : public ref_counter<animation> {
public:
	animation() : _period(), _fps() {}

	float get_period() const { return _period; }
	void set_period(float period) { _period = period; }
	float get_periodic_position(float time) const;

	float get_fps() const { return _fps; }
	void set_fps(float fps) { _fps = fps; }

	size_t get_channel_count() const { return _channels.size(); }
	animation_channel* get_channel(size_t index) const;
	void add_channel(animation_channel* channel) { _channels.push_back(channel); }

private:
	float _period;
	float _fps;

	typedef std::vector<animation_channel_ptr> Channels;
	Channels _channels;

private:
	DISALLOW_COPY_AND_ASSIGN(animation)
};

typedef boost::intrusive_ptr<animation> animation_ptr;

// model
class model : public ref_counter<model> {
public:
	model() {}

	void load(io::input_stream* stream);

	size_t get_mesh_count() const { return _meshes.size(); }
	size_t get_material_count() const { return _materials.size(); }
	size_t get_bone_count() const { return _bones.size(); }
	size_t get_animation_count() const { return _animations.size(); }

	mesh* get_mesh(size_t index) const { return boost::get_pointer(_meshes[index].second); }
	material* get_material(size_t index) const { return boost::get_pointer(_materials[index].second); }
	bone* get_bone(size_t index) const { return boost::get_pointer(_bones[index].second); }
	animation* get_animation(size_t index) const { return boost::get_pointer(_animations[index].second); }
	const std::string& get_animation_name(size_t index) const { return _animations[index].first; }

	mesh* find_mesh(const char* name) const;
	material* find_material(const char* name) const;
	bone* find_bone(const char* name) const;
	animation* find_animation(const char* name) const;

	void draw();

private:
	typedef std::vector<std::pair<std::string, bone*>> MeshRefs;

	void load_materials(pugi::xml_node elem);
	void load_bones(pugi::xml_node elem, MeshRefs& mesh_refs);
	void load_meshes(pugi::xml_node elem, const MeshRefs& mesh_refs);
	void load_animations(pugi::xml_node elem);

	void load_material(pugi::xml_node elem);
	void load_bone(pugi::xml_node elem, bone* parent, MeshRefs& mesh_refs);
	void load_mesh(pugi::xml_node elem);
	void load_animation(pugi::xml_node elem);

	static animation_channel_ptr load_animation_channel(pugi::xml_node elem);
	static render::color_value load_color_value(pugi::xml_node elem);
	static glm::mat4 load_matrix(pugi::xml_node elem);
	static glm::vec3 load_vec3(pugi::xml_node elem);
	static glm::quat load_quat(pugi::xml_node elem);

private:
	typedef std::vector<std::pair<std::string, mesh_ptr>> Meshes;
	typedef std::vector<std::pair<std::string, material_ptr>> Materials;
	typedef std::vector<std::pair<std::string, bone_ptr>> Bones;
	typedef std::vector<std::pair<std::string, animation_ptr>> Animations;

	Meshes _meshes;
	Materials _materials;
	Bones _bones;
	Animations _animations;

private:
	DISALLOW_COPY_AND_ASSIGN(model)
};

typedef boost::intrusive_ptr<model> model_ptr;

// animation player
class animation_player : public ref_counter<animation_player> {
public:
	explicit animation_player(model* m);

	void play(const char* name);
	void pause(const char* name);
	void stop(const char* name);

	void update(float dt);

private:
	model* _model;

	class animation_info {
	public:
		explicit animation_info(animation* anim);

		animation* get_animation() const { return _anim; }
		bool is_paused() const { return _paused; }
		void set_paused(bool paused) { _paused = paused; }		
		float get_time() const { return _time; }
		float get_speed() const { return _speed; }
		void set_speed(float speed) { _speed = speed; }
		float get_blend() const { return _blend; }
		void set_blend(float blend) { _blend = blend; }

		void advance_time(float dt);

	private:
		animation* _anim;
		bool _paused;
		float _time;
		float _speed;
		float _blend;
	};

	typedef std::map<std::string, animation_info> Animations;
	
	Animations _animations;
};

typedef boost::intrusive_ptr<animation_player> animation_player_ptr;

} // namespace scenegraph