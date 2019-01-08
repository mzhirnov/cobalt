#pragma once

// Classes in this file:
//     shared_object
//     application_component
//     application

#include "cool/common.hpp"
#include "cool/enum_traits.hpp"

#include <unordered_map>

#include <boost/thread.hpp>
#include <boost/cast.hpp>

/// shared data object base
class shared_object : public ref_counter<shared_object> {
public:
	virtual ~shared_object() {}
};

typedef boost::intrusive_ptr<shared_object> shared_object_ptr;

DEFINE_ENUM_CLASS(
	update_order, uint32_t,
	earliest,
	pre_update,
	update,
	post_update,
	pre_draw,
	draw,
	post_draw,
	latest
)

/// application component
class application_component : public ref_counter<application_component> {
public:
	application_component(update_order order) : _order(order) {}
	virtual ~application_component() {}

	update_order get_update_order() const { return _order; }

	virtual void initialize() = 0;
	virtual void update() = 0;

private:
	update_order _order;
};

typedef boost::intrusive_ptr<application_component> application_component_ptr;

DEFINE_ENUM_CLASS(
	touch, uint32_t,
	begin,
	end,
	move,
	cancel
)

/// application base class
class application : public ref_counter<application> {
public:
	application(void* context);

	virtual ~application();

	static application* get_instance() { return _instance; }

	void* get_context() const { return _context; }

	shared_object* add_shared_object(const char* name, shared_object* data);
	shared_object* get_shared_object(const char* name) const;
	shared_object_ptr remove_shared_object(const char* name);

	template <typename T>
	T* add_shared_object(const char* name, shared_object* data) {
		return static_cast<T*>(add_shared_object(name, data));
	}
	
	template <typename T>
	T* get_shared_object(const char* name) const {
		return boost::polymorphic_downcast<T*>(get_shared_object(name));
	}

	application_component* add_component(application_component* component);
	void remove_component(application_component* component);

	// lifecycle callbacks

	virtual void on_save_state() {}
	virtual void on_create() {}
	virtual void on_gain_focus() {}
	virtual void on_lost_focus() {}
	virtual void on_terminate() {}

	// loop callbacks

	virtual void on_update();
	virtual void on_render(int /*width*/, int /*height*/) {}

	// input callbacks

	virtual void on_touch(touch /*action*/, int /*x*/, int /*y*/) {}

protected:
	void update_components();

private:
	static application* _instance;

	void* _context;

	typedef std::unordered_map<std::string, shared_object_ptr> SharedObjects;
	SharedObjects _shared_objects;

	mutable boost::mutex _mutex;

	typedef std::vector<application_component_ptr> Components;
	Components _components;

private:
	DISALLOW_COPY_AND_ASSIGN(application)
};

typedef boost::intrusive_ptr<application> application_ptr;

extern application* create_application(void* context);
