#ifndef COBALT_EVENTS_HPP_INCLUDED
#define COBALT_EVENTS_HPP_INCLUDED

#pragma once

#include <cobalt/events_fwd.hpp>

namespace cobalt {

////////////////////////////////////////////////////////////////////////////////
// event
//

inline void event::reset() noexcept {
	handled(false);
	phase(event_phase::capture);
}
	
inline event::target_type event::create_custom_target(const char* name, event::target_type base_target) noexcept {
	return murmur3(name, base_target);
}
	
////////////////////////////////////////////////////////////////////////////////
// event_dispatcher
//
	
namespace detail {

template <typename T, typename E, typename = typename std::enable_if_t<std::is_base_of<event, E>::value>>
struct rebind_method {
	using method_type = void(T::*)(E*);
	
	constexpr rebind_method(method_type mf, T* obj) noexcept
		: _method(mf), _object(obj)
	{ }
	
	const method_type method() const noexcept { return _method; }
	
	const T* object() const noexcept { return _object; }
	
	void operator()(event* ev) { (_object->*_method)(static_cast<E*>(ev)); }
	
private:
	friend bool operator==(const rebind_method& rhs, const rebind_method& lhs) noexcept {
		return rhs._object == lhs._object && rhs._method == lhs._method;
	}

private:
	method_type _method;
	T* _object;
};

} // namespace detail

event_dispatcher::~event_dispatcher() noexcept {
	BOOST_ASSERT(_connections.empty());
}
	
template <typename T, typename E>
inline void event_dispatcher::subscribe(void(T::*mf)(E*), const T* obj, event::target_type target) {
	static_assert(std::is_base_of<event, E>::value, "`E` must be derived from event");
	
	_subscriptions.emplace(target, ObjectHandler{obj, detail::rebind_method<T, E>(mf, const_cast<T*>(obj))});
	_connections.emplace(obj, target);
}

template <typename T, typename E>
inline bool event_dispatcher::subscribed(void(T::*mf)(E*), const T* obj, event::target_type target) const noexcept {
	static_assert(std::is_base_of<event, E>::value, "`E` must be derived from event");
	
	const detail::rebind_method<T, E> sought(mf, const_cast<T*>(obj));
	
	auto range = _subscriptions.equal_range(target);
	for (auto it = range.first; it != range.second; ++it) {
		auto&& object_handler = (*it).second;
		auto target = object_handler.second.template target<detail::rebind_method<T, E>>();
		if (target && *target == sought)
			return true;
	}
	
	return false;
}

template <typename T, typename E>
inline bool event_dispatcher::unsubscribe(void(T::*mf)(E*), const T* obj, event::target_type target) noexcept {
	static_assert(std::is_base_of<event, E>::value, "`E` must be derived from event");
	
	const detail::rebind_method<T, E> sought(mf, const_cast<T*>(obj));
	
	auto range = _subscriptions.equal_range(target);
	for (auto it = range.first; it != range.second; ++it) {
		auto&& object_handler = (*it).second;
		auto target = object_handler.second.template target<detail::rebind_method<T, E>>();
		if (target && *target == sought) {
			// Remove object subscription
			_subscriptions.erase(it);
			// Remove object connection
			auto conn_range = _connections.equal_range(obj);
			for (auto conn_it = conn_range.first; conn_it != conn_range.second; ++conn_it) {
				if ((*conn_it).second == target) {
					_connections.erase(conn_it);
					break;
				}
			}
			return true;
		}
	}
	
	return false;
}

inline bool event_dispatcher::connected(const void* obj, event::target_type target) const noexcept {
	auto range = _connections.equal_range(obj);
	for (auto it = range.first; it != range.second; ++it) {
		if ((*it).second == target)
			return true;
	}
	return false;
}

inline void event_dispatcher::disconnect(const void* obj, event::target_type target) {
	// Remove object subscriptions
	auto subscr_range = _subscriptions.equal_range(target);
	for (auto it = subscr_range.first; it != subscr_range.second; ) {
		auto&& object_handler = (*it).second;
		it = (object_handler.first == obj) ? _subscriptions.erase(it) : ++it;
	}
	
	// Remove object connections
	auto conn_range = _connections.equal_range(obj);
	for (auto conn_it = conn_range.first; conn_it != conn_range.second; )
		conn_it = ((*conn_it).second == target) ? _connections.erase(conn_it) : ++conn_it;
}

inline void event_dispatcher::disconnect(const void* obj) {
	auto conn_range = _connections.equal_range(obj);
	for (auto conn_it = conn_range.first; conn_it != conn_range.second; ++conn_it) {
		auto subscr_range = _subscriptions.equal_range((*conn_it).second);
		for (auto it = subscr_range.first; it != subscr_range.second; ) {
			auto&& object_handler = (*it).second;
			it = (object_handler.first == obj) ? _subscriptions.erase(it) : ++it;
		}
	}
	
	if (conn_range.first != conn_range.second)
		_connections.erase(obj);
}
	
inline bool event_dispatcher::empty() const noexcept {
	return _queue.empty();
}

inline void event_dispatcher::post(const ref_ptr<event>& event) {
	_queue.emplace_back(event->target(), event);
}

inline void event_dispatcher::post(ref_ptr<event>&& event) {
	_queue.emplace_back(event->target(), std::move(event));
}

inline void event_dispatcher::post(event::target_type target, const ref_ptr<event>& event) {
	_queue.emplace_back(target, event);
}

inline void event_dispatcher::post(event::target_type target, ref_ptr<event>&& event) {
	_queue.emplace_back(target, std::move(event));
}

inline bool event_dispatcher::posted(event::target_type target) const noexcept {
	for (auto&& p : _queue) {
		if (p.first == target)
			return true;
	}
	return false;
}

inline bool event_dispatcher::abort_first(event::target_type target) {
	auto it = std::find_if(_queue.begin(), _queue.end(), [&](auto&& v) { return v.first == target; });
	if (it != _queue.end()) {
		_queue.erase(it);
		return true;
	}
	return false;
}

inline bool event_dispatcher::abort_last(event::target_type target) {
	auto it = std::find_if(_queue.rbegin(), _queue.rend(), [&](auto&& v) { return v.first == target; });
	if (it != _queue.rend()) {
		_queue.erase(--it.base());
		return true;
	}
	return false;
}

inline bool event_dispatcher::abort_all(event::target_type target) {
	auto count = _queue.size();
	
	_queue.erase(std::remove_if(_queue.begin(), _queue.end(),
		[&](auto&& v) {
			return v.first == target;
		}),
		_queue.end());
	
	return _queue.size() != count;
}

inline size_t event_dispatcher::dispatch(clock_type::duration timeout) {
	size_t count = 0;
	
	EventQueue queue = std::move(_queue);
	
	auto start = clock_type::now();
	
	while (!queue.empty() && !(timeout != clock_type::duration() && clock_type::now() - start >= timeout)) {
		auto&& p = queue.front();
		count += invoke(p.first, p.second);
		queue.pop_front();
	}
	
	// Return not processed events to the queue
	if (!queue.empty())
		_queue.insert(_queue.begin(), std::make_move_iterator(queue.begin()), std::make_move_iterator(queue.end()));
	
	return count;
}

inline size_t event_dispatcher::invoke(const ref_ptr<event>& event) {
	auto range = _subscriptions.equal_range(event->target());
	for (auto it = range.first; it != range.second; ++it)
		(*it).second.second(event.get());
	return std::distance(range.first, range.second);
}

inline size_t event_dispatcher::invoke(event::target_type target, const ref_ptr<event>& event) {
	auto range = _subscriptions.equal_range(target);
	for (auto it = range.first; it != range.second; ++it)
		(*it).second.second(event.get());
	return std::distance(range.first, range.second);
}
	
////////////////////////////////////////////////////////////////////////////////
// event_subscriber
//

template <typename T>
inline event_handler<T>::event_handler(event_dispatcher& dispatcher) noexcept
	: _dispatcher(dispatcher)
{
}

template <typename T>
inline event_handler<T>::~event_handler() {
	_dispatcher.disconnect(this);
}

template <typename T>
template <typename E>
inline void event_handler<T>::subscribe(handler<E> handler) {
	_dispatcher.subscribe(handler, static_cast<T*>(this));
}

template <typename T>
template <typename E>
inline bool event_handler<T>::subscribed(handler<E> handler) const noexcept {
	return _dispatcher.subscribed(handler, static_cast<const T*>(this));
}

template <typename T>
template <typename E>
inline void event_handler<T>::unsubscribe(handler<E> handler, event::target_type target) noexcept {
	BOOST_VERIFY(_dispatcher.unsubscribe(target, handler, static_cast<T*>(this)));
}

template <typename T>
template <typename E>
inline void event_handler<T>::respond(event::target_type target, handler<E> handler) {
	_dispatcher.subscribe(handler, static_cast<T*>(this), target);
}

template <typename T>
template <typename E>
inline bool event_handler<T>::responds(event::target_type target, handler<E> handler) const noexcept {
	return _dispatcher.subscribed(handler, static_cast<const T*>(this), target);
}

template <typename T>
inline bool event_handler<T>::connected(event::target_type target) const noexcept {
	return _dispatcher.connected(this, target);
}

} // namespace cobalt

#endif // COBALT_EVENTS_HPP_INCLUDED
