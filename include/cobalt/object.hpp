#ifndef COBALT_OBJECT_HPP_INCLUDED
#define COBALT_OBJECT_HPP_INCLUDED

#pragma once

// Classes in this file:
//     object

#include <cobalt/utility/intrusive.hpp>
#include <cobalt/utility/type_index.hpp>
#include <cobalt/utility/identifier.hpp>
#include <cobalt/utility/factory.hpp>

#include <type_traits>

namespace cobalt {

///
/// Object
///
class object : public local_ref_counter<object> {
public:
	object() = default;
	
	object(const object&) = delete;
	object& operator=(const object&) = delete;
	
	virtual ~object() = default;
	
	virtual const type_index& object_type() const noexcept = 0;
	
	const identifier& name() const noexcept { return _name; }
	void name(const identifier& name) noexcept { _name = name; }
	
	static object* create_instance(const type_index& type);
	template <class T> static T* create_instance();
	
protected:
	using object_factory = auto_factory<object(), type_index>;
	
private:
	identifier _name;
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
	return static_cast<T*>(object_factory::create(T::class_type()));
}

#define IMPLEMENT_OBJECT_TYPE(ThisClass) \
private: \
	REGISTER_FACTORY_WITH_KEY(object_factory, ThisClass, ThisClass::class_type()) \
public: \
	static const type_index& class_type() noexcept { static auto type = type_id<ThisClass>(); return type; } \
	virtual const type_index& object_type() const noexcept override { return class_type(); }

} // namespace cobalt

#endif // COBALT_OBJECT_HPP_INCLUDED
