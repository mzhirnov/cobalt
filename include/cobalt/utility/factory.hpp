#ifndef COBALT_UTILITY_FACTORY_HPP_INCLUDED
#define COBALT_UTILITY_FACTORY_HPP_INCLUDED

#pragma once

#include <cobalt/utility/type_index.hpp>

#include <boost/intrusive/slist.hpp>

namespace cobalt {

template <typename T>
struct factory_key_traits {
	using key_type = T;
	using param_type = key_type;
	
	static bool empty(param_type key) noexcept { return !key; }
	static bool equal(param_type key1, param_type key2) noexcept { return key1 == key2; }
};

template <>
struct factory_key_traits<const char*> {
	using key_type = const char*;
	using param_type = key_type;
	
	static bool empty(param_type key) noexcept { return !key; }
	static bool equal(param_type key1, param_type key2) noexcept { return !std::strcmp(key1, key2); }
};

template <>
struct factory_key_traits<type_index> {
	using key_type = type_index;
	using param_type = const type_index&;
	
	static bool empty(param_type key) noexcept { return key == type_index(); }
	static bool equal(param_type key1, param_type key2) noexcept { return key1 == key2; }
};

template <typename T, typename Key>
class auto_factory;

template <typename R, typename... Args, typename Key>
class auto_factory<R(Args...), Key> {
public:
	using key_type = typename factory_key_traits<Key>::key_type;
	using param_type = typename factory_key_traits<Key>::param_type;
	using result_type = R*;
	
	static bool can_create(param_type key) noexcept { return !!find_creator(key); }

	static result_type create(param_type key, Args&&... args) {
		if (auto creator = find_creator(key))
			return creator->create(std::forward<Args&&>(args)...);
		return nullptr;
	}

private:
	struct creator;
	
	using list_hook = boost::intrusive::slist_base_hook<boost::intrusive::link_mode<boost::intrusive::normal_link>>;
	using creators_list = boost::intrusive::slist<creator, boost::intrusive::base_hook<list_hook>, boost::intrusive::constant_time_size<false>>;
	
	struct creator : list_hook {
		key_type key = {};
		creator() noexcept { instances().push_front(*this); }
		virtual result_type create(Args&&... args) const = 0;
	};
	
	static creators_list& instances() noexcept {
		static creators_list creators;
		return creators;
	}
	
	static creator* find_creator(param_type key) noexcept {
		for (auto&& creator : instances()) {
			if (!factory_key_traits<Key>::empty(creator.key) && factory_key_traits<Key>::equal(creator.key, key))
				return &creator;
		}
		return nullptr;
	}
	
public:
	template <typename T, typename I>
	class registrar {
		struct creator_impl : creator {
			creator_impl() {
				T::set_creator_key();
			}
			
			virtual result_type create(Args&&... args) const override {
				static_assert(std::is_base_of<R, I>::value, "Implementation not derived from result type");
				return new I(std::forward<Args&&>(args)...);
			}
		};
		
	protected:
		static creator_impl _creator;
	};
};

template <typename R, typename... Args, typename Key> template <typename T, typename I>
typename auto_factory<R(Args...), Key>::template registrar<T, I>::creator_impl auto_factory<R(Args...), Key>::template registrar<T, I>::_creator;

#define REGISTER_FACTORY_WITH_KEY(Factory, Class, Key) \
	struct Factory##_registrar : Factory::registrar<Factory##_registrar, Class> { \
		static void set_creator_key() { \
			BOOST_ASSERT(!Factory::can_create(Key)); \
			_creator.key = Key; \
		} \
	};

} // namespace cobalt

#endif // COBALT_UTILITY_FACTORY_HPP_INCLUDED
