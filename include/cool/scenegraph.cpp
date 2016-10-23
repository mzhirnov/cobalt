#include "pch.h"
#include "cool/scenegraph.hpp"
#include "cool/common.hpp"
#include "cool/render.hpp"
#include "cool/logger.hpp"

#include <boost/assert.hpp>
#include <boost/exception/all.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <pugixml.hpp>

namespace glm {

inline vec3 lerp(const vec3& v1, const vec3& v2, float s) {
	return v1 + (v2 - v1) * s;
}

} // glm

namespace scenegraph {

void decompose_matrix(const glm::mat4& m, glm::vec3& scaling, glm::quat& rotation, glm::vec3& translation) {
	// extract translation
	translation = glm::vec3(m[3]);

	glm::vec3 col1(m[0]);
	glm::vec3 col2(m[1]);
	glm::vec3 col3(m[2]);

	// extract scaling
	scaling.x = glm::length(col1);
	scaling.y = glm::length(col2);
	scaling.z = glm::length(col3);

	// handle negative scaling
	if (glm::determinant(m) < 0) {
		scaling = -scaling;
	}

	// remove scaling
	if (scaling.x != 0) { col1 /= scaling.x; }
	if (scaling.y != 0) { col2 /= scaling.y; }
	if (scaling.z != 0) { col3 /= scaling.z; }

	// extract rotation
	rotation = glm::quat_cast(glm::mat3(col1, col2, col3));
}

glm::mat4 compose_matrix(const glm::vec3& scaling, const glm::quat& rotation, const glm::vec3& translation) {
	glm::mat4 m;
	m = glm::scale(m, scaling);
	m *= glm::mat4_cast(rotation);
	m = glm::translate(m, translation);
	return m;
}

// bounding_box

bounding_box::bounding_box()
	: _min(std::numeric_limits<float>::max())
	, _max(std::numeric_limits<float>::min())
{
}

void bounding_box::add_internal_point(const glm::vec3& p) {
	_min.x = std::min(_min.x, p.x);
	_min.y = std::min(_min.y, p.y);
	_min.z = std::min(_min.z, p.z);
		
	_max.x = std::max(_max.x, p.x);
	_max.y = std::max(_max.y, p.y);
	_max.z = std::max(_max.z, p.z);
}

// bounding_sphere

bounding_sphere bounding_sphere::from_bounding_box(const bounding_box& bbox) {
	const glm::vec3& min = bbox.get_min_point();
	const glm::vec3& max = bbox.get_max_point();

	glm::vec3 center = (max + min) * 0.5f;
	float radius = glm::length(max - center);

	return bounding_sphere(center, radius);
}

// bone

void bone::add_child(bone* child) {
	BOOST_ASSERT(!!child);
	_children.push_back(child);
}

glm::mat4 bone::compute_absolute_transform() const {
	if (_parent) {
		return _parent->compute_absolute_transform() * _transform;
	}

	return _transform;
}

// mesh

mesh::mesh()
	: _type(mesh_type::regular)
	, _bone()
	, _material()
{
	init();
}

mesh::mesh(mesh_type type)
	: _type(type)
	, _bone()
	, _material()
{
	init();
}

void mesh::init() {
	io::input_stream_ptr vert_src;
	io::input_stream_ptr frag_src;

	// TODO: smarter resource management needed
	if (_type == mesh_type::regular) {
		vert_src = io::stream::from_asset("regular_mesh.vsh");
		frag_src = io::stream::from_asset("regular_mesh.fsh");		
	} else if (_type == mesh_type::skinned) {
		vert_src = io::stream::from_asset("skinned_mesh.vsh");
		frag_src = io::stream::from_asset("skinned_mesh.fsh");		
	} else {
		BOOST_ASSERT_MSG(false, "unknown mesh type");
	}

	_shader.create(vert_src.get(), frag_src.get());
}

void mesh::set_vertex_data(const render::vertex_element_description* decl, const void* data, size_t size) {
	_vertices.create(render::buffer_type::vertex, decl, size, data);	
	compute_bounding_sphere(data, size, decl->stride);
}

void mesh::compute_bounding_sphere(const void* data, size_t size, size_t stride) {
	bounding_box bbox;

	for (size_t i = 0; i < size; i += stride) {
		const glm::vec3* pos = reinterpret_cast<const glm::vec3*>(reinterpret_cast<const uint8_t*>(data) + i);
		bbox.add_internal_point(*pos);
	}

	_bsphere = bounding_sphere::from_bounding_box(bbox);
}

void mesh::set_index_data(const void* data, size_t size) {
	_indices.create(render::buffer_type::index16, nullptr, size, data);
}

void mesh::draw() {
	pre_draw();

	render::renderer& r = render::renderer::get();

	r.set_geometry(&_vertices, &_indices);
	r.set_texture(_material->get_texture());
	
	for (MeshParts::const_iterator it = _mesh_parts.begin(); it != _mesh_parts.end(); ++it) {
		r.draw_elements(render::primitives::triangles, (*it).first, (*it).second);
	}
	
	r.set_geometry(0, 0);
	r.set_texture(0);

	post_draw();
}

void mesh::pre_draw() {
	_shader.bind();
}

void mesh::post_draw() {
	_shader.unbind();
}

// skinned mesh

void skinned_mesh::pre_draw() {
	const glm::mat4 absolute_transform = get_bone()->compute_absolute_transform();
	const size_t count = get_influent_bone_count();

	_matrices.clear();
	_matrices.reserve(count);

	for (size_t i = 0; i < count; ++i) {
		_matrices.push_back(absolute_transform * get_influent_bone_offset(i) * get_influent_bone(i)->compute_absolute_transform());
	}

	// shader binds here
	mesh::pre_draw();

	_shader.set_uniform("u_matrix_palette[0]", glm::value_ptr(_matrices.front()), count * 16);
}

// animation_channel

namespace {

template<typename T, typename Collection>
T find_frame_in_collection(const Collection& coll, float time) {
	BOOST_ASSERT(!coll.empty());

	if (coll.size() == 1 || time >= coll.back().first) {
		return coll.back().second;
	}

	if (time <= coll.front().first) {
		return coll.front().second;
	}

	// assume that collection is sorted
	typename Collection::const_iterator it = std::lower_bound(std::begin(coll), std::end(coll), time,
		[](const typename Collection::value_type& val, float time) {
			return val.first < time;
		});

	if (it == coll.end()) {
		return coll.back().second;
	}

	if (it == coll.begin() || almost_equal_floats(time, (*it).first)) {
		return (*it).second;
	}

	// move to the previous frame
	typename Collection::const_iterator it2 = it--;

	// linear interpolate between two frames
	float s = (time - (*it).first) / ((*it2).first - (*it).first);
	return glm::lerp((*it).second, (*it2).second, s);
}

} // namespace

void animation_channel::get_srt(float time, glm::vec3& scaling, glm::quat& rotation, glm::vec3& translation) const {
	scaling = find_frame_in_collection<glm::vec3>(_scaling_keys, time);
	rotation = find_frame_in_collection<glm::quat>(_rotation_keys, time);
	translation = find_frame_in_collection<glm::vec3>(_translation_keys, time);
}

// animation

float animation::get_periodic_position(float time) const {
	return std::fmod(time, _period);
}

animation_channel* animation::get_channel(size_t index) const {
	BOOST_ASSERT(index < _channels.size());

	if (index < _channels.size()) {
		return boost::get_pointer(_channels[index]);
	}

	return nullptr;
}

// model

void model::load(io::input_stream* stream) {
	io::input_stream_ptr guard(stream);

	std::vector<uint8_t> buffer;
	io::stream::read_all(stream, buffer);

	guard.reset(); // don't need it any longer

	if (buffer.empty()) {
		LOGE("model xml file is empty");
		return;
	}

	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_buffer_inplace(&buffer.front(), buffer.size());
	if (!result) {
		LOGE("failed to parse model xml: ") << result.description();
		return;
	}

	MeshRefs mesh_refs;

	load_materials(doc.child("model").child("materials"));
	load_bones(doc.child("model"), mesh_refs);
	load_meshes(doc.child("model").child("meshes"), mesh_refs);
	load_animations(doc.child("model").child("animations"));
}

void model::load_materials(pugi::xml_node elem) {
	size_t materials_count = elem.attribute("count").as_uint();
	
	_materials.clear();
	_materials.reserve(materials_count);

	size_t material_num = 0;
	for (pugi::xml_node material_xml = elem.child("material"); material_xml; material_xml = material_xml.next_sibling("material"), ++material_num) {
		load_material(material_xml);
	}
	BOOST_ASSERT(material_num == materials_count);

	std::sort(_materials.begin(), _materials.end(),
		[](const Materials::value_type& m1, const Materials::value_type& m2) {
			return m1.first < m2.first;
		});
}

void model::load_material(pugi::xml_node elem) {
	material_ptr m = new material();

	m->set_duffuse_color(load_color_value(elem.child("diffuse")));
	m->set_specular_color(load_color_value(elem.child("specular")));
	m->set_emissive_color(load_color_value(elem.child("emissive")));
	m->set_specular_power(elem.child("specular").attribute("power").as_float());

	// TODO: manage resources cleverer
	render::texture2d_ptr tex = new render::texture2d();
	const char* tex_name = elem.child("texture").attribute("name").as_string();
	tex->create(imaging::image::from_stream(io::stream::from_asset(tex_name)), render::texture_address::repeat);
	m->set_texture(tex.get());

	std::string name = elem.attribute("id").as_string();
	_materials.emplace_back(name, m);
}

void model::load_bones(pugi::xml_node elem, MeshRefs& mesh_refs) {
	_bones.clear();

	for (pugi::xml_node bone_xml = elem.child("node"); bone_xml; bone_xml = bone_xml.next_sibling("node")) {
		load_bone(bone_xml, nullptr, mesh_refs);
	}

	// in sorted collection we will be able to find duplicated values
	std::sort(mesh_refs.begin(), mesh_refs.end(),
		[](const MeshRefs::value_type& r1, const MeshRefs::value_type& r2) {
			return r1.first < r2.first;
		});

	std::sort(_bones.begin(), _bones.end(),
		[](const Bones::value_type& b1, const Bones::value_type& b2) {
			return b1.first < b2.first;
		});

	// right after sorting array we can assign indices to the bones
	for (size_t i = 0; i < _bones.size(); ++i) {
		_bones[i].second->set_index(i);
	}
}

void model::load_bone(pugi::xml_node elem, bone* parent, MeshRefs& mesh_refs) {
	bone_ptr b = new bone(parent);
	if (parent) {
		parent->add_child(b.get());
	}

	b->set_transform(load_matrix(elem.child("matrix")));
	_bones.emplace_back(elem.attribute("name").as_string(), b);

	// collect reference to mesh if there is
	pugi::xml_node mesh = elem.child("mesh");
	if (mesh) {
		mesh_refs.emplace_back(mesh.attribute("ref").as_string(), b.get());
	}

	// load children recursively
	for (pugi::xml_node child_xml = elem.child("node"); child_xml; child_xml = child_xml.next_sibling("node")) {
		load_bone(child_xml, b.get(), mesh_refs);
	}
}

void model::load_meshes(pugi::xml_node elem, const MeshRefs& mesh_refs) {
	size_t mesh_count = elem.attribute("count").as_uint();

	_meshes.clear();
	_meshes.reserve(mesh_count);

	size_t mesh_num = 0;
	for (pugi::xml_node mesh_xml = elem.child("mesh"); mesh_xml; mesh_xml = mesh_xml.next_sibling("mesh"), ++mesh_num) {
		load_mesh(mesh_xml);
	}
	BOOST_ASSERT(mesh_num == mesh_count);
	
	std::sort(_meshes.begin(), _meshes.end(),
		[](const Meshes::value_type& m1, const Meshes::value_type& m2) {
			return m1.first < m2.first;
		});

	// link meshes with corresponding bones
	std::string last_mesh;
	for (MeshRefs::const_iterator it = mesh_refs.begin(); it != mesh_refs.end(); ++it) {
		const std::string& mesh_name = (*it).first;
		bone* mesh_bone = (*it).second;
		if (mesh_name == last_mesh) {
			BOOST_ASSERT_MSG(false, "there is duplicating mesh, cloning needed");
		} else {
			mesh* m = find_mesh(mesh_name.c_str());
			BOOST_ASSERT(!!m);
			if (m) {
				m->set_bone(mesh_bone);
			}
		}

		last_mesh = (*it).first;
	}
}

void model::load_mesh(pugi::xml_node elem) {
	mesh_ptr m;

	pugi::xml_node vertices_xml = elem.child("vertices");
	pugi::xml_node bones_xml = elem.child("bones");

	uint32_t vertices_count = vertices_xml.attribute("count").as_uint();

	if (bones_xml) {
		typedef std::vector<std::pair<uint32_t, float>> Weights;
		typedef std::vector<Weights> VertexWeights;

		skinned_mesh_ptr sm = new skinned_mesh();
		VertexWeights all_vertex_weights;

		size_t bones_count = bones_xml.attribute("count").as_uint();

		sm->reserve_bone_influences(bones_count);
		all_vertex_weights.reserve(bones_count);

		size_t bone_num = 0;
		for (pugi::xml_node bone_xml = bones_xml.child("bone"); bone_xml; bone_xml = bone_xml.next_sibling("bone"), ++bone_num) {
			sm->add_bone_influence(find_bone(bone_xml.attribute("node").as_string()), load_matrix(bone_xml.child("matrix")));
			
			all_vertex_weights.emplace_back();
			Weights& vertex_weights = all_vertex_weights.back();

			vertex_weights.reserve(bone_xml.attribute("weights").as_uint());
			for (pugi::xml_node weight = bone_xml.child("weight"); weight; weight = weight.next_sibling("weight")) {
				vertex_weights.emplace_back(weight.attribute("vertex").as_uint(), weight.attribute("weight").as_float());
			}
		}
		BOOST_ASSERT(bone_num == bones_count);

		VertexWeights vertex_weights(vertices_count);
		for (uint32_t i = 0; i < all_vertex_weights.size(); ++i) {
			const Weights& weights = all_vertex_weights[i];
			for (Weights::const_iterator it = weights.begin(); it != weights.end(); ++it) {
				vertex_weights[(*it).first].emplace_back(i, (*it).second);
			}
		}

		// clean up
		VertexWeights().swap(all_vertex_weights);

		std::vector<render::vertex_position_normal_beta5_texture> vertices;	
		vertices.reserve(vertices_count);

		uint32_t vertex_num = 0;
		for (pugi::xml_node vertex_xml = vertices_xml.child("vertex"); vertex_xml; vertex_xml = vertex_xml.next_sibling("vertex"), ++vertex_num) {
			vertices.emplace_back();
			render::vertex_position_normal_beta5_texture& v = vertices.back();
			v.x = vertex_xml.attribute("x").as_float();
			v.y = vertex_xml.attribute("y").as_float();
			v.z = vertex_xml.attribute("z").as_float();
			v.nx = vertex_xml.attribute("nx").as_float();
			v.ny = vertex_xml.attribute("ny").as_float();
			v.nz = vertex_xml.attribute("nz").as_float();
			v.tu = vertex_xml.attribute("tu").as_float();
			v.tv = vertex_xml.attribute("tv").as_float();
			v.beta[0] = 0.0f;
			v.beta[1] = 0.0f;
			v.beta[2] = 0.0f;
			v.beta[3] = 0.0f;
			v.beta[4] = 0.0f;

			for (uint32_t i = 0; i < std::min(4u, vertex_weights[vertex_num].size()); ++i) {
				v.indices[i] = static_cast<uint8_t>(vertex_weights[vertex_num][i].first);
				v.weights[i] = vertex_weights[vertex_num][i].second;
			}
		}
		BOOST_ASSERT(vertex_num == vertices_count);

		m = sm;
		m->set_vertex_data(render::vertex_position_normal_beta5_texture::decl(), &vertices.front(), vertices.size());
	} else {
		std::vector<render::vertex_position_normal_texture> vertices;	
		vertices.reserve(vertices_count);

		uint32_t vertex_num = 0;
		for (pugi::xml_node vertex_xml = vertices_xml.child("vertex"); vertex_xml; vertex_xml = vertex_xml.next_sibling("vertex"), ++vertex_num) {
			vertices.emplace_back();
			render::vertex_position_normal_texture& v = vertices.back();
			v.x = vertex_xml.attribute("x").as_float();
			v.y = vertex_xml.attribute("y").as_float();
			v.z = vertex_xml.attribute("z").as_float();
			v.nx = vertex_xml.attribute("nx").as_float();
			v.ny = vertex_xml.attribute("ny").as_float();
			v.nz = vertex_xml.attribute("nz").as_float();
			v.tu = vertex_xml.attribute("tu").as_float();
			v.tv = vertex_xml.attribute("tv").as_float();
		}
		BOOST_ASSERT(vertex_num == vertices_count);

		m = new mesh();
		m->set_vertex_data(render::vertex_position_normal_texture::decl(), &vertices.front(), vertices.size());
	}

	pugi::xml_node faces_xml = elem.child("faces");

	size_t faces_count = faces_xml.attribute("count").as_uint();

	std::vector<uint16_t> indices;
	indices.reserve(faces_count);
	
	size_t face_num = 0;
	for (pugi::xml_node face_xml = faces_xml.child("face"); face_xml; face_xml = face_xml.next_sibling("face"), ++face_num) {
		indices.push_back(static_cast<uint16_t>(face_xml.attribute("a").as_uint()));
		indices.push_back(static_cast<uint16_t>(face_xml.attribute("b").as_uint()));
		indices.push_back(static_cast<uint16_t>(face_xml.attribute("c").as_uint()));
	}
	BOOST_ASSERT(face_num == faces_count);

	m->set_index_data(&indices.front(), indices.size());
	
	// TODO: divide mesh into several parts according to hardware capabilities
	m->add_mesh_part(indices.size(), 0);
	m->set_material(find_material(elem.attribute("material").as_string()));

	std::string name = elem.attribute("id").as_string();
	_meshes.emplace_back(name, m);
}

void model::load_animations(pugi::xml_node elem) {
	size_t animations_count = elem.attribute("count").as_uint();
	
	_animations.clear();
	_animations.reserve(animations_count);

	size_t animation_num = 0;
	for (pugi::xml_node animation_xml = elem.child("animation"); animation_xml; animation_xml = animation_xml.next_sibling("animation"), ++animation_num) {
		load_animation(animation_xml);
	}
	BOOST_ASSERT(animation_num == animations_count);

	std::sort(_animations.begin(), _animations.end(),
		[](const Animations::value_type& a1, const Animations::value_type& a2) {
			return a1.first < a2.first;
		});
}

void model::load_animation(pugi::xml_node elem) {
	animation_ptr anim = new animation();

	anim->set_period(elem.attribute("duration").as_float(4.0f));
	anim->set_fps(elem.attribute("ticksPerSec").as_float(24.0f));

	for (pugi::xml_node channel_xml = elem.child("channel"); channel_xml; channel_xml = channel_xml.next_sibling("channel")) {
		bone* animation_bone = find_bone(channel_xml.attribute("node").as_string());
		if (!animation_bone) {
			LOGW("skipping animation for unknown node: ") << channel_xml.attribute("node").as_string();
			continue;
		}

		animation_channel_ptr channel = load_animation_channel(channel_xml);
		channel->set_bone(animation_bone);
		anim->add_channel(channel.get());
	}

	std::string name = elem.attribute("name").as_string();
	_animations.emplace_back(name, anim);
}

animation_channel_ptr model::load_animation_channel(pugi::xml_node elem) {
	animation_channel_ptr anim = new animation_channel();

	pugi::xml_node transforms_xml = elem.child("transforms");
	if (transforms_xml) {
		size_t transforms_count = transforms_xml.attribute("count").as_uint();

		anim->reserve_rotations(transforms_count);
		anim->reserve_scalings(transforms_count);
		anim->reserve_translations(transforms_count);

		glm::vec3 last_scaling;
		glm::quat last_rotation;
		glm::vec3 last_translation;
		bool first = true;

		size_t transforms_num = 0;
		for (pugi::xml_node transform_xml = transforms_xml.child("transform"); transform_xml; transform_xml = transform_xml.next_sibling("transform"), ++transforms_num) {
			glm::vec3 scaling;
			glm::quat rotation;
			glm::vec3 translation;

			decompose_matrix(load_matrix(transform_xml), scaling, rotation, translation);

			float time = transform_xml.attribute("time").as_float();

			if (first || last_scaling != scaling) {
				anim->add_scaling(time, scaling);
			}
				
			if (first || last_rotation != rotation) {
				anim->add_rotation(time, rotation);
			}
				
			if (first || last_translation != translation) {
				anim->add_translation(time, translation);
			}

			last_scaling = scaling;
			last_rotation = rotation;
			last_translation = translation;
			first = false;
		}
		BOOST_ASSERT(transforms_num == transforms_count);
	} else {
		pugi::xml_node rotations_xml = elem.child("rotations");
		size_t rotations_count = rotations_xml.attribute("count").as_uint();
			
		anim->reserve_rotations(rotations_count);
			
		size_t rotation_num = 0;
		for (pugi::xml_node rotation_xml = rotations_xml.child("rotation"); rotation_xml; rotation_xml = rotation_xml.next_sibling("rotation"), ++rotation_num) {
			anim->add_rotation(rotation_xml.attribute("time").as_float(), load_quat(rotation_xml));
		}
		BOOST_ASSERT(rotation_num == rotations_count);

		pugi::xml_node scalings_xml = elem.child("scalings");
		size_t scalings_count = scalings_xml.attribute("count").as_uint();
			
		anim->reserve_scalings(scalings_count);
			
		size_t scaling_num = 0;
		for (pugi::xml_node scaling_xml = scalings_xml.child("scaling"); scaling_xml; scaling_xml = scaling_xml.next_sibling("scaling"), ++scaling_num) {
			anim->add_scaling(scaling_xml.attribute("time").as_float(), load_vec3(scaling_xml));
		}
		BOOST_ASSERT(scaling_num == scalings_count);

		pugi::xml_node positions_xml = elem.child("positions");
		size_t positions_count = positions_xml.attribute("count").as_uint();
			
		anim->reserve_translations(positions_count);
			
		size_t position_num = 0;
		for (pugi::xml_node position_xml = positions_xml.child("position"); position_xml; position_xml = position_xml.next_sibling("position"), ++position_num) {
			anim->add_translation(position_xml.attribute("time").as_float(), load_vec3(position_xml));
		}
		BOOST_ASSERT(position_num == positions_count);
	}

	return anim;
}

render::color_value model::load_color_value(pugi::xml_node elem) {
	render::color_value color(
		elem.attribute("b").as_float(),
		elem.attribute("g").as_float(),
		elem.attribute("r").as_float(),
		elem.attribute("a").as_float(1.0f)
	);

	return color;
}

glm::mat4 model::load_matrix(pugi::xml_node elem) {
	glm::mat4 mat(
		elem.attribute("_11").as_float(), elem.attribute("_12").as_float(), elem.attribute("_13").as_float(), elem.attribute("_14").as_float(),
		elem.attribute("_21").as_float(), elem.attribute("_22").as_float(), elem.attribute("_23").as_float(), elem.attribute("_24").as_float(),
		elem.attribute("_31").as_float(), elem.attribute("_32").as_float(), elem.attribute("_33").as_float(), elem.attribute("_34").as_float(),
		elem.attribute("_41").as_float(), elem.attribute("_42").as_float(), elem.attribute("_43").as_float(), elem.attribute("_44").as_float()
	);

	return mat;
}

glm::vec3 model::load_vec3(pugi::xml_node elem) {
	glm::vec3 vec(
		elem.attribute("x").as_float(),
		elem.attribute("y").as_float(),
		elem.attribute("z").as_float()
	);

	return vec;
}

glm::quat model::load_quat(pugi::xml_node elem) {
	glm::quat quat(
		elem.attribute("x").as_float(),
		elem.attribute("y").as_float(),
		elem.attribute("z").as_float(),
		elem.attribute("w").as_float()
	);

	return quat;
}

void model::draw() {
	for (Meshes::const_iterator it = _meshes.begin(); it != _meshes.end(); ++it) {
		mesh* m = boost::get_pointer((*it).second);
		m->draw();
	}
}

namespace {

template<typename T, typename Collection>
T* find_in_sorted_collection(const Collection& coll, const char* name) {
	typename Collection::const_iterator it = std::lower_bound(std::begin(coll), std::end(coll), name,
		[](const typename Collection::value_type& val, const char* name) {
			return val.first < name;
		});

	if ((*it).first == name) {
		return boost::get_pointer((*it).second);
	}

	return nullptr;
}

} // namespace

mesh* model::find_mesh(const char* name) const {
	return find_in_sorted_collection<mesh>(_meshes, name);
}

material* model::find_material(const char* name) const {
	return find_in_sorted_collection<material>(_materials, name);
}

bone* model::find_bone(const char* name) const {
	return find_in_sorted_collection<bone>(_bones, name);
}

animation* model::find_animation(const char* name) const {
	return find_in_sorted_collection<animation>(_animations, name);
}

// animation info

animation_player::animation_info::animation_info(animation* anim)
	: _anim(anim)
	, _paused(false)
	, _time(0.0f)
	, _speed(1.0f)
	, _blend(1.0f)
{
}

void animation_player::animation_info::advance_time(float dt) {
	_time += dt * _speed;
}

// animation player

animation_player::animation_player(model* m)
	: _model(m)
{
}

void animation_player::play(const char* name) {
	Animations::iterator it = _animations.find(name);
	if (it != _animations.end()) {
		animation_info& info = (*it).second;
		// unpause and return if already playing
		if (info.is_paused()) {
			info.set_paused(false);
			return;
		}
	}

	animation* anim = _model->find_animation(name);
	BOOST_ASSERT(!!anim);

	if (anim) {
		_animations.emplace(name, animation_info(anim));
	}
}

void animation_player::pause(const char* name) {
	Animations::iterator it = _animations.find(name);
	BOOST_ASSERT_MSG(it != _animations.end(), "animation not playing");
	if (it != _animations.end()) {
		animation_info& info = (*it).second;
		// unpause and return if already playing
		if (!info.is_paused()) {
			info.set_paused(true);
			return;
		}
	}
}

void animation_player::stop(const char* name) {
	_animations.erase(name);
}

void animation_player::update(float dt) {
	for (auto& kvp: _animations) {
		animation_info& info = kvp.second;
		if (!info.is_paused()) {
			info.advance_time(dt);
		}
	}
}

} // namespace scenegraph
