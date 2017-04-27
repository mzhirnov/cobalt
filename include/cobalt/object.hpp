#ifndef COBALT_OBJECT_HPP_INCLUDED
#define COBALT_OBJECT_HPP_INCLUDED

#pragma once

// Classes in this file:
//     component
//     basic_component
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

#include <boost/utility/string_view.hpp>

#include <type_traits>
#include <deque>

namespace cobalt {

class object;

/// Component
class component
	: public ref_counter<component>
	, public intrusive_single_list_base<component>
{
public:
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
	, public intrusive_single_list_base<object>
{
public:
	using children_type = intrusive_single_list<object>;
	using components_type = intrusive_single_list<component>;
	
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
// component
//
	
inline void component::detach() noexcept {
	if (_object)
		_object->remove_component(this);
}
	
////////////////////////////////////////////////////////////////////////////////
// object
//

inline object::~object() {
	remove_all_components();
	remove_all_children();
}

inline bool object::active_in_hierarchy() const noexcept {
	for (auto o = this; o; o = o->parent()) {
		if (!o->active())
			return false;
	}

	return true;
}

inline object* object::add_child(object* o) noexcept {
	BOOST_ASSERT(o != nullptr);
	if (!o) return nullptr;
	
	add_ref(o);
	
	_children.push_front(*o);
	
	o->parent(this);
	
	return o;
}

inline counted_ptr<object> object::remove_child(object* o) noexcept {
	counted_ptr<object> sp;
	
	if (!o)
		return sp;
	
	for (auto it = _children.before_begin(), next = std::next(it); next != _children.end(); ++it, ++next) {
		if (std::addressof(*next) == o) {
			_children.erase_after_and_dispose(it, [&](auto&& o) {
				// Don't increase ref_counter, transfer ownership to smart pointer
				sp.reset(o, false);
				sp->parent(nullptr);
			});
			break;
		}
	}

	return sp;
}

inline void object::detach() noexcept {
	if (_parent)
		_parent->remove_child(this);
}
	
inline void object::remove_all_children() noexcept {
	_children.clear_and_dispose([](auto&& o) {
		o->parent(nullptr);
		release(o);
	});
}

inline component* object::add_component(component* c) noexcept {
	add_ref(c);
	
	_components.push_front(*c);
	
	c->object(this);
	
	return c;
}

inline counted_ptr<component> object::remove_component(component* c) noexcept {
	counted_ptr<component> sp;
	
	if (!c)
		return sp;
	
	for (auto it = _components.before_begin(), next = std::next(it); next != _components.end(); ++it, ++next) {
		if (&*next == c) {
			_components.erase_after_and_dispose(it, [&](auto&& c) {
				// Don't increase ref_counter, transfer ownership to smart pointer
				sp.reset(c, false);
				sp->object(nullptr);
			});
			break;
		}
	}
	
	return sp;
}

inline size_t object::remove_components(uint32_t component_type) noexcept {
	size_t count = 0;
	
	_components.remove_and_dispose_if(
		[&](auto&& c) { return c.type() == component_type; },
		[&](auto&& c) { c->object(nullptr); release(c); ++count; }
	);
	
	return count;
}
	
inline void object::remove_all_components() noexcept {
	_components.clear_and_dispose([](auto&& c) {
		c->object(nullptr);
		release(c);
	});
}

////////////////////////////////////////////////////////////////////////////////

inline const object* find_root(const object* o) noexcept {
	BOOST_ASSERT(o != nullptr);
	if (!o) return nullptr;
	
	while (o->parent())
		o = o->parent();	
	
	return o;
}

inline const object* find_parent(const object* o, const identifier& name) noexcept {
	BOOST_ASSERT(o != nullptr);
	if (!o) return nullptr;
	
	for (o = o->parent(); o; o = o->parent()) {
		if (!o->active())
			continue;
		
		if (o->name() == name)
			return o;
	}
	
	return nullptr;
}

inline const object* find_child(const object* o, const identifier& name) noexcept {
	BOOST_ASSERT(o != nullptr);
	if (!o) return nullptr;
	
	for (auto&& child : o->children()) {
		if (!child.active())
			continue;
		
		if (child.name() == name)
			return &child;
	}
	
	return nullptr;
}

// Breadth-first search
inline const object* find_child_in_hierarchy(const object* o, const identifier& name) noexcept {
	BOOST_ASSERT(o != nullptr);
	if (!o) return nullptr;
	
	if (auto found = find_child(o, name))
		return found;
	
	for (auto&& child : o->children()) {
		if (!child.active())
			continue;
		
		if (auto found = find_child(&child, name))
			return found;
	}
	
	auto children = o->children();
	std::deque<std::reference_wrapper<const object>> queue(children.begin(), children.end());
	
	while (!queue.empty()) {
		auto&& front = queue.front().get();
		
		if (!front.active())
			continue;
		
		children = front.children();
		
		for (auto&& child : children) {
			if (!child.active())
				continue;
			
			if (auto found = find_child(&child, name))
				return found;
		}
		
		queue.insert(queue.end(), children.begin(), children.end());
		queue.pop_front();
	}
	
	return nullptr;
}

inline const object* find_object_with_path(const object* o, const char* path) noexcept {
	BOOST_ASSERT(o != nullptr);
	BOOST_ASSERT(path != nullptr);
	
	if (!o || !path)
		return nullptr;
	
	const char* b = path;
	const char* e = b;
	
	auto current = (*e == '/' ? find_root(o) : o);
	
	while (*e == '/')
		b = ++e;
	
	// Iterate through names in the path
	while (*e++) {
		if (*e == '/' || !*e) {
			boost::string_view name(b, e - b);
			bool found = false;
			
			// Compare child name with current path part
			for (auto&& child : current->children()) {
				if (!child.active())
					continue;
				
				if (child.name() == name) {
					current = &child;
					found = true;
					break;
				}
			}
			
			if (!found)
				return nullptr;
			
			while (*e == '/')
				b = ++e;
		}
	}
	
	return current;
}

inline const component* find_component(const object* o, uint32_t component_type) noexcept {
	BOOST_ASSERT(o != nullptr);
	if (!o) return nullptr;
	
	for (auto&& c : o->components()) {
		if (c.type() == component_type)
			return &c;
	}

	return nullptr;
}

inline const component* find_component_in_parent(const object* o, uint32_t component_type) noexcept {
	BOOST_ASSERT(o != nullptr);
	
	for ( ; o; o = o->parent()) {
		if (!o->active())
			continue;
		
		if (auto c = find_component(o, component_type))
			return c;
	}

	return nullptr;
}

// Breadth-first search
inline const component* find_component_in_children(const object* o, uint32_t component_type) {
	BOOST_ASSERT(o != nullptr);
	if (!o) return nullptr;
	
	if (auto c = find_component(o, component_type))
		return c;
	
	for (auto&& child : o->children()) {
		if (!child.active())
			continue;
		
		if (auto c = find_component(&child, component_type))
			return c;
	}
	
	auto children = o->children();
	std::deque<std::reference_wrapper<const object>> queue(children.begin(), children.end());
	
	while (!queue.empty()) {
		auto&& front = queue.front().get();
		
		children = front.children();
		
		for (auto&& child : children) {
			if (!child.active())
				continue;
			
			if (auto c = find_component(&child, component_type))
				return c;
		}
		
		queue.insert(queue.end(), children.begin(), children.end());
		queue.pop_front();
	}

	return nullptr;
}

inline const component* find_component(const object* o, const char* name) noexcept {
	return find_component(o, murmur3(name, 0));
}

inline const component* find_component_in_parent(const object* o, const char* name) noexcept {
	return find_component_in_parent(o, murmur3(name, 0));
}

inline const component* find_component_in_children(const object* o, const char* name) {
	return find_component_in_children(o, murmur3(name, 0));
}

template <typename T, typename = typename std::enable_if_t<std::is_base_of<component, T>::value>>
inline const T* find_component(const object* o) noexcept {
	return static_cast<const T*>(find_component(o, T::component_type));
}

template <typename T, typename = typename std::enable_if_t<std::is_base_of<component, T>::value>>
inline const T* find_component_in_parent(const object* o) noexcept {
	return static_cast<const T*>(find_component_in_parent(o, T::component_type));
}

template <typename T, typename = typename std::enable_if_t<std::is_base_of<component, T>::value>>
inline const T* find_component_in_children(const object* o) noexcept {
	return static_cast<const T*>(find_component_in_children(o, T::component_type));
}

template <typename OutputIterator>
inline void find_components(const object* o, uint32_t component_type, OutputIterator result) {
	for (auto&& c : o->components()) {
		if (c.type() == component_type)
			*result++ = &c;
	}
}

template <typename OutputIterator>
inline void find_components_in_parent(const object* o, uint32_t component_type, OutputIterator result) {
	BOOST_ASSERT(o != nullptr);
	if (!o) return;
	
	for ( ; o; o = o->parent()) {
		if (!o->active())
			continue;
		
		find_components(o, component_type, result);
	}
}

// Breadth-first search
template <typename OutputIterator>
inline void find_components_in_children(const object* o, uint32_t component_type, OutputIterator result) {
	BOOST_ASSERT(o != nullptr);
	if (!o) return;
	
	find_components(o, component_type, result);
	
	for (auto&& child : o->children()) {
		if (!child.active())
			continue;
		
		find_components(&child, component_type, result);
	}
	
	auto children = o->children();
	std::deque<std::reference_wrapper<const object>> queue(children.begin(), children.end());
	
	while (!queue.empty()) {
		auto&& front = queue.front().get();
		
		if (!front.active())
			continue;
		
		children = front.children();
		
		for (auto&& child : children) {
			if (!child.active())
				continue;
			
			find_components(&child, component_type, result);
		}
		
		queue.insert(queue.end(), children.begin(), children.end());
		queue.pop_front();
	}
}

template <typename T, typename OutputIterator, typename = typename std::enable_if_t<std::is_base_of<component, T>::value>>
inline void find_components(const object* o, OutputIterator result) {
	BOOST_ASSERT(o != nullptr);
	if (!o) return;
	
	for (auto&& c : o->components()) {
		if (c.type() == T::component_type)
			*result++ = static_cast<const T*>(&c);
	}
}

template <typename T, typename OutputIterator, typename = typename std::enable_if_t<std::is_base_of<component, T>::value>>
inline void find_components_in_parent(const object* o, OutputIterator result) {
	BOOST_ASSERT(o != nullptr);
	if (!o) return;
	
	for ( ; o; o = o->parent()) {
		if (!o->active())
			continue;
		
		find_components<T>(o, result);
	}
}

// Breadth-first search
template <typename T, typename OutputIterator, typename = typename std::enable_if_t<std::is_base_of<component, T>::value>>
inline void find_components_in_children(const object* o, OutputIterator result) {
	BOOST_ASSERT(o != nullptr);
	if (!o) return;
	
	find_components<T>(o, result);
	
	for (auto&& child : o->children()) {
		if (!child.active())
			continue;
		
		find_components<T>(&child, result);
	}
	
	auto children = o->children();
	std::deque<std::reference_wrapper<const object>> queue(children.begin(), children.end());
	
	while (!queue.empty()) {
		auto&& front = queue.front().get();
		
		if (!front.active())
			continue;
		
		children = front.children();
		
		for (auto&& child : children) {
			if (!child.active())
				continue;
			
			find_components<T>(&child, result);
		}
		
		queue.insert(queue.end(), children.begin(), children.end());
		queue.pop_front();
	}
}

} // namespace cobalt

#endif // COBALT_OBJECT_HPP_INCLUDED
