#ifndef COBALT_ACTOR_HPP_INCLUDED
#define COBALT_ACTOR_HPP_INCLUDED

#pragma once

// Classes in this file:
//     actor_component
//     transform_component
//     actor
//     level
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

#include <cobalt/utility/identifier.hpp>
#include <cobalt/utility/type_index.hpp>
#include <cobalt/utility/intrusive.hpp>

#include <boost/utility/string_view.hpp>

#include <type_traits>
#include <deque>

#define IMPLEMENT_OBJECT_TYPE(ThisClass) \
public: \
	static const type_index& static_type() noexcept { static auto type = type_id<ThisClass>(); return type; } \
	virtual const type_index& type() const noexcept override { return static_type(); }

namespace cobalt {

/// Object
class object : public ref_counter<object> {
public:
	virtual ~object() = default;
	
	virtual const type_index& type() const noexcept = 0;
};

struct generic_list_tag { };

class actor;
class transform_component;

/// Actor abstract component
class actor_component : public object, public intrusive_list_base<generic_list_tag> {
public:
	virtual actor* actor() const noexcept;
	
	void attach_to(transform_component* parent) noexcept;
	void detach() noexcept;
	
	transform_component* parent() const noexcept { return _parent; }
	
	const identifier& name() const noexcept { return _name; }
	void name(const identifier& name) noexcept { _name = name; }
	
private:
	friend class transform_component;

private:
	transform_component* _parent = nullptr;
	identifier _name;
};

/// Transform and hierarchy component
class transform_component : public actor_component {
	IMPLEMENT_OBJECT_TYPE(transform_component)
public:
	using children_type = intrusive_list<actor_component, generic_list_tag>;
	
	~transform_component() {
		while (!_children.empty())
			_children.back().detach();
	}
	
	virtual class actor* actor() const noexcept override { return _actor; }
	
private:
	friend class actor_component;
	
	void add_child(actor_component* component) noexcept {
		BOOST_ASSERT(!!component);
		if (!component->is_linked())
			retain(component);
		_children.push_back(*component);
		component->_parent = this;
	}
	
	void remove_child(actor_component* component) noexcept {
		BOOST_ASSERT(!!component);
		_children.erase_and_dispose(_children.iterator_to(*component), [&](actor_component* c) {
			c->_parent = nullptr;
			release(c);
		});
	}
	
private:
	friend class actor;
	
	class actor* _actor = nullptr;
	children_type _children;
};

class level;

/// Actor is a container for components
class actor	: public object, public intrusive_list_base<generic_list_tag> {
	IMPLEMENT_OBJECT_TYPE(actor)
public:
	actor();
	
	actor(actor&&) = default;
	actor& operator=(actor&&) = default;

	actor(const actor&) = delete;
	actor& operator=(const actor&) = delete;
	
	const identifier& name() const noexcept { return _name; }
	void name(const identifier& name) noexcept { _name = name; }
	
	void attach_to(level* level) noexcept;
	void detach() noexcept;
	
	level* level() const noexcept { return _level; }
	
	transform_component* transform() const noexcept { return _transform.get(); }
	void transform(transform_component* transform) noexcept;

	void active(bool active) noexcept { _active = active; }
	bool active_self() const noexcept { return _active; }
	//bool active_in_hierarchy() const noexcept;

private:
	friend class level;
	
	identifier _name;
	class level* _level = nullptr;
	ref_ptr<transform_component> _transform;
	bool _active = true;
};

/// Level is a set of actors
class level : public object {
public:
	using actors_type = intrusive_list<actor, generic_list_tag>;
	
	~level() {
		_actors.clear_and_dispose([](actor* a) {
			a->_level = nullptr;
			release(a);
		});
	}
	
private:
	friend class actor;
	
	void add_actor(actor* actor) noexcept {
		BOOST_ASSERT(!!actor);
		if (!actor->is_linked())
			retain(actor);
		_actors.push_back(*actor);
		actor->_level = this;
	}
	
