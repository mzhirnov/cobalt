#ifndef COBALT_OBJECT_FWD_HPP_INCLUDED
#define COBALT_OBJECT_FWD_HPP_INCLUDED

#include <cobalt/object_fwd.hpp>

#include <deque>

namespace cobalt {
	
void component::remove_from_parent() {
	if (_object)
		_object->remove_component(this);
}

inline bool object::active_in_hierarchy() const noexcept {
	if (!_active)
		return false;

	for (auto parent = _parent; parent; parent = parent->parent()) {
		if (!parent->active())
			return false;
	}

	return true;
}

inline object* object::add_child(const ref_ptr<object>& o) {
	BOOST_ASSERT(std::find(_children.begin(), _children.end(), o) == _children.end());
	_children.push_back(o);
	_children.back()->parent(this);
	return _children.back().get();
}
	
object* object::add_child(ref_ptr<object>&& o) {
	BOOST_ASSERT(std::find(_children.begin(), _children.end(), o) == _children.end());
	_children.push_back(std::move(o));
	_children.back()->parent(this);
	return _children.back().get();
}

inline ref_ptr<object> object::remove_child(object* o) {
	auto it = std::remove(_children.begin(), _children.end(), o);
	if (it != _children.end()) {
		ref_ptr<object> ret = std::move(*it);
		_children.erase(it, _children.end());
		ret->parent(nullptr);
		return ret;
	}
	return nullptr;
}

inline void object::remove_from_parent() {
	if (_parent)
		_parent->remove_child(this);
}

inline object* object::find_root() const noexcept {
	for (auto p = _parent; p; p = p->parent()) {
		if (!p->parent())
			return p;
	}
	return nullptr;
}

inline object* object::find_child(const char* name) const noexcept {
	if (!name)
		return nullptr;
	
	const char* b = name;
	const char* e = b;
	
	while (*e == '/')
		b = ++e;
	
	auto current = const_cast<object*>(this);
	
	// Iterate through names in the path
	while (*e++) {
		if (*e == '/' || !*e) {
			bool found = false;
			
			// Compare child name with current path part
			for (auto&& child : current->_children) {
				if (std::equal(b, e, child->name(), child->name() + child->_name.size())) {
					current = const_cast<object*>(child.get());
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

inline object* object::find_ancestor(const char* name) const noexcept {
	for (auto p = _parent; p; p = p->parent()) {
		if (!std::strcmp(p->name(), name))
			return p;
	}
	
	return nullptr;
}

inline object* object::find_descendant(const char* name) const noexcept {
	// Breadth-first search
	
	if (auto o = find_child(name))
		return o;
	
	for (auto&& child : _children) {
		auto desc = child->find_descendant(name);
		if (desc)
			return desc;
	}
	
	return nullptr;
}

inline component* object::add_component(const ref_ptr<component>& c) {
	_components.emplace_back(c->type(), c);
	c->object(this);
	return c.get();
}
	
inline component* object::add_component(ref_ptr<component>&& c) {
	auto type = c->type();
	_components.emplace_back(type, std::move(c));
	_components.back().second->object(this);
	return _components.back().second.get();
}

inline ref_ptr<component> object::remove_component(component* c) {
	auto it = std::remove_if(_components.begin(), _components.end(), [&](auto&& value) {
		if (value.second.get() == c) {
			value.second->object(nullptr);
			return true;
		}
		return false;
	});

	if (it != _components.end()) {
		ref_ptr<component> ret = std::move((*it).second);
		_components.erase(it, _components.end());
		return ret;
	}

	return nullptr;
}

inline size_t object::remove_components(hash_type component_type) {
	auto it = std::remove_if(_components.begin(), _components.end(), [&](auto&& value) {
		if (value.first == component_type) {
			value.second->object(nullptr);
			return true;
		}
		return false;
	});
	
	size_t count = std::distance(it, _components.end());

	if (it != _components.end())
		_components.erase(it, _components.end());
	
	return count;
}

inline component* object::find_component(hash_type component_type) const noexcept {
	for (auto&& pair : _components) {
		if (pair.first == component_type)
			return pair.second.get();
	}

	return nullptr;
}

inline component* object::find_component_in_parent(hash_type component_type) const noexcept {
	for (auto&& p = this; p; p = p->parent()) {
		if (auto c = p->find_component(component_type))
			return c;
	}

	return nullptr;
}

inline component* object::find_component_in_children(hash_type component_type) const {
	// Breadth-first search
	
	if (auto c = find_component(component_type))
		return c;
	
	for (auto&& child : _children) {
		if (auto c = child->find_component(component_type))
			return c;
	}
	
	std::deque<std::reference_wrapper<const ref_ptr<object>>> queue(_children.begin(), _children.end());
	
	while (!queue.empty()) {
		auto&& o = queue.front().get();
		
		for (auto&& child : o->_children) {
			if (auto c = child->find_component(component_type))
				return c;
		}
		
		queue.insert(queue.end(), o->_children.begin(), o->_children.end());
		queue.pop_front();
	}

	return nullptr;
}

template <typename OutputIterator>
inline void object::find_components(hash_type component_type, OutputIterator result) const {
	for (auto&& pair : _components) {
		if (pair.first == component_type)
			*result++ = pair.second.get();
	}
}

template <typename OutputIterator>
inline void object::find_components_in_parent(hash_type component_type, OutputIterator result) const {
	for (auto&& p = this; p; p = p->parent())
		p->find_components(component_type, result);
}

template <typename OutputIterator>
inline void object::find_components_in_children(hash_type component_type, OutputIterator result) const {
	// Breadth-first search
	
	find_components(component_type, result);
	
	for (auto&& child : _children)
		child->find_components(component_type, result);
	
	std::deque<std::reference_wrapper<const ref_ptr<object>>> queue(_children.begin(), _children.end());
	
	while (!queue.empty()) {
		auto&& o = queue.front().get();
		
		for (auto&& child : o->_children)
			child->find_components(component_type, result);
		
		queue.insert(queue.end(), o->_children.begin(), o->_children.end());
		queue.pop_front();
	}
}

template <typename T, typename OutputIterator, typename>
inline void object::find_components(OutputIterator result) const {
	for (auto&& pair : _components) {
		if (pair.first == T::component_type)
			*result++ = static_cast<T*>(pair.second.get());
	}
}
	
template <typename T, typename OutputIterator, typename>
inline void object::find_components_in_parent(OutputIterator result) const {
	for (auto&& p = this; p; p = p->parent())
		p->find_components<T>(result);
}
	
template <typename T, typename OutputIterator, typename>
inline void object::find_components_in_children(OutputIterator result) const {
	// Breadth-first search
	
	find_components<T>(result);
	
	for (auto&& child : _children)
		child->find_components<T>(result);
	
	std::deque<std::reference_wrapper<const ref_ptr<object>>> queue(_children.begin(), _children.end());
	
	while (!queue.empty()) {
		auto&& o = queue.front().get();
		
		for (auto&& child : o->_children)
			child->find_components<T>(result);
		
		queue.insert(queue.end(), o->_children.begin(), o->_children.end());
		queue.pop_front();
	}
}

} // namespace cobalt

#endif // COBALT_OBJECT_FWD_HPP_INCLUDED
