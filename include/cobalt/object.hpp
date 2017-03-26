#ifndef COBALT_OBJECT_HPP_INCLUDED
#define COBALT_OBJECT_HPP_INCLUDED

#include <cobalt/object_fwd.hpp>

#include <boost/utility/string_view.hpp>

#include <deque>

namespace cobalt {
	
////////////////////////////////////////////////////////////////////////////////
// component
//
	
inline void component::detach() noexcept {
	if (_object)
		_object->detach(this);
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

inline object* object::attach(object* o) noexcept {
	// constructor will call initial add_ref
	counted_ptr<object> sp = o;
	
	o->parent(this);
	_children.push_front(*o);
	
	return sp.detach();
}

inline counted_ptr<object> object::detach(object* o) noexcept {
	counted_ptr<object> sp;
	
	if (!o)
		return sp;
	
	for (auto it = _children.before_begin(), next = std::next(it); next != _children.end(); ++it, ++next) {
		if (std::addressof(*next) == o) {
			_children.erase_after_and_dispose(it, [&](auto&& o) { sp.reset(o, false); sp->parent(nullptr); });
			break;
		}
	}

	return sp;
	// destructor will call final release
}

inline void object::detach() noexcept {
	if (_parent)
		_parent->detach(this);
}
	
inline void object::remove_all_children() noexcept {
	_children.clear_and_dispose([](auto&& o) { o->parent(nullptr); release(o); });
}

inline const object* object::find_root() const noexcept {
	auto o = this;
	
	while (o->parent())
		o = o->parent();
	
	return o;
}
	
inline const object* object::find_object(const identifier& id) const noexcept {
	for (auto&& child : _children) {
		if (!child.active())
			continue;
		
		if (child.id() == id)
			return &child;
	}
	
	return nullptr;
}

inline const object* object::find_object_in_parent(const identifier& id) const noexcept {
	for (auto o = _parent; o; o = o->parent()) {
		if (!o->active())
			continue;
		
		if (o->id() == id)
			return o;
	}
	
	return nullptr;
}

inline const object* object::find_object_in_children(const identifier& id) const noexcept {
	// Breadth-first search
	
	if (auto o = find_object(id))
		return o;
	
	for (auto&& child : _children) {
		if (!child.active())
			continue;
		
		if (auto o = child.find_object(id))
			return o;
	}
	
	std::deque<std::reference_wrapper<const object>> queue(_children.begin(), _children.end());
	
	while (!queue.empty()) {
		auto&& o = queue.front().get();
		
		if (!o.active())
			continue;
		
		for (auto&& child : o._children) {
			if (!child.active())
				continue;
			
			if (auto o = child.find_object(id))
				return o;
		}
		
		queue.insert(queue.end(), o._children.begin(), o._children.end());
		queue.pop_front();
	}
	
	return nullptr;
}

inline const object* object::find_object_with_path(const char* path) const noexcept {
	if (!path)
		return nullptr;
	
	const char* b = path;
	const char* e = b;
	
	while (*e == '/')
		b = ++e;
	
	auto current = this;
	
	// Iterate through names in the path
	while (*e++) {
		if (*e == '/' || !*e) {
			boost::string_view id(b, e - b);
			bool found = false;
			
			// Compare child name with current path part
			for (auto&& child : current->_children) {
				if (!child.active())
					continue;
				
				if (child.id() == id) {
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

inline component* object::attach(component* c) noexcept {
	// constructor will call initial add_ref
	counted_ptr<component> sp = c;
	
	c->object(this);
	_components.push_front(*c);
	
	return sp.detach();
}

inline counted_ptr<component> object::detach(component* c) noexcept {
	counted_ptr<component> sp;
	
	if (!c)
		return sp;
	
	for (auto it = _components.before_begin(), next = std::next(it); next != _components.end(); ++it, ++next) {
		if (&*next == c) {
			_components.erase_after_and_dispose(it, [&](auto&& c) { sp.reset(c, false); sp->object(nullptr); });
			break;
		}
	}
	
	return sp;
	// destructor will call final release
}

inline size_t object::remove_components(hash_type component_type) noexcept {
	size_t count = 0;
	
	_components.remove_and_dispose_if([&](auto&& c) { return c.type() == component_type; },
									  [&](auto&& c) { c->object(nullptr); release(c); ++count; });
	
	return count;
}
	
inline void object::remove_all_components() noexcept {
	_components.clear_and_dispose([](auto&& c) { c->object(nullptr); release(c); });
}

inline const component* object::find_component(hash_type component_type) const noexcept {
	for (auto&& c : _components) {
		if (c.type() == component_type)
			return &c;
	}

	return nullptr;
}

inline const component* object::find_component_in_parent(hash_type component_type) const noexcept {
	for (auto&& o = this; o; o = o->parent()) {
		if (!o->active())
			continue;
		
		if (auto c = o->find_component(component_type))
			return c;
	}

	return nullptr;
}

inline const component* object::find_component_in_children(hash_type component_type) const {
	// Breadth-first search
	
	if (auto c = find_component(component_type))
		return c;
	
	for (auto&& child : _children) {
		if (!child.active())
			continue;
		
		if (auto c = child.find_component(component_type))
			return c;
	}
	
	std::deque<std::reference_wrapper<const object>> queue(_children.begin(), _children.end());
	
	while (!queue.empty()) {
		auto&& o = queue.front().get();
		
		for (auto&& child : o._children) {
			if (!child.active())
				continue;
			
			if (auto c = child.find_component(component_type))
				return c;
		}
		
		queue.insert(queue.end(), o._children.begin(), o._children.end());
		queue.pop_front();
	}

	return nullptr;
}

template <typename OutputIterator>
inline void object::find_components(hash_type component_type, OutputIterator result) const {
	for (auto&& c : _components) {
		if (c.type() == component_type)
			*result++ = &c;
	}
}

template <typename OutputIterator>
inline void object::find_components_in_parent(hash_type component_type, OutputIterator result) const {
	for (auto&& o = this; o; o = o->parent()) {
		if (!o->active())
			continue;
		
		o->find_components(component_type, result);
	}
}

template <typename OutputIterator>
inline void object::find_components_in_children(hash_type component_type, OutputIterator result) const {
	// Breadth-first search
	
	find_components(component_type, result);
	
	for (auto&& child : _children) {
		if (!child.active())
			continue;
		
		child.find_components(component_type, result);
	}
	
	std::deque<std::reference_wrapper<const object>> queue(_children.begin(), _children.end());
	
	while (!queue.empty()) {
		auto&& o = queue.front().get();
		
		if (!o.active())
			continue;
		
		for (auto&& child : o._children) {
			if (!child.active())
				continue;
			
			child.find_components(component_type, result);
		}
		
		queue.insert(queue.end(), o._children.begin(), o._children.end());
		queue.pop_front();
	}
}

template <typename T, typename OutputIterator, typename>
inline void object::find_components(OutputIterator result) const {
	for (auto&& c : _components) {
		if (c.type() == T::component_type)
			*result++ = static_cast<const T*>(&c);
	}
}
	
template <typename T, typename OutputIterator, typename>
inline void object::find_components_in_parent(OutputIterator result) const {
	for (auto&& o = this; o; o = o->parent()) {
		if (!o->active())
			continue;
		
		o->find_components<T>(result);
	}
}
	
template <typename T, typename OutputIterator, typename>
inline void object::find_components_in_children(OutputIterator result) const {
	// Breadth-first search
	
	find_components<T>(result);
	
	for (auto&& child : _children) {
		if (!child.active())
			continue;
		
		child.find_components<T>(result);
	}
	
	std::deque<std::reference_wrapper<const object>> queue(_children.begin(), _children.end());
	
	while (!queue.empty()) {
		auto&& o = queue.front().get();
		
		if (!o.active())
			continue;
		
		for (auto&& child : o._children) {
			if (!child.active())
				continue;
			
			child.find_components<T>(result);
		}
		
		queue.insert(queue.end(), o._children.begin(), o._children.end());
		queue.pop_front();
	}
}

} // namespace cobalt

#endif // COBALT_OBJECT_HPP_INCLUDED
