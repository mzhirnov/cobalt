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
//

#include <cobalt/utility/identifier.hpp>
#include <cobalt/utility/type_index.hpp>
#include <cobalt/utility/intrusive.hpp>
#include <cobalt/utility/factory.hpp>

#include <boost/utility/string_view.hpp>

#include <type_traits>
#include <deque>

#define IMPLEMENT_OBJECT_TYPE(ThisClass) \
private: \
	REGISTER_FACTORY_WITH_KEY(object_factory, ThisClass, ThisClass::static_type()) \
public: \
	static const type_index& static_type() noexcept { static auto type = type_id<ThisClass>(); return type; } \
	virtual const type_index& generic_type() const noexcept override { return static_type(); }

namespace cobalt {

///
/// Object
///
class object : public ref_counter<object> {
public:
	object() = default;
	
	object(const object&) = delete;
	object& operator=(const object&) = delete;
	
	virtual ~object() = default;
	
	virtual const type_index& generic_type() const noexcept = 0;
	
	const identifier& name() const noexcept { return _name; }
	void name(const identifier& name) noexcept { _name = name; }
	
	static object* create_instance(const type_index& type);
	template <class T> static T* create_instance();
	
protected:
	using object_factory = auto_factory<object(), type_index>;
	
private:
	identifier _name;
};

struct basic_list_tag { };

class actor;
class transform_component;

///
/// Actor component
///
class actor_component : public object, public intrusive_list_base<basic_list_tag> {
public:
	virtual class actor* actor() const noexcept { return _actor; }
	
private:
	friend class actor;
	class actor* _actor = nullptr;
};

///
/// Transform and hierarchy actor component
///
class transform_component : public actor_component {
	IMPLEMENT_OBJECT_TYPE(transform_component)

public:
	using children_type = intrusive_list<transform_component, basic_list_tag>;
	
	~transform_component();
	
	virtual class actor* actor() const noexcept override;
	
	transform_component* parent() const noexcept { return _parent; }
	children_type& children() noexcept { return _children; }
	
	void add_child(transform_component* child) noexcept;
	void remove_child(transform_component* child) noexcept;
	
	void clear_children() noexcept;
	
	void attach_to(transform_component* parent) noexcept;
	void detach_from_parent() noexcept;
	
private:
	transform_component* _parent = nullptr;
	children_type _children;
};

class level;

///
/// Actor is a container for components
///
class actor : public object, public intrusive_list_base<basic_list_tag> {
	IMPLEMENT_OBJECT_TYPE(actor)

public:
	using components_type = intrusive_list<actor_component, basic_list_tag>;
	
	~actor();
	
	transform_component* transform() const noexcept { return _transform.get(); }
	void transform(transform_component* transform) noexcept;
	
	const components_type& components() const noexcept { return _components; }
	
	void add_component(actor_component* component) noexcept;
	void remove_component(actor_component* component) noexcept;
	
	void clear_components() noexcept;
	
	actor_component* find_component_by_type(const type_index& type) noexcept;
	size_t find_components_by_type(const type_index& type, std::vector<actor_component*>& components) noexcept;
	
	template <typename T> T* find_component() noexcept;
	template <typename T, typename OutputIterator> size_t find_components(OutputIterator components) noexcept;
	
	level* level() const noexcept { return _level; }

	void attach_to(class level* level) noexcept;
	void detach_from_level() noexcept;
	
	void active(bool active) noexcept { _active = active; }
	bool active_self() const noexcept { return _active; }
	//bool active_in_hierarchy() const noexcept;
	
private:
	template <typename Handler> void traverse_components(Handler handler) noexcept;

private:
	friend class level;
	class level* _level = nullptr;
	ref_ptr<transform_component> _transform;
	components_type _components;
	bool _active = true;
};

///
/// Level is a set of actors
///
class level : public object {
	IMPLEMENT_OBJECT_TYPE(level)

public:
	using actors_type = intrusive_list<actor, basic_list_tag>;
	
	~level();
	
	const actors_type& actors() const noexcept { return _actors; }
	
	void add_actor(actor* actor) noexcept;
	void remove_actor(actor* actor) noexcept;
	
