#ifndef COBALT_OBJECT_FWD_HPP_INCLUDED
#define COBALT_OBJECT_FWD_HPP_INCLUDED

#pragma once

// Classes in this file:
//     component
//     object
//     scene

#include <cobalt/utility/intrusive.hpp>
#include <cobalt/utility/hash.hpp>
#include <cobalt/utility/enumerator.hpp>

#include <type_traits>
#include <forward_list>

namespace cobalt {

class object;

/// Component
class component : public ref_counter<component>, public intrusive_slist_base<component> {
public:
	component() = default;
	
	component(component&&) = default;
	component& operator=(component&&) = default;
	
	component(const component&) = default;
	component& operator=(const component&) = default;
	
	virtual ~component() = default;

	virtual hash_type type() const noexcept = 0;

	class object* object() const noexcept { return _object; }
	
	void detach();
	
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
	
/// Component factory
class component_factory {
public:
	explicit constexpr component_factory(hash_type name) noexcept;
	explicit component_factory(const char* name) noexcept
		: component_factory(murmur3(name, 0)) {}
	
	component_factory(const component_factory&) = delete;
	component_factory& operator=(const component_factory&) = delete;
	
	virtual component* create_component() = 0;
	
	static component* create(hash_type name);	
	static component* create(const char* name) { return create(murmur3(name, 0)); }
	
private:
	static component_factory* root(component_factory* new_value = nullptr) noexcept;
	static bool check_unique_name(hash_type name) noexcept;
	
private:
	hash_type _name = 0;
	component_factory* _next = nullptr;
};

/// Defines simple component factory
#define CO_DEFINE_COMPONENT_FACTORY(component) CO_DEFINE_COMPONENT_FACTORY_WITH_NAME(component, #component)

/// Defines simple component factory with specified component name
#define CO_DEFINE_COMPONENT_FACTORY_WITH_NAME(component, component_name)                       \
	class component##_factory : public component_factory {                                     \
	public:                                                                                    \
		constexpr component##_factory() noexcept : component_factory(component_name##_hash) {} \
		virtual component* create_component() override { return new component(); }             \
	} static component##_factory_instance;                                                     \

/// Object is a container for components
class object : public ref_counter<object>, public intrusive_slist_base<object> {
public:
	object() = default;
	
	object(object&&) = default;
	object& operator=(object&&) = default;

	object(const object&) = delete;
	object& operator=(const object&) = delete;
	
	explicit object(hash_type name) noexcept : _name(name) {}
	explicit object(const char* name) noexcept : _name(murmur3(name, 0)) {}
	
	~object();

	hash_type name() const noexcept { return _name; }
	void name(hash_type name) noexcept { _name = name; }
	void name(const char* name) noexcept { _name = murmur3(name, 0); }

	bool active() const noexcept { return _active; }
	void active(bool active) noexcept { _active = active; }
	bool active_in_hierarchy() const noexcept;

	object* parent() const noexcept { return _parent; }
	
	enumerator<intrusive_slist<object>::const_iterator> children() const { return make_enumerator(_children); }
	enumerator<intrusive_slist<component>::const_iterator> components() const { return make_enumerator(_components); }
	
	object* attach(object* o) noexcept;
	counted_ptr<object> detach(object* o);
	
	void detach();
	
	void remove_all_children();
	
	const object* find_root() const noexcept;
	const object* find_child(hash_type name) const noexcept;
	const object* find_object_in_parent(hash_type name) const noexcept;
	const object* find_object_in_children(hash_type name) const noexcept;
	
	const object* find_child(const char* name) const noexcept;
	const object* find_object_in_parent(const char* name) const noexcept { return find_object_in_parent(murmur3(name, 0)); }
	const object* find_object_in_children(const char* name) const noexcept { return find_object_in_children(murmur3(name, 0)); }

	component* attach(component* c) noexcept;
	counted_ptr<component> detach(component* c);
	
	size_t remove_components(hash_type component_type);
	void remove_all_components();
	
	const component* find_component(hash_type component_type) const noexcept;
	const component* find_component_in_parent(hash_type component_type) const noexcept;
	const component* find_component_in_children(hash_type component_type) const;
	
	template <typename T, typename = typename std::enable_if_t<std::is_base_of<component, T>::value>>
	const T* find_component() const { return static_cast<const T*>(find_component(T::component_type)); }
	template <typename T, typename = typename std::enable_if_t<std::is_base_of<component, T>::value>>
	const T* find_component_in_parent() const { return static_cast<const T*>(find_component_in_parent(T::component_type)); }
	template <typename T, typename = typename std::enable_if_t<std::is_base_of<component, T>::value>>
	const T* find_component_in_children() const { return static_cast<const T*>(find_component_in_children(T::component_type)); }
	
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
	
	using Children = intrusive_slist<object>;
	Children _children;
	
	using Components = intrusive_slist<component>;
	Components _components;
	
	hash_type _name = 0;
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
