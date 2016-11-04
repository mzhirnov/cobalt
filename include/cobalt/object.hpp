#ifndef COBALT_OBJECT_FWD_HPP_INCLUDED
#define COBALT_OBJECT_FWD_HPP_INCLUDED

#include <cobalt/object_fwd.hpp>

namespace cobalt {

inline bool object::active_in_hierarchy() const {
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
	o->parent(this);
	return boost::get_pointer(_children.back());
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

inline object* object::find_root_object() const {
	for (auto parent = _parent; parent; parent = parent->parent()) {
		if (!parent->parent())
			return parent;
	}
	return nullptr;
}

inline object* object::find_object(const char* name) {
	if (!name)
		return nullptr;
	
	const char* b = name;
	const char* e = b;
	
	while (*e == '/')
		b = ++e;
	
	auto o = this;
	
	// Iterate through names in the path
	while (*e++) {
		if (*e == '/' || *e == '\0') {
			bool found = false;
			for (auto&& c : o->_children) {
				if (std::equal(b, e, c->_name.c_str(), c->_name.c_str() + c->_name.size())) {
					o = c.get();
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
	
	return o;
}

inline object* object::find_object_in_parent(const char* name) const {
	for (auto parent = _parent; parent; parent = parent->parent()) {
		auto o = parent->find_object(name);
		if (o)
			return o;
	}
	
	return nullptr;
}

inline object* object::find_object_in_children(const char* name) const {
	// Breadth-first search
	for (auto&& child : _children) {
		auto o = child->find_object(name);
		if (o)
			return o;
		
	}
	
	for (auto&& child : _children) {
		auto o = child->find_object_in_children(name);
		if (o)
			return o;
	}
	
	return nullptr;
}

inline component* object::add_component(const type_index& type, const ref_ptr<component>& c) {
	_components.emplace_back(type, c);
	c->set_object(this);
	return boost::get_pointer(_components.back().second);
}

inline bool object::remove_component(component* c) {
	auto it = std::remove_if(_components.begin(), _components.end(), [&c](const Components::value_type& value) {
		if (boost::get_pointer(value.second) == c) {
			c->set_object(nullptr);
			return true;
		}
		return false;
	});

	if (it != _components.end()) {
		_components.erase(it, _components.end());
		return true;
	}

	return false;
}

inline void object::remove_components(const type_index& type) {
	auto it = std::remove_if(_components.begin(), _components.end(), [&type](const Components::value_type& value) {
		if (value.first == type) {
			value.second->set_object(nullptr);
			return true;
		}
		return false;
	});

	_components.erase(it, _components.end());
}

inline component* object::get_component(const type_index& type) const {
	for (auto&& pair : _components) {
		if (pair.first == type)
			return boost::get_pointer(pair.second);
	}

	return nullptr;
}

inline component* object::get_component_in_parent(const type_index& type) const {
	for (auto parent = _parent; parent; parent = parent->parent()) {
		auto c = parent->get_component(type);
		if (c)
			return c;
	}

	// No parent or no components in parent found
	return nullptr;
}

inline component* object::get_component_in_children(const type_index& type) const {
	// Use recursion here
	for (auto&& child : _children) {
		// Look in children first
		auto c = child->get_component_in_children(type);
		if (!c)
			c = child->get_component(type);
		// Return if something found in the child
		if (c)
			return c;
	}

	// No children or no components in children found
	return nullptr;
}

inline void object::get_components(const type_index& type, std::vector<ref_ptr<component>>& components) const {
	for (auto&& pair : _components) {
		if (pair.first == type)
			components.push_back(pair.second);
	}
}

inline void object::get_components_in_parent(const type_index& type, std::vector<ref_ptr<component>>& components) const {
	for (auto parent = _parent; parent; parent = parent->parent())
		parent->get_components(type, components);
}

inline void object::get_components_in_children(const type_index& type, std::vector<ref_ptr<component>>& components) const {
	// Use recursion here
	for (auto&& child : _children) {
		// Look in children first
		child->get_components_in_children(type, components);
		child->get_components(type, components);
	}
}

} // namespace cobalt

#endif // COBALT_OBJECT_FWD_HPP_INCLUDED