	void clear_actors() noexcept;
	
private:
	actors_type _actors;
};

////////////////////////////////////////////////////////////////////////////////
// object
//

inline object* object::create_instance(const type_index& type) {
	return object_factory::create(type);
}

template <class T>
inline T* object::create_instance() {
	static_assert(std::is_base_of<object, T>::value, "T is not derived from object");
	return static_cast<T*>(object_factory::create(T::static_type()));
}

////////////////////////////////////////////////////////////////////////////////
// actor_component
//

////////////////////////////////////////////////////////////////////////////////
// trasform_component
//

inline transform_component::~transform_component() {
	clear_children();
}

inline class actor* transform_component::actor() const noexcept {
	if (auto a = actor_component::actor())
		return a;
	
	for (auto p = parent(); p; p = p->parent()) {
		if (auto a = p->actor_component::actor())
			return a;
	}
	
	return nullptr;
}

inline void transform_component::add_child(transform_component* child) noexcept {
	BOOST_ASSERT(!!child);
	if (!child->is_linked())
		retain(child);
	_children.push_back(*child);
	child->_parent = this;
}

inline void transform_component::remove_child(transform_component* child) noexcept {
	BOOST_ASSERT(!!child);
	_children.erase_and_dispose(_children.iterator_to(*child), [](transform_component* child) {
		child->_parent = nullptr;
		release(child);
	});
}

inline void transform_component::clear_children() noexcept {
	_children.clear_and_dispose([](transform_component* child) {
		child->_parent = nullptr;
		release(child);
	});
}

inline void transform_component::attach_to(transform_component* parent) noexcept {
	BOOST_ASSERT(!!parent);
	parent->add_child(this);
}

inline void transform_component::detach_from_parent() noexcept {
	if (_parent)
		_parent->remove_child(this);
}
	
////////////////////////////////////////////////////////////////////////////////
// actor
//

inline actor::~actor() {
	clear_components();
}

inline void actor::add_component(actor_component* component) noexcept {
	BOOST_ASSERT(!!component);
	if (!component->is_linked())
		retain(component);
	_components.push_back(*component);
	component->_actor = this;
}

inline void actor::remove_component(actor_component* component) noexcept {
	BOOST_ASSERT(!!component);
	_components.erase_and_dispose(_components.iterator_to(*component), [](actor_component* c) {
		c->_actor = nullptr;
		release(c);
	});
}

inline void actor::clear_components() noexcept {
	_components.clear_and_dispose([](actor_component* component) {
		component->_actor = nullptr;
		release(component);
	});
}

template <typename Handler>
inline void actor::traverse_components(Handler handler) noexcept {
	struct helper {
		static bool traverse(transform_component* component, Handler handler) {
			if (!handler(component))
				return false;
			
			for (auto&& child : component->children()) {
				if (!traverse(&child, handler))
					return false;
			}
			
			return true;
		}
	};
	
	if (_transform && !helper::traverse(_transform.get(), handler))
		return;
	
	for (auto&& component : _components) {
		if (!handler(&component))
			break;
	}
}

inline actor_component* actor::find_component_by_type(const type_index& type) noexcept {
	actor_component* result = nullptr;
	
	traverse_components([&](actor_component* component) -> bool {
		if (component->generic_type() == type) {
			result = component;
			return false;
		}
		return true;
	});
	
	return result;
}

inline size_t actor::find_components_by_type(const type_index& type, std::vector<actor_component*>& components) noexcept {
	size_t initial_size = components.size();
	
	traverse_components([&](actor_component* component) -> bool {
		if (component->generic_type() == type)
			components.push_back(component);
		return true;
	});
	
	return components.size() - initial_size;
}

template <typename T>
T* actor::find_component() noexcept {
	return static_cast<T*>(find_component_by_type(T::static_type()));
}

template <typename T, typename OutputIterator>
size_t actor::find_components(OutputIterator components) noexcept {
	size_t count = 0;
	
	traverse_components([&](actor_component* component) -> bool {
		if (component->generic_type() == T::static_type()) {
			*components = static_cast<T*>(component);
			count++;
		}
		return true;
	});
	
	return count;
}

inline void actor::attach_to(class level* level) noexcept {
	BOOST_ASSERT(!!level);
	level->add_actor(this);
}

inline void actor::detach_from_level() noexcept {
	if (_level)
		_level->remove_actor(this);
}

inline void actor::transform(transform_component* transform) noexcept {
	if (_transform)
		_transform->_actor = nullptr;

	if ((_transform = transform))
		_transform->_actor = this;
}

////////////////////////////////////////////////////////////////////////////////
// level
//

inline level::~level() {
	clear_actors();
}

inline void level::add_actor(actor* actor) noexcept {
	BOOST_ASSERT(!!actor);
	if (!actor->is_linked())
		retain(actor);
	_actors.push_back(*actor);
	actor->_level = this;
}

inline void level::remove_actor(actor* actor) noexcept {
	BOOST_ASSERT(!!actor);
	_actors.erase_and_dispose(_actors.iterator_to(*actor), [](class actor* actor) {
		actor->_level = nullptr;
		release(actor);
	});
}

inline void level::clear_actors() noexcept {
	_actors.clear_and_dispose([](actor* actor) {
		actor->_level = nullptr;
		release(actor);
	});
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
