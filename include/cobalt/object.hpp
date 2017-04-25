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

template <typename T, typename>
inline const T* find_component(const object* o) noexcept {
	return static_cast<const T*>(find_component(o, T::component_type));
}

template <typename T, typename>
inline const T* find_component_in_parent(const object* o) noexcept {
	return static_cast<const T*>(find_component_in_parent(o, T::component_type));
}

template <typename T, typename>
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

template <typename T, typename OutputIterator, typename>
inline void find_components(const object* o, OutputIterator result) {
	BOOST_ASSERT(o != nullptr);
	if (!o) return;
	
	for (auto&& c : o->components()) {
		if (c.type() == T::component_type)
			*result++ = static_cast<const T*>(&c);
	}
}

template <typename T, typename OutputIterator, typename>
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
template <typename T, typename OutputIterator, typename>
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
