#ifndef COBALT_OBJECT_HPP_INCLUDED
#define COBALT_OBJECT_HPP_INCLUDED

#pragma once

// Classes in this file:
//     component
//     object

#include <cobalt/utility/ref_ptr.hpp>
#include <cobalt/hash.hpp>

#include <type_traits>
#include <vector>
#include <string>

namespace cobalt {

class object;

/// Component
class component : public ref_counter<component> {
public:
	virtual ~component() = default;

	virtual hash_type type() const noexcept = 0;

	class object* object() const noexcept { return _object; }
	
	void remove_from_parent();
	
private:
	friend class object;
	
	void object(class object* object) noexcept { _object = object; }

private:
	class object* _object = nullptr;
};

/// Base class for components
template <hash_type ComponentType>
class basic_component : public component {
public:
	static constexpr hash_type component_type = ComponentType;
	
	virtual hash_type type() const noexcept override { return ComponentType; }
};

/// Object is a container for components
class object : public ref_counter<object> {
public:
	object() noexcept = default;
	explicit object(hash_type name) noexcept : _name(name) {}
	explicit object(const char* name) noexcept : _name(murmur3(name, 0)) {}

	hash_type name() const noexcept { return _name; }
	void name(hash_type name) noexcept { _name = name; }
	void name(const char* name) noexcept { _name = murmur3(name, 0); }

	bool active() const noexcept { return _active; }
	void active(bool active) noexcept { _active = active; }
	bool active_in_hierarchy() const noexcept;

	object* parent() const noexcept { return _parent; }
	
	object* add_child(const ref_ptr<object>& o);
	object* add_child(ref_ptr<object>&& o);
	ref_ptr<object> remove_child(object* o);
	
	void remove_from_parent();
	
	object* find_root() const noexcept;
	object* find_child(hash_type name) const noexcept;
	object* find_object_in_parent(hash_type name) const noexcept;
	object* find_object_in_children(hash_type name) const noexcept;
	
	object* find_child(const char* name) const noexcept;
	object* find_object_in_parent(const char* name) const noexcept { return find_object_in_parent(murmur3(name, 0)); }
	object* find_object_in_children(const char* name) const noexcept { return find_object_in_children(murmur3(name, 0)); }

	component* add_component(const ref_ptr<component>& c);
	component* add_component(ref_ptr<component>&& c);
	
	ref_ptr<component> remove_component(component* c);
	size_t remove_components(hash_type component_type);
	
	component* find_component(hash_type component_type) const noexcept;
	component* find_component_in_parent(hash_type component_type) const noexcept;
	component* find_component_in_children(hash_type component_type) const;
	
	template <typename T, typename = typename std::enable_if_t<std::is_base_of<component, T>::value>>
	T* find_component() const { return static_cast<T*>(find_component(T::component_type)); }
	template <typename T, typename = typename std::enable_if_t<std::is_base_of<component, T>::value>>
	T* find_component_in_parent() const { return static_cast<T*>(find_component_in_parent(T::component_type)); }
	template <typename T, typename = typename std::enable_if_t<std::is_base_of<component, T>::value>>
	T* find_component_in_children() const { return static_cast<T*>(find_component_in_children(T::component_type)); }
	
	template <typename OutputIterator>
	void find_components(hash_type component_type, OutputIterator result) const;
	template <typename OutputIterator>
	void find_components_in_parent(hash_type component_type, OutputIterator result) const;
	template <typename OutputIterator>
	void find_components_in_children(hash_type component_type, OutputIterator result) const;
	
	template <typename T, typename OutputIterator, typename = typename std::enable_if_t<std::is_base_of<component, T>::value>>
	void find_components(OutputIterator result) const;
	template <typename T, typename OutputIterator, typename = typename std::enable_if_t<std::is_base_of<component, T>::value>>
	void find_components_in_parent(OutputIterator result) const;
	template <typename T, typename OutputIterator, typename = typename std::enable_if_t<std::is_base_of<component, T>::value>>
	void find_components_in_children(OutputIterator result) const;

private:
	void parent(object* parent) { _parent = parent; }

private:
	mutable object* _parent = nullptr;

	using Children = std::vector<ref_ptr<object>>;
	Children _children;
	
	using Components = std::vector<ref_ptr<component>>;
	Components _components;
	
	hash_type _name = 0;
	bool _active = true;
};

} // namespace cobalt

#endif // COBALT_OBJECT_HPP_INCLUDED
