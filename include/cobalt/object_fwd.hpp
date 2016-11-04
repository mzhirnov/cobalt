#ifndef COBALT_OBJECT_HPP_INCLUDED
#define COBALT_OBJECT_HPP_INCLUDED

#pragma once

// Classes in this file:
//     component
//     object

#include <cobalt/utility/ref_ptr.hpp>

#include <boost/type_index.hpp>

#include <type_traits>
#include <vector>
#include <string>

namespace cobalt {
	
using boost::typeindex::type_info;
using boost::typeindex::type_index;
using boost::typeindex::type_id;

class object;

/// Base class for all components
class component : public ref_counter<component> {
public:
	virtual ~component() = default;

	virtual const char* name() const = 0;

	object* get_object() const { return _object; }
	
private:
	friend class object;
	
	void set_object(object* object) { _object = object; }

private:
	object* _object = nullptr;
};

/// Object is a container for components
class object : public ref_counter<object> {
public:
	object() = default;
	explicit object(const char* name) : _name(name) {}

	const char* name() const { return _name.c_str(); }
	void name(const char* name) { _name = name; }

	bool active() const { return _active; }
	void active(bool active) { _active = active; }
	bool active_in_hierarchy() const;

	object* parent() const { return _parent; }
	
	object* add_child(const ref_ptr<object>& o);
	ref_ptr<object> remove_child(object* o);
	
	void remove_from_parent();
	
	object* find_root_object() const;
	object* find_object(const char* name);
	object* find_object_in_parent(const char* name) const;
	object* find_object_in_children(const char* name) const;

	template<typename T, typename = typename std::enable_if_t<std::is_base_of<component, T>::value>>
	T* add_component(const ref_ptr<component>& c)
		{ return static_cast<T*>(add_component(type_id<T>(), c)); }

	template<typename T, typename = typename std::enable_if_t<std::is_base_of<component, T>::value>>
	void remove_components()
		{ remove_components(type_id<T>()); }

	bool remove_component(component* c);

	template<typename T, typename = typename std::enable_if_t<std::is_base_of<component, T>::value>>
	T* get_component() const
		{ return static_cast<T*>(get_component(type_id<T>())); }
	
	template<typename T, typename = typename std::enable_if_t<std::is_base_of<component, T>::value>>
	T* get_component_in_parent() const
		{ return static_cast<T*>(get_component_in_parent(type_id<T>())); }
	
	template<typename T, typename = typename std::enable_if_t<std::is_base_of<component, T>::value>>
	T* get_component_in_children() const
		{ return static_cast<T*>(get_component_in_children(type_id<T>())); }

	template<typename T, typename = typename std::enable_if_t<std::is_base_of<component, T>::value>>
	void get_components(std::vector<ref_ptr<component>>& components) const
		{ get_components(type_id<T>(), components); }

	template<typename T, typename = typename std::enable_if_t<std::is_base_of<component, T>::value>>
	void get_components_in_parent(std::vector<ref_ptr<component>>& components) const
		{ get_components_in_parent(type_id<T>(), components); }

	template<typename T, typename = typename std::enable_if_t<std::is_base_of<component, T>::value>>
	void get_components_in_children(std::vector<ref_ptr<component>>& components) const
		{ get_components_in_children(type_id<T>(), components); }

private:
	void parent(object* parent) { _parent = parent; }
	
	// Components management
	//
	component* add_component(const type_index& type, const ref_ptr<component>& c);
	void remove_components(const type_index& type);

	component* get_component(const type_index& type) const;
	component* get_component_in_parent(const type_index& type) const;
	component* get_component_in_children(const type_index& type) const;

	void get_components(const type_index& type, std::vector<ref_ptr<component>>& components) const;
	void get_components_in_parent(const type_index& type, std::vector<ref_ptr<component>>& components) const;
	void get_components_in_children(const type_index& type, std::vector<ref_ptr<component>>& components) const;

private:
	std::string _name;
	bool _active = true;
	
	mutable object* _parent = nullptr;

	using Children = std::vector<ref_ptr<object>>;
	Children _children;
	
	using Components = std::vector<std::pair<type_index, ref_ptr<component>>>;
	Components _components;
};

} // namespace cobalt

#endif // COBALT_OBJECT_HPP_INCLUDED
