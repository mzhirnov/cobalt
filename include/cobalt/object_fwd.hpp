#ifndef COBALT_OBJECT_FWD_HPP_INCLUDED
#define COBALT_OBJECT_FWD_HPP_INCLUDED

#pragma once

// Classes in this file:
//     component
//     object
//
// Functions in this file:
//     find_root
//     find_parent
//     find_child
//     find_child_in_hierarchy
//     find_object_with_path
//     find_component
//     find_component_in_parent
//     find_component_in_children

#include <cobalt/utility/intrusive.hpp>
#include <cobalt/utility/identifier.hpp>
#include <cobalt/utility/enumerator.hpp>
#include <cobalt/utility/hash.hpp>

#include <type_traits>

namespace cobalt {

class object;

/// Component
class component
	: public ref_counter<component>
	, public intrusive_slist_base<component>
{
public:
	component() = default;
	
	component(component&&) = default;
	component& operator=(component&&) = default;
	
	component(const component&) = default;
	component& operator=(const component&) = default;
	
	virtual ~component() = default;

	virtual uint32_t type() const noexcept = 0;

	class object* object() const noexcept { return _object; }
	
	void detach() noexcept;
	
private:
	friend class object;
	
	void object(class object* object) noexcept { _object = object; }

private:
	class object* _object = nullptr;
};

/// Base class for components
template <uint32_t ComponentType>
class basic_component : public component {
public:
	static constexpr uint32_t component_type = ComponentType;
	
	virtual uint32_t type() const noexcept override { return ComponentType; }
};

/// Object is a container for components
class object
	: public ref_counter<object>
	, public intrusive_slist_base<object>
{
public:
	using children_type = intrusive_slist<object>;
	using components_type = intrusive_slist<component>;
	
	object() = default;
	
	object(object&&) = default;
	object& operator=(object&&) = default;

	object(const object&) = delete;
	object& operator=(const object&) = delete;
	
	explicit object(const identifier& name) noexcept : _name(name) {}
	
	~object();
	
	object* parent() const noexcept { return _parent; }
	
	enumerator<children_type::const_iterator> children() const noexcept { return make_enumerator(_children); }
	enumerator<components_type::const_iterator> components() const noexcept { return make_enumerator(_components); }

	const identifier& name() const noexcept { return _name; }
	void name(const identifier& name) noexcept { _name = name; }

	bool active() const noexcept { return _active; }
	void active(bool active) noexcept { _active = active; }
	bool active_in_hierarchy() const noexcept;
	
	object* add_child(object* o) noexcept;
	counted_ptr<object> remove_child(object* o) noexcept;
	
	void detach() noexcept;
	
	void remove_all_children() noexcept;

	component* add_component(component* c) noexcept;
	counted_ptr<component> remove_component(component* c) noexcept;
	
	size_t remove_components(uint32_t component_type) noexcept;
	void remove_all_components() noexcept;

private:
	void parent(object* parent) noexcept { _parent = parent; }

private:
	object* _parent = nullptr;
	children_type _children;
	components_type _components;	
	identifier _name;
	bool _active = true;
};

////////////////////////////////////////////////////////////////////////////////

const object* find_root(const object* o) noexcept;
const object* find_parent(const object* o, const identifier& name) noexcept;
const object* find_child(const object* o, const identifier& name) noexcept;
const object* find_child_in_hierarchy(const object* o, const identifier& name) noexcept;
const object* find_object_with_path(const object* o, const char* path) noexcept;

const component* find_component(const object* o, uint32_t component_type) noexcept;
const component* find_component_in_parent(const object* o, uint32_t component_type) noexcept;
const component* find_component_in_children(const object* o, uint32_t component_type);

const component* find_component(const object* o, const char* name) noexcept;
const component* find_component_in_parent(const object* o, const char* name) noexcept;
const component* find_component_in_children(const object* o, const char* name);

template <typename T, typename = typename std::enable_if_t<std::is_base_of<component, T>::value>>
const T* find_component(const object* o) noexcept;
template <typename T, typename = typename std::enable_if_t<std::is_base_of<component, T>::value>>
const T* find_component_in_parent(const object* o) noexcept;
template <typename T, typename = typename std::enable_if_t<std::is_base_of<component, T>::value>>
const T* find_component_in_children(const object* o) noexcept;

template <typename OutputIterator>
void find_components(const object* o, uint32_t component_type, OutputIterator result);
template <typename OutputIterator>
void find_components_in_parent(const object* o, uint32_t component_type, OutputIterator result);
template <typename OutputIterator>
void find_components_in_children(const object* o, uint32_t component_type, OutputIterator result);

template <typename T, typename OutputIterator, typename = typename std::enable_if_t<std::is_base_of<component, T>::value>>
void find_components(const object* o, OutputIterator result);
template <typename T, typename OutputIterator, typename = typename std::enable_if_t<std::is_base_of<component, T>::value>>
void find_components_in_parent(const object* o, OutputIterator result);
template <typename T, typename OutputIterator, typename = typename std::enable_if_t<std::is_base_of<component, T>::value>>
void find_components_in_children(const object* o, OutputIterator result);

} // namespace cobalt

#endif // COBALT_OBJECT_FWD_HPP_INCLUDED