	void remove_actor(actor* actor) noexcept {
		BOOST_ASSERT(!!actor);
		_actors.erase_and_dispose(_actors.iterator_to(*actor), [&](class actor* a) {
			a->_level = nullptr;
			release(a);
		});
	}
	
private:
	actors_type _actors;
};

////////////////////////////////////////////////////////////////////////////////
// actor_component
//

inline class actor* actor_component::actor() const noexcept {
	for (auto p = parent(); p; p = p->parent()) {
		if (auto a = p->actor())
			return a;
	}
	return nullptr;
}

inline void actor_component::attach_to(transform_component* parent) noexcept {
	BOOST_ASSERT(!!parent);
	parent->add_child(this);
}

inline void actor_component::detach() noexcept {
	if (_parent)
		_parent->remove_child(this);
}
	
////////////////////////////////////////////////////////////////////////////////
// actor
//

inline actor::actor()
	: _name(type_id_runtime(*this).pretty_name())
	, _transform(make_ref<transform_component>())
{
	_transform->_actor = this;
}

inline void actor::attach_to(class level* level) noexcept {
	BOOST_ASSERT(!!level);
	level->add_actor(this);
}

inline void actor::detach() noexcept {
	if (_level)
		_level->remove_actor(this);
}

inline void actor::transform(transform_component* transform) noexcept {
	_transform = transform;
	if (_transform)
		_transform->_actor = this;
}

//inline bool actor::active_in_hierarchy() const noexcept {
//	for (auto o = this; o; o = o->parent()) {
//		if (!o->active())
//			return false;
//	}
//
//	return true;
//}

////////////////////////////////////////////////////////////////////////////////

//inline const actor* find_root(const actor* o) noexcept {
//	BOOST_ASSERT(o != nullptr);
//	if (!o) return nullptr;
//	
//	while (o->parent())
//		o = o->parent();	
//	
//	return o;
//}

//inline const actor* find_parent(const actor* o, const identifier& name) noexcept {
//	BOOST_ASSERT(o != nullptr);
//	if (!o) return nullptr;
//	
//	for (o = o->parent(); o; o = o->parent()) {
//		if (!o->active())
//			continue;
//		
//		if (o->name() == name)
//			return o;
//	}
//	
//	return nullptr;
//}

//inline const actor* find_child(const actor* o, const identifier& name) noexcept {
//	BOOST_ASSERT(o != nullptr);
//	if (!o) return nullptr;
//	
//	for (auto&& child : o->children()) {
//		if (!child.active())
//			continue;
//		
//		if (child.name() == name)
//			return &child;
//	}
//	
//	return nullptr;
//}

// Breadth-first search
//inline const actor* find_child_in_hierarchy(const actor* o, const identifier& name) noexcept {
//	BOOST_ASSERT(o != nullptr);
//	if (!o) return nullptr;
//	
//	if (auto found = find_child(o, name))
//		return found;
//	
//	for (auto&& child : o->children()) {
//		if (!child.active())
//			continue;
//		
//		if (auto found = find_child(&child, name))
//			return found;
//	}
//
//	std::deque<std::reference_wrapper<const actor>> queue(std::begin(o->children()), std::end(o->children()));
//	
//	while (!queue.empty()) {
//		auto&& front = queue.front().get();
//		
//		if (!front.active())
//			continue;
//		
//		for (auto&& child : front.children()) {
//			if (!child.active())
//				continue;
//			
//			if (auto found = find_child(&child, name))
//				return found;
//		}
//		
//		queue.insert(queue.end(), std::begin(front.children()), std::end(front.children()));
//		queue.pop_front();
//	}
//	
//	return nullptr;
//}

//inline const actor* find_object_with_path(const actor* o, const char* path) noexcept {
//	BOOST_ASSERT(o != nullptr);
//	BOOST_ASSERT(path != nullptr);
//	
//	if (!o || !path)
//		return nullptr;
//	
//	const char* b = path;
//	const char* e = b;
//	
//	auto current = (*e == '/' ? find_root(o) : o);
//	
//	while (*e == '/')
//		b = ++e;
//	
//	// Iterate through names in the path
//	while (*e++) {
//		if (*e == '/' || !*e) {
//			boost::string_view name(b, e - b);
//			bool found = false;
//			
//			// Compare child name with current path part
//			for (auto&& child : current->children()) {
//				if (!child.active())
//					continue;
//				
//				if (child.name() == name) {
//					current = &child;
//					found = true;
//					break;
//				}
//			}
//			
//			if (!found)
//				return nullptr;
//			
//			while (*e == '/')
//				b = ++e;
//		}
//	}
//	
//	return current;
//}

//inline const actor_component* find_component(const actor* o, type_index component_type) noexcept {
//	BOOST_ASSERT(o != nullptr);
//	if (!o) return nullptr;
//	
//	for (auto&& c : o->components()) {
//		if (c.type() == component_type)
//			return &c;
//	}
//
//	return nullptr;
//}

//inline const actor_component* find_component_in_parent(const actor* o, type_index component_type) noexcept {
//	BOOST_ASSERT(o != nullptr);
//	
//	for ( ; o; o = o->parent()) {
//		if (!o->active())
//			continue;
//		
//		if (auto c = find_component(o, component_type))
//			return c;
//	}
//
//	return nullptr;
//}

// Breadth-first search
//inline const actor_component* find_component_in_children(const actor* o, type_index component_type) {
//	BOOST_ASSERT(o != nullptr);
//	if (!o) return nullptr;
//	
//	if (auto c = find_component(o, component_type))
//		return c;
//	
//	for (auto&& child : o->children()) {
//		if (!child.active())
//			continue;
//		
//		if (auto c = find_component(&child, component_type))
//			return c;
//	}
//
//	std::deque<std::reference_wrapper<const actor>> queue(std::begin(o->children()), std::end(o->children()));
//	
//	while (!queue.empty()) {
//		auto&& front = queue.front().get();
//		
//		for (auto&& child : front.children()) {
//			if (!child.active())
//				continue;
//			
//			if (auto c = find_component(&child, component_type))
//				return c;
//		}
//		
//		queue.insert(queue.end(), std::begin(front.children()), std::end(front.children()));
//		queue.pop_front();
//	}
//
//	return nullptr;
//}

//template <typename T, typename = typename std::enable_if_t<std::is_base_of<actor_component, T>::value>>
//inline const T* find_component(const actor* o) noexcept {
//	return static_cast<const T*>(find_component(o, T::static_type()));
//}
//
//template <typename T, typename = typename std::enable_if_t<std::is_base_of<actor_component, T>::value>>
//inline const T* find_component_in_parent(const actor* o) noexcept {
//	return static_cast<const T*>(find_component_in_parent(o, T::static_type()));
//}
//
//template <typename T, typename = typename std::enable_if_t<std::is_base_of<actor_component, T>::value>>
//inline const T* find_component_in_children(const actor* o) noexcept {
//	return static_cast<const T*>(find_component_in_children(o, T::static_type()));
//}
//
//template <typename OutputIterator>
//inline void find_components(const actor* o, type_index component_type, OutputIterator result) {
//	for (auto&& c : o->components()) {
//		if (c.type() == component_type)
//			*result++ = &c;
//	}
//}
//
//template <typename OutputIterator>
//inline void find_components_in_parent(const actor* o, type_index component_type, OutputIterator result) {
//	BOOST_ASSERT(o != nullptr);
//	if (!o) return;
//	
//	for ( ; o; o = o->parent()) {
//		if (!o->active())
//			continue;
//		
//		find_components(o, component_type, result);
//	}
//}

// Breadth-first search
//template <typename OutputIterator>
//inline void find_components_in_children(const actor* o, type_index component_type, OutputIterator result) {
//	BOOST_ASSERT(o != nullptr);
//	if (!o) return;
//	
//	find_components(o, component_type, result);
//	
//	for (auto&& child : o->children()) {
//		if (!child.active())
//			continue;
//		
//		find_components(&child, component_type, result);
//	}
//
//	std::deque<std::reference_wrapper<const actor>> queue(std::begin(o->children()), std::end(o->children()));
//	
//	while (!queue.empty()) {
//		auto&& front = queue.front().get();
//		
//		if (!front.active())
//			continue;
//
//		for (auto&& child : front.children()) {
//			if (!child.active())
//				continue;
//			
//			find_components(&child, component_type, result);
//		}
//		
//		queue.insert(queue.end(), std::begin(front.children()), std::end(front.children()));
//		queue.pop_front();
//	}
//}

//template <typename T, typename OutputIterator, typename = typename std::enable_if_t<std::is_base_of<actor_component, T>::value>>
//inline void find_components(const actor* o, OutputIterator result) {
//	BOOST_ASSERT(o != nullptr);
//	if (!o) return;
//	
//	for (auto&& c : o->components()) {
//		if (c.type() == T::static_type())
//			*result++ = static_cast<const T*>(&c);
//	}
//}

//template <typename T, typename OutputIterator, typename = typename std::enable_if_t<std::is_base_of<actor_component, T>::value>>
//inline void find_components_in_parent(const actor* o, OutputIterator result) {
//	BOOST_ASSERT(o != nullptr);
//	if (!o) return;
//	
//	for ( ; o; o = o->parent()) {
//		if (!o->active())
//			continue;
//		
//		find_components<T>(o, result);
//	}
//}

// Breadth-first search
//template <typename T, typename OutputIterator, typename = typename std::enable_if_t<std::is_base_of<actor_component, T>::value>>
//inline void find_components_in_children(const actor* o, OutputIterator result) {
//	BOOST_ASSERT(o != nullptr);
//	if (!o) return;
//	
//	find_components<T>(o, result);
//	
//	for (auto&& child : o->children()) {
//		if (!child.active())
//			continue;
//		
//		find_components<T>(&child, result);
//	}
//
//	std::deque<std::reference_wrapper<const actor>> queue(std::begin(o->children()), std::end(o->children()));
//	
//	while (!queue.empty()) {
//		auto&& front = queue.front().get();
//		
//		if (!front.active())
//			continue;
//
//		for (auto&& child : front.children()) {
//			if (!child.active())
//				continue;
//			
//			find_components<T>(&child, result);
//		}
//		
//		queue.insert(queue.end(), std::begin(front.children()), std::end(front.children()));
//		queue.pop_front();
//	}
//}

} // namespace cobalt

#endif // COBALT_ACTOR_HPP_INCLUDED
