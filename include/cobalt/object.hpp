#ifndef COBALT_ACTOR_HPP_INCLUDED
#define COBALT_ACTOR_HPP_INCLUDED

#pragma once

// Classes in this file:
//     actor_component
//     basic_actor_component
//     actor
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
#include <cobalt/utility/hash.hpp>

#include <boost/utility/string_view.hpp>

#include <type_traits>
#include <deque>

namespace cobalt {

struct actor_components_tag {};
struct actor_children_tag {};

class actor;

/// Actor component
class actor_component
	: public ref_counter<actor_component>
	, public intrusive_single_list_base<actor_components_tag>
{
public:
	virtual ~actor_component() = default;

	virtual uint32_t type() const noexcept = 0;

	class actor* actor() const noexcept { return _actor; }
	
	void detach() noexcept;
	
private:
	friend class actor;
	
	void actor(class actor* actor) noexcept { _actor = actor; }

private:
	class actor* _actor = nullptr;
};

/// Base class for components
template <uint32_t ComponentType>
class basic_actor_component : public actor_component {
public:
	static constexpr uint32_t component_type = ComponentType;
	
	virtual uint32_t type() const noexcept override { return ComponentType; }
};

/// Actor is a container for components
class actor
	: public ref_counter<actor>
	, public intrusive_single_list_base<actor_children_tag>
{
public:
	using children_type = intrusive_single_list<actor, actor_children_tag>;
	using components_type = intrusive_single_list<actor_component, actor_components_tag>;
	
	actor() = default;
	
	actor(actor&&) = default;
	actor& operator=(actor&&) = default;

	actor(const actor&) = delete;
	actor& operator=(const actor&) = delete;
	
	explicit actor(const identifier& name) noexcept : _name(name) {}
	
	~actor();
	
	actor* parent() const noexcept { return _parent; }
	
	const children_type& children() const noexcept { return _children; }
	const components_type& components() const noexcept { return _components; }

	const identifier& name() const noexcept { return _name; }
	void name(const identifier& name) noexcept { _name = name; }

	bool active() const noexcept { return _active; }
	void active(bool active) noexcept { _active = active; }
	bool active_in_hierarchy() const noexcept;
	
	actor* add_child(actor* o) noexcept;
	ref_ptr<actor> remove_child(actor* o) noexcept;
	
	void detach() noexcept;
	
	void remove_all_children() noexcept;

	actor_component* add_component(actor_component* c) noexcept;
	ref_ptr<actor_component> remove_component(actor_component* c) noexcept;
	
	size_t remove_components(uint32_t component_type) noexcept;
	void remove_all_components() noexcept;

private:
	void parent(actor* parent) noexcept { _parent = parent; }

private:
	actor* _parent = nullptr;
	children_type _children;
	components_type _components;	
	identifier _name;
	bool _active = true;
};
	
////////////////////////////////////////////////////////////////////////////////
// component
//
	
inline void actor_component::detach() noexcept {
	if (_actor)
		_actor->remove_component(this);
}
	
////////////////////////////////////////////////////////////////////////////////
// object
//

inline actor::~actor() {
	remove_all_components();
	remove_all_children();
}

inline bool actor::active_in_hierarchy() const noexcept {
	for (auto o = this; o; o = o->parent()) {
		if (!o->active())
			return false;
	}

	return true;
}

inline actor* actor::add_child(actor* o) noexcept {
	BOOST_ASSERT(o != nullptr);
	if (!o) return nullptr;
	
	retain(o);
	
	_children.push_front(*o);
	
	o->parent(this);
	
	return o;
}

inline ref_ptr<actor> actor::remove_child(actor* o) noexcept {
	ref_ptr<actor> sp;
	
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

inline void actor::detach() noexcept {
	if (_parent)
		_parent->remove_child(this);
}
	
inline void actor::remove_all_children() noexcept {
	_children.clear_and_dispose([](auto&& o) {
		o->parent(nullptr);
		release(o);
	});
}

inline actor_component* actor::add_component(actor_component* c) noexcept {
	retain(c);
	
	_components.push_front(*c);
	
	c->actor(this);
	
	return c;
}

inline ref_ptr<actor_component> actor::remove_component(actor_component* c) noexcept {
	ref_ptr<actor_component> sp;
	
	if (!c)
		return sp;
	
	for (auto it = _components.before_begin(), next = std::next(it); next != _components.end(); ++it, ++next) {
		if (&*next == c) {
			_components.erase_after_and_dispose(it, [&](auto&& c) {
				// Don't increase ref_counter, transfer ownership to smart pointer
				sp.reset(c, false);
				sp->actor(nullptr);
			});
			break;
		}
	}
	
	return sp;
}

inline size_t actor::remove_components(uint32_t component_type) noexcept {
	size_t count = 0;
	
	_components.remove_and_dispose_if(
		[&](auto&& c) { return c.type() == component_type; },
		[&](auto&& c) { c->actor(nullptr); release(c); ++count; }
	);
	
	return count;
}
	
inline void actor::remove_all_components() noexcept {
	_components.clear_and_dispose([](auto&& c) {
		c->actor(nullptr);
		release(c);
	});
}

////////////////////////////////////////////////////////////////////////////////

inline const actor* find_root(const actor* o) noexcept {
	BOOST_ASSERT(o != nullptr);
	if (!o) return nullptr;
	
	while (o->parent())
		o = o->parent();	
	
	return o;
}

inline const actor* find_parent(const actor* o, const identifier& name) noexcept {
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

inline const actor* find_child(const actor* o, const identifier& name) noexcept {
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
inline const actor* find_child_in_hierarchy(const actor* o, const identifier& name) noexcept {
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

	std::deque<std::reference_wrapper<const actor>> queue(std::begin(o->children()), std::end(o->children()));
	
	while (!queue.empty()) {
		auto&& front = queue.front().get();
		
		if (!front.active())
			continue;
		
		for (auto&& child : front.children()) {
			if (!child.active())
				continue;
			
			if (auto found = find_child(&child, name))
				return found;
		}
		
		queue.insert(queue.end(), std::begin(front.children()), std::end(front.children()));
		queue.pop_front();
	}
	
	return nullptr;
}

inline const actor* find_object_with_path(const actor* o, const char* path) noexcept {
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

inline const actor_component* find_component(const actor* o, uint32_t component_type) noexcept {
	BOOST_ASSERT(o != nullptr);
	if (!o) return nullptr;
	
	for (auto&& c : o->components()) {
		if (c.type() == component_type)
			return &c;
	}

	return nullptr;
}

inline const actor_component* find_component_in_parent(const actor* o, uint32_t component_type) noexcept {
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
inline const actor_component* find_component_in_children(const actor* o, uint32_t component_type) {
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

	std::deque<std::reference_wrapper<const actor>> queue(std::begin(o->children()), std::end(o->children()));
	
	while (!queue.empty()) {
		auto&& front = queue.front().get();
		
		for (auto&& child : front.children()) {
			if (!child.active())
				continue;
			
			if (auto c = find_component(&child, component_type))
				return c;
		}
		
		queue.insert(queue.end(), std::begin(front.children()), std::end(front.children()));
		queue.pop_front();
	}

	return nullptr;
}

inline const actor_component* find_component(const actor* o, const char* name) noexcept {
	return find_component(o, murmur3(name, 0));
}

inline const actor_component* find_component_in_parent(const actor* o, const char* name) noexcept {
	return find_component_in_parent(o, murmur3(name, 0));
}

inline const actor_component* find_component_in_children(const actor* o, const char* name) {
	return find_component_in_children(o, murmur3(name, 0));
}

template <typename T, typename = typename std::enable_if_t<std::is_base_of<actor_component, T>::value>>
inline const T* find_component(const actor* o) noexcept {
	return static_cast<const T*>(find_component(o, T::component_type));
}

template <typename T, typename = typename std::enable_if_t<std::is_base_of<actor_component, T>::value>>
inline const T* find_component_in_parent(const actor* o) noexcept {
	return static_cast<const T*>(find_component_in_parent(o, T::component_type));
}

template <typename T, typename = typename std::enable_if_t<std::is_base_of<actor_component, T>::value>>
inline const T* find_component_in_children(const actor* o) noexcept {
	return static_cast<const T*>(find_component_in_children(o, T::component_type));
}

template <typename OutputIterator>
inline void find_components(const actor* o, uint32_t component_type, OutputIterator result) {
	for (auto&& c : o->components()) {
		if (c.type() == component_type)
			*result++ = &c;
	}
}

template <typename OutputIterator>
inline void find_components_in_parent(const actor* o, uint32_t component_type, OutputIterator result) {
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
inline void find_components_in_children(const actor* o, uint32_t component_type, OutputIterator result) {
	BOOST_ASSERT(o != nullptr);
	if (!o) return;
	
	find_components(o, component_type, result);
	
	for (auto&& child : o->children()) {
		if (!child.active())
			continue;
		
		find_components(&child, component_type, result);
	}

	std::deque<std::reference_wrapper<const actor>> queue(std::begin(o->children()), std::end(o->children()));
	
	while (!queue.empty()) {
		auto&& front = queue.front().get();
		
		if (!front.active())
			continue;

		for (auto&& child : front.children()) {
			if (!child.active())
				continue;
			
			find_components(&child, component_type, result);
		}
		
		queue.insert(queue.end(), std::begin(front.children()), std::end(front.children()));
		queue.pop_front();
	}
}

template <typename T, typename OutputIterator, typename = typename std::enable_if_t<std::is_base_of<actor_component, T>::value>>
inline void find_components(const actor* o, OutputIterator result) {
	BOOST_ASSERT(o != nullptr);
	if (!o) return;
	
	for (auto&& c : o->components()) {
		if (c.type() == T::component_type)
			*result++ = static_cast<const T*>(&c);
	}
}

template <typename T, typename OutputIterator, typename = typename std::enable_if_t<std::is_base_of<actor_component, T>::value>>
inline void find_components_in_parent(const actor* o, OutputIterator result) {
	BOOST_ASSERT(o != nullptr);
	if (!o) return;
	
	for ( ; o; o = o->parent()) {
		if (!o->active())
			continue;
		
		find_components<T>(o, result);
	}
}

// Breadth-first search
template <typename T, typename OutputIterator, typename = typename std::enable_if_t<std::is_base_of<actor_component, T>::value>>
inline void find_components_in_children(const actor* o, OutputIterator result) {
	BOOST_ASSERT(o != nullptr);
	if (!o) return;
	
	find_components<T>(o, result);
	
	for (auto&& child : o->children()) {
		if (!child.active())
			continue;
		
		find_components<T>(&child, result);
	}

	std::deque<std::reference_wrapper<const actor>> queue(std::begin(o->children()), std::end(o->children()));
	
	while (!queue.empty()) {
		auto&& front = queue.front().get();
		
		if (!front.active())
			continue;

		for (auto&& child : front.children()) {
			if (!child.active())
				continue;
			
			find_components<T>(&child, result);
		}
		
		queue.insert(queue.end(), std::begin(front.children()), std::end(front.children()));
		queue.pop_front();
	}
}

} // namespace cobalt

#endif // COBALT_ACTOR_HPP_INCLUDED
