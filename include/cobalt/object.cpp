#include "pch.h"
#include "cool/object.hpp"

bool object::active_in_hierarchy() const {
	if (!_active)
		return false;

	for (const object* parent = _parent; parent; parent = parent->parent()) {
		if (!parent->active_self())
			return false;
	}

	return true;
}

component* object::add_component(const boost::typeindex::type_index& type, const component_ptr& c) {
	_components.emplace_back(type, c);
	c->set_object(this);
	return boost::get_pointer(_components.back().second);
}

bool object::remove_component(component* c) {
	auto it = std::remove_if(_components.begin(), _components.end(), [&c](const Components::value_type& value)->bool {
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

void object::remove_components(const boost::typeindex::type_index& type) {
	auto it = std::remove_if(_components.begin(), _components.end(), [&type](const Components::value_type& value)->bool {
		if (value.first == type) {
			value.second->set_object(nullptr);
			return true;
		}
		return false;
	});

	_components.erase(it, _components.end());
}

component* object::get_component(const boost::typeindex::type_index& type) const {
	for (auto&& pair : _components) {
		if (pair.first == type)
			return boost::get_pointer(pair.second);
	}

	return nullptr;
}

component* object::get_component_in_parent(const boost::typeindex::type_index& type) const {
	for (const object* parent = _parent; parent; parent = parent->parent()) {
		auto c = parent->get_component(type);
		if (c)
			return c;
	}

	// No parent or no components in parent found
	return nullptr;
}

component* object::get_component_in_children(const boost::typeindex::type_index& type) const {
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

void object::get_components(const boost::typeindex::type_index& type, std::vector<component_ptr>& components) const {
	for (auto&& pair : _components) {
		if (pair.first == type)
			components.push_back(pair.second);
	}
}

void object::get_components_in_parent(const boost::typeindex::type_index& type, std::vector<component_ptr>& components) const {
	for (const object* parent = _parent; parent; parent = parent->parent())
		parent->get_components(type, components);
}

void object::get_components_in_children(const boost::typeindex::type_index& type, std::vector<component_ptr>& components) const {
	// Use recursion here
	for (auto&& child : _children) {
		// Look in children first
		child->get_components_in_children(type, components);
		child->get_components(type, components);
	}
}

object* object::add_child_object(const object_ptr& o) {
	BOOST_ASSERT(std::find(_children.begin(), _children.end(), o) == _children.end());
	_children.push_back(o);
	o->parent(this);
	return boost::get_pointer(_children.back());
}

bool object::remove_child_object(const object_ptr& o) {
	auto it = std::remove(_children.begin(), _children.end(), o);
	if (it != _children.end()) {
		_children.erase(it, _children.end());
		o->parent(nullptr);
		return true;
	}
	return false;
}

object* object::find_root_object() const {
	for (object* parent = _parent; parent; parent = parent->parent()) {
		if (!parent->parent())
			return parent;
	}
	return nullptr;
}

object* object::find_object(const char* name) {
	if (!name)
		return nullptr;

	const char* b = name;
	const char* e = b;

	while (*e == '/')
		b = ++e;

	object* o = this;

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

object* object::find_object_in_parent(const char* name) {
	for (object* parent = _parent; parent; parent = parent->parent()) {
		auto o = parent->find_object(name);
		if (o)
			return o;
	}

	return nullptr;
}

object* object::find_object_in_children(const char* name) {
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
