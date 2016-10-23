#include "pch.h"
#include "cool/application.hpp"
#include "cool/common.hpp"

// application

application* application::_instance = nullptr;

application::application(void* context)
	: _context(context)
{
	BOOST_ASSERT_MSG(!_instance, "application instance already exists");
	_instance = this;
}

application::~application() {
	BOOST_ASSERT_MSG(!!_instance, "application instance doesn't exist");
	_instance = nullptr;
}

shared_object* application::add_shared_object(const char* name, shared_object* data) {
	boost::mutex::scoped_lock lock(_mutex);
	
	auto it = _shared_objects.emplace(name, data).first;
	
	return boost::get_pointer((*it).second);
}

shared_object* application::get_shared_object(const char* name) const {
	boost::mutex::scoped_lock lock(_mutex);
	
	auto it = _shared_objects.find(name);
	if (it != _shared_objects.end()) {
		return boost::get_pointer((*it).second);
	}
	
	return nullptr;
}

shared_object_ptr application::remove_shared_object(const char* name) {
	shared_object_ptr object;
	
	boost::mutex::scoped_lock lock(_mutex);
	
	auto it = _shared_objects.find(name);
	if (it != _shared_objects.end()) {
		object = (*it).second;
		_shared_objects.erase(it);
	}
	
	return object;
}

application_component* application::add_component(application_component* component) {
	auto it = std::find_if(_components.rbegin(), _components.rend(),
		[&component](const Components::value_type& comp) {
			return comp->get_update_order() <= component->get_update_order();
		});
	
	_components.insert(it.base(), component);
	
	component->initialize();

	return component;
}

void application::remove_component(application_component* component) {
	_components.erase(
		std::remove_if(_components.begin(), _components.end(), [&component](const Components::value_type& comp) {
			return comp.get() == component;
		}),
		_components.end()
	);
}

void application::update_components() {
	for (auto& comp : _components) {
		comp->update();
	}
}

void application::on_update() {
	update_components();
}
