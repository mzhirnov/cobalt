#ifndef COBALT_UTILITY_FACTORY_HPP_INCLUDED
#define COBALT_UTILITY_FACTORY_HPP_INCLUDED

#pragma once

#include <boost/intrusive/slist.hpp>

namespace cobalt {

template <typename T>
class auto_factory;

template <typename R, typename... Args>
class auto_factory<R(Args...)> {
public:
	using result_type = R*;
	
	static bool can_create(const char* name) {
		for (auto&& factory : s_instances()) {
			if (factory.name && !strcmp(factory.name, name))
				return true;
		}
		
		return false;
	}

	static result_type create(const char* name, Args&&... args) {
		for (auto&& factory : s_instances()) {
			if (factory.name && !strcmp(factory.name, name))
				return factory.create(std::forward<Args&&>(args)...);
		}
		
		return nullptr;
	}

private:
	struct factory_node;
	
	using factory_list_hook = boost::intrusive::slist_base_hook<boost::intrusive::link_mode<boost::intrusive::normal_link>>;
	using factory_list = boost::intrusive::slist<factory_node, boost::intrusive::base_hook<factory_list_hook>, boost::intrusive::constant_time_size<false>>;
	
	struct factory_node : factory_list_hook {
		const char* name = nullptr;
		
		factory_node() noexcept {
			s_instances().push_front(*this);
		}
		
		virtual result_type create(Args&&... args) const = 0;
	};
	
	static factory_list& s_instances() noexcept {
		static factory_list instances;
		return instances;
	}
	
public:
	template <typename T, typename I>
	class registrar {
		struct factory_impl : factory_node {
			factory_impl() {
				T::set_factory_name();
			}
			
			virtual result_type create(Args&&... args) const override {
				static_assert(std::is_base_of<R, I>::value, "Implementation not derived from result type");
				return new I(std::forward<Args&&>(args)...);
			}
		};
		
	protected:
		static factory_impl _factory;
	};
};

template <typename R, typename... Args> template <typename T, typename I>
typename auto_factory<R(Args...)>::template registrar<T, I>::factory_impl auto_factory<R(Args...)>::template registrar<T, I>::_factory;

#define REGISTER_AUTO_FACTORY_WITH_NAME(Factory, Class, Name) \
	struct Factory##_registrar : Factory::registrar<Factory##_registrar, Class> { \
		static void set_factory_name() { \
			BOOST_ASSERT(!Factory::can_create(Name)); \
			_factory.name = Name; \
		} \
	};
	
#define REGISTER_AUTO_FACTORY(Factory, Class) \
	REGISTER_AUTO_FACTORY_WITH_NAME(Factory, Class, #Class)

} // namespace cobalt

#endif // COBALT_UTILITY_FACTORY_HPP_INCLUDED
