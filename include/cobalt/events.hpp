#ifndef COBALT_EVENTS_HPP_INCLUDED
#define COBALT_EVENTS_HPP_INCLUDED

#pragma once

#include <cobalt/events_fwd.hpp>

namespace cobalt {

////////////////////////////////////////////////////////////////////////////////
// event
//
	
inline event::target_type event::create_custom_target(const char* name, event::target_type base_target) {
	return murmur3(name, base_target);
}
	
////////////////////////////////////////////////////////////////////////////////
// event_dispatcher
//
	
namespace {
	template <typename T, typename E, typename = typename std::enable_if_t<std::is_base_of<event, E>::value>>
	struct rebind_method_event_handler {
		typedef rebind_method_event_handler self;
		
		constexpr rebind_method_event_handler(void(T::*mf)(E*), T* obj) noexcept : _obj(obj), _mf(mf) { }
		
		void operator()(event* ev) { (_obj->*_mf)(static_cast<E*>(ev)); }
		
		friend bool operator==(const self& rhs, const self& lhs) noexcept {
			return rhs._obj == lhs._obj && rhs._mf == lhs._mf;
		}
	private:
		T* _obj;
		void(T::*_mf)(E*);
	};
}
	
inline size_t event_dispatcher::subscribe(event::target_type target, const handler_type& handler) {
	size_t handler_hash = handler.target_type().hash_code();
	_handlers.emplace(target, handler);
	return handler_hash;
}

inline size_t event_dispatcher::subscribe(event::target_type target, handler_type&& handler) {
	size_t handler_hash = handler.target_type().hash_code();
	_handlers.emplace(target, std::move(handler));
	return handler_hash;
}

inline bool event_dispatcher::subscribed(event::target_type target, const handler_type& handler) const noexcept {
	return subscribed(target, handler.target_type().hash_code());
}

inline bool event_dispatcher::subscribed(event::target_type target, size_t handler_hash) const noexcept {
	auto range = _handlers.equal_range(target);
	for (auto it = range.first; it != range.second; ++it) {
		if ((*it).second.target_type().hash_code() == handler_hash)
			return true;
	}
	return false;
}

inline bool event_dispatcher::unsubscribe(event::target_type target, const handler_type& handler) noexcept {
	return unsubscribe(target, handler.target_type().hash_code());
}

inline bool event_dispatcher::unsubscribe(event::target_type target, size_t handler_hash) noexcept {
	auto range = _handlers.equal_range(target);
	for (auto it = range.first; it != range.second; ++it) {
		if ((*it).second.target_type().hash_code() == handler_hash) {
			_handlers.erase(it);
			return true;
		}
	}
	return false;
}
	
template <typename T, typename E>
inline size_t event_dispatcher::subscribe(void(T::*mf)(E*), T* obj, event::target_type target) {
	static_assert(std::is_same<event, E>::value || std::is_base_of<event, E>::value, "");
	subscribe(target, rebind_method_event_handler<T, E>(mf, obj));
	return typeid(rebind_method_event_handler<T, E>).hash_code();
}

template <typename T, typename E>
inline bool event_dispatcher::subscribed(void(T::*mf)(E*), const T* obj, event::target_type target) const noexcept {
	static_assert(std::is_same<event, E>::value || std::is_base_of<event, E>::value, "");
	const rebind_method_event_handler<T, E> sought(mf, const_cast<T*>(obj));
	auto range = _handlers.equal_range(target);
	for (auto it = range.first; it != range.second; ++it) {
		auto&& target = (*it).second.template target<rebind_method_event_handler<T, E>>();
		if (target && *target == sought)
			return true;
	}
	return false;
}

template <typename T, typename E>
inline bool event_dispatcher::unsubscribe(void(T::*mf)(E*), T* obj, event::target_type target, size_t* handler_hash) noexcept {
	static_assert(std::is_same<event, E>::value || std::is_base_of<event, E>::value, "");
	const rebind_method_event_handler<T, E> sought(mf, obj);
	auto range = _handlers.equal_range(target);
	for (auto it = range.first; it != range.second; ++it) {
		auto&& target = (*it).second.template target<rebind_method_event_handler<T, E>>();
		if (target && *target == sought) {
			if (handler_hash)
				*handler_hash = (*it).second.target_type().hash_code();
				_handlers.erase(it);
				return true;
		}
	}
	return false;
}
	
inline bool event_dispatcher::empty() const noexcept {
	return _queues[0].empty();
}

inline void event_dispatcher::post(const ref_ptr<event>& event) {
	_queues[0].emplace_back(event->target(), event);
}

inline void event_dispatcher::post(ref_ptr<event>&& event) {
	_queues[0].emplace_back(event->target(), std::move(event));
}

inline void event_dispatcher::post(event::target_type target, const ref_ptr<event>& event) {
	_queues[0].emplace_back(target, event);
}

inline void event_dispatcher::post(event::target_type target, ref_ptr<event>&& event) {
	_queues[0].emplace_back(target, std::move(event));
}

inline bool event_dispatcher::posted(event::target_type target) const noexcept {
	for (auto&& p : _queues[0]) {
		if (p.first == target)
			return true;
	}
	return false;
}

inline bool event_dispatcher::abort_first(event::target_type target) {
	auto it = std::find_if(_queues[0].begin(), _queues[0].end(), [&](auto&& v) { return v.first == target; });
	if (it != _queues[0].end()) {
		_queues[0].erase(it);
		return true;
	}
	return false;
}

inline bool event_dispatcher::abort_last(event::target_type target) {
	auto it = std::find_if(_queues[0].rbegin(), _queues[0].rend(), [&](auto&& v) { return v.first == target; });
	if (it != _queues[0].rend()) {
		_queues[0].erase(--it.base());
		return true;
	}
	return false;
}

inline void event_dispatcher::abort_all(event::target_type target) {
	_queues[0].erase(std::remove_if(_queues[0].begin(), _queues[0].end(),
		[&](auto&& v) {
			return v.first == target;
		}),
		_queues[0].end());
}

inline size_t event_dispatcher::dispatch(clock_type::duration timeout) {
	size_t count = 0;
	
	std::swap(_queues[0], _queues[1]);
	
	auto start = clock_type::now();
	
	while (!_queues[1].empty()) {
		auto&& p = _queues[1].front();
		count += invoke(p.first, p.second);
		_queues[1].pop_front();
		
		if (timeout != clock_type::duration() && clock_type::now() - start >= timeout)
			break;
	}
	
	// Insert not processed events to the main queue
	if (!_queues[1].empty()) {
		_queues[0].insert(_queues[0].begin(),
						  std::make_move_iterator(_queues[1].begin()),
						  std::make_move_iterator(_queues[1].end()));
		
		_queues[1].clear();
	}
	
	return count;
}

inline size_t event_dispatcher::invoke(const ref_ptr<event>& event) {
	auto range = _handlers.equal_range(event->target());
	for (auto it = range.first; it != range.second; ++it)
		(*it).second(event.get());
	return std::distance(range.first, range.second);
}

inline size_t event_dispatcher::invoke(event::target_type target, const ref_ptr<event>& event) {
	auto range = _handlers.equal_range(target);
	for (auto it = range.first; it != range.second; ++it)
		(*it).second(event.get());
	return std::distance(range.first, range.second);
}
	
////////////////////////////////////////////////////////////////////////////////
// event_subscriber
//

template <typename T>
inline event_subscriber<T>::event_subscriber(event_dispatcher& dispatcher) noexcept
	: _dispatcher(dispatcher)
{
}

template <typename T>
inline event_subscriber<T>::~event_subscriber() {
	for (auto&& s : _subscriptions) {
		BOOST_VERIFY(_dispatcher.unsubscribe(s.first, s.second));
	}
}

template <typename T>
template <typename E>
inline void event_subscriber<T>::subscribe(void(T::*mf)(E*)) {
	_subscriptions.emplace_back(E::type, _dispatcher.subscribe(mf, static_cast<T*>(this)));
}

template <typename T>
template <typename E>
inline bool event_subscriber<T>::subscribed(void(T::*mf)(E*)) const noexcept {
	return _dispatcher.subscribed(mf, static_cast<const T*>(this));
}

template <typename T>
template <typename E>
inline void event_subscriber<T>::unsubscribe(void(T::*mf)(E*), event::target_type target) noexcept {
	size_t handler_hash = 0;
	BOOST_VERIFY(_dispatcher.unsubscribe(target, mf, static_cast<T*>(this), &handler_hash));
	auto it = std::find(_subscriptions.begin(), _subscriptions.end(), std::make_pair(target, handler_hash));
	if (it != _subscriptions.end())
		_subscriptions.erase(it);
}

template <typename T>
template <typename E>
inline void event_subscriber<T>::respond(event::target_type target, void(T::*mf)(E*)) {
	static_assert(std::is_same<event, E>::value || std::is_base_of<event, E>::value, "");
	_subscriptions.emplace_back(target, _dispatcher.subscribe(mf, static_cast<T*>(this), target));
}

template <typename T>
template <typename E>
inline bool event_subscriber<T>::responding(event::target_type target, void(T::*mf)(E*)) const noexcept {
	static_assert(std::is_same<event, E>::value || std::is_base_of<event, E>::value, "");
	return _dispatcher.subscribed(mf, static_cast<const T*>(this), target);
}

} // namespace cobalt

#endif // COBALT_EVENTS_HPP_INCLUDED
