#ifndef COBALT_OBJECT_FWD_HPP_INCLUDED
#define COBALT_OBJECT_FWD_HPP_INCLUDED

#pragma once

// Classes in this file:
//     component
//     object
//     scene

#include <cobalt/utility/intrusive.hpp>
#include <cobalt/utility/identifier.hpp>
#include <cobalt/utility/enumerator.hpp>
#include <cobalt/utility/hash.hpp>

#include <type_traits>
#include <forward_list>

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
	
	explicit object(const identifier& id) noexcept : _id(id) {}
	
	~object();

	const identifier& id() const noexcept { return _id; }
	void id(const identifier& id) noexcept { _id = id; }

	bool active() const noexcept { return _active; }
	void active(bool active) noexcept { _active = active; }
	bool active_in_hierarchy() const noexcept;

	object* parent() const noexcept { return _parent; }
	
	enumerator<children_type::const_iterator> children() const noexcept { return make_enumerator(_children); }
	enumerator<components_type::const_iterator> components() const noexcept { return make_enumerator(_components); }
	
	object* attach(object* o) noexcept;
	counted_ptr<object> detach(object* o) noexcept;
	
	void detach() noexcept;
	
	void remove_all_children() noexcept;
	
	const object* find_root() const noexcept;
	const object* find_object(const identifier& id) const noexcept;
	const object* find_object_in_parent(const identifier& id) const noexcept;
	const object* find_object_in_children(const identifier& id) const noexcept;
	const object* find_object_with_path(const char* path) const noexcept;

	component* attach(component* c) noexcept;
	counted_ptr<component> detach(component* c) noexcept;
	
	size_t remove_components(uint32_t component_type) noexcept;
	void remove_all_components() noexcept;
	
	const component* find_component(uint32_t component_type) const noexcept;
	const component* find_component_in_parent(uint32_t component_type) const noexcept;
	const component* find_component_in_children(uint32_t component_type) const;
	
	const component* find_component(const char* name) const noexcept { return find_component(murmur3(name, 0)); }
	const component* find_component_in_parent(const char* name) const noexcept { return find_component_in_parent(murmur3(name, 0)); }
	const component* find_component_in_children(const char* name) const { return find_component_in_children(murmur3(name, 0)); }
	
	template <typename T, typename = typename std::enable_if_t<std::is_base_of<component, T>::value>>
	const T* find_component() const noexcept { return static_cast<const T*>(find_component(T::component_type)); }
	template <typename T, typename = typename std::enable_if_t<std::is_base_of<component, T>::value>>
	const T* find_component_in_parent() const noexcept { return static_cast<const T*>(find_component_in_parent(T::component_type)); }
	template <typename T, typename = typename std::enable_if_t<std::is_base_of<component, T>::value>>
	const T* find_component_in_children() const noexcept { return static_cast<const T*>(find_component_in_children(T::component_type)); }
	
	template <typename OutputIterator>
	void find_components(uint32_t component_type, OutputIterator result) const;
	template <typename OutputIterator>
	void find_components_in_parent(uint32_t component_type, OutputIterator result) const;
	template <typename OutputIterator>
	void find_components_in_children(uint32_t component_type, OutputIterator result) const;
	
	template <typename T, typename OutputIterator, typename = typename std::enable_if_t<std::is_base_of<component, T>::value>>
	void find_components(OutputIterator result) const;
	template <typename T, typename OutputIterator, typename = typename std::enable_if_t<std::is_base_of<component, T>::value>>
	void find_components_in_parent(OutputIterator result) const;
	template <typename T, typename OutputIterator, typename = typename std::enable_if_t<std::is_base_of<component, T>::value>>
	void find_components_in_children(OutputIterator result) const;

private:
	void parent(object* parent) noexcept { _parent = parent; }

private:
	mutable object* _parent = nullptr;
	children_type _children;
	components_type _components;	
	identifier _id;
	bool _active = true;
};
	
/// Scene
class scene : public object {
public:
	scene() = default;
	
	scene(const scene&) = delete;
	scene& operator=(const scene&) = delete;
	
private:
};

} // namespace cobalt

#endif // COBALT_OBJECT_FWD_HPP_INCLUDED
