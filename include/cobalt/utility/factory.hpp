#ifndef COBALT_UTILITY_FACTORY_HPP_INCLUDED
#define COBALT_UTILITY_FACTORY_HPP_INCLUDED

#pragma once

#include <cobalt/utility/identifier.hpp>

#include <boost/intrusive/slist.hpp>

namespace cobalt {

template <typename T>
class auto_factory;

template <typename R, typename... Args>
class auto_factory<R(Args...)> {
public:
	using result_type = R*;
	
	static bool can_create(const identifier& id) {
		for (auto&& factory : s_instances()) {
			if (factory.id == id)
				return true;
		}
		
		return false;
	}

	static result_type create(const identifier& id, Args&&... args) {
		for (auto&& factory : s_instances()) {
			if (factory.id == id)
				return factory.create(std::forward<Args&&>(args)...);
		}
		
		return nullptr;
	}

private:
	struct factory_node;
	
	using factory_list_hook = boost::intrusive::slist_base_hook<boost::intrusive::link_mode<boost::intrusive::normal_link>>;
	using factory_list = boost::intrusive::slist<factory_node, boost::intrusive::base_hook<factory_list_hook>, boost::intrusive::constant_time_size<false>>;
	
	struct factory_node : factory_list_hook {
		identifier id;
		
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
				// So as this code performs during static objects initialization,
				// we need to initialize boost::flyweight's static storage first.
				identifier::init();
				
				T::register_factory();
			}
			
			virtual result_type create(Args&&... args) const override {
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
	struct registrar : Factory::registrar<registrar, Class> { static void register_factory() { _factory.id = Name; } };
	
#define REGISTER_AUTO_FACTORY(Factory, Class) \
	REGISTER_AUTO_FACTORY_WITH_NAME(Factory, Class, #Class)

} // namespace cobalt

#endif // COBALT_UTILITY_FACTORY_HPP_INCLUDED
