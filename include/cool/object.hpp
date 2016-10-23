#pragma once

// Classes in this file:
//     component
//     transform
//     object

#include "cool/common.hpp"

class component;
class object;

using component_ptr = boost::intrusive_ptr<component>;
using object_ptr = boost::intrusive_ptr<object>;

// Base for all components
class component : public ref_counter<component> {
public:
	virtual ~component() = default;

	virtual const char* name() const = 0;

	object* get_object() const { return _object; }
	void set_object(object* object) { _object = object; }

private:
	object* _object;
};

// Transform component
class transform : public component {
public:
	explicit transform(object* o) { set_object(o); }

	virtual const char* name() const override { return "transform"; }

	float x() const { return _x; }
	float y() const { return _y; }
	float z() const { return _z; }

	void x(float value) { _x = value; }
	void y(float value) { _y = value; }
	void z(float value) { _z = value; }

private:
	float _x = 0;
	float _y = 0;
	float _z = 0;
};

class object : public ref_counter<object> {
public:
	object() : _transform(this) {}
	explicit object(const char* name) : _transform(this), _name(name) {}

	virtual ~object() = default;

	// Name
	const char* name() const { return _name.c_str(); }
	void name(const char* name) { _name = name; }

	// Active
	bool active_self() const { return _active; }
	void active_self(bool active) { _active = active; }
	bool active_in_hierarchy() const;

	// Parent
	object* parent() const { return _parent; }
	void parent(object* parent) { _parent = parent; }

	// Manage components
	template<typename T, typename = typename std::enable_if<std::is_base_of<component, T>::value>::type>
	T* add_component(const component_ptr& c)
		{ return static_cast<T*>(add_component(boost::typeindex::type_id<T>(), c)); }

	template<typename T, typename = typename std::enable_if<std::is_base_of<component, T>::value>::type>
	void remove_components()
		{ remove_components(boost::typeindex::type_id<T>()); }

	bool remove_component(component* c);

	// Get components
	template<typename T, typename = typename std::enable_if<std::is_base_of<component, T>::value>::type>
	T* get_component()
		{ return static_cast<T*>(get_component(boost::typeindex::type_id<T>())); }

	template<>
	transform* get_component<transform>() { return get_transform(); }
	
	template<typename T, typename = typename std::enable_if<std::is_base_of<component, T>::value>::type>
	T* get_component_in_parent() const
		{ return static_cast<T*>(get_component_in_parent(boost::typeindex::type_id<T>())); }
	
	template<typename T, typename = typename std::enable_if<std::is_base_of<component, T>::value>::type>
	T* get_component_in_children() const
		{ return static_cast<T*>(get_component_in_children(boost::typeindex::type_id<T>())); }

	template<typename T, typename = typename std::enable_if<std::is_base_of<component, T>::value>::type>
	void get_components(std::vector<component_ptr>& components) const
		{ get_components(boost::typeindex::type_id<T>(), components); }

	template<typename T, typename = typename std::enable_if<std::is_base_of<component, T>::value>::type>
	void get_components_in_parent(std::vector<component_ptr>& components) const
		{ get_components_in_parent(boost::typeindex::type_id<T>(), components); }

	template<typename T, typename = typename std::enable_if<std::is_base_of<component, T>::value>::type>
	void get_components_in_children(std::vector<component_ptr>& components) const
		{ get_components_in_children(boost::typeindex::type_id<T>(), components); }

	// Manage children objects
	object* add_child_object(const object_ptr& o);
	bool remove_child_object(const object_ptr& o);

	// Find objects
	object* find_root_object() const;
	object* find_object(const char* name);
	object* find_object_in_parent(const char* name);
	object* find_object_in_children(const char* name);

private:
	// Manage components
	component* add_component(const boost::typeindex::type_index& type, const component_ptr& c);
	void remove_components(const boost::typeindex::type_index& type);

	// Get components
	transform* get_transform() { return &_transform; }

	component* get_component(const boost::typeindex::type_index& type) const;
	component* get_component_in_parent(const boost::typeindex::type_index& type) const;
	component* get_component_in_children(const boost::typeindex::type_index& type) const;

	void get_components(const boost::typeindex::type_index& type, std::vector<component_ptr>& components) const;
	void get_components_in_parent(const boost::typeindex::type_index& type, std::vector<component_ptr>& components) const;
	void get_components_in_children(const boost::typeindex::type_index& type, std::vector<component_ptr>& components) const;

private:
	transform _transform;

	using Components = std::vector<std::pair<boost::typeindex::type_index, component_ptr>>;
	Components _components;

	object* _parent = nullptr;

	using Children = std::vector<object_ptr>;
	Children _children;

	std::string _name;

	bool _active = true;
};
