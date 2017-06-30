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
	
////////////////////////////////////////////////////////////////////////////////
// event_dispatcher
//
	
namespace detail {

template <typename T, typename E, typename = typename std::enable_if_t<std::is_base_of<event, E>::value>>
struct rebind_method {
	using method_type = void(T::*)(E*);
	
	constexpr rebind_method(method_type mf, const T* obj) noexcept
		: _method(mf), _object(obj)
	{ }
	
	const method_type method() const noexcept { return _method; }
	const T* object() const noexcept { return _object; }
	
	void operator()(event* ev) const { (_object->*_method)(static_cast<E*>(ev)); }
	void operator()(event* ev) { (const_cast<T*>(_object)->*_method)(static_cast<E*>(ev)); }

private:
	method_type _method;
	const T* _object;
	
private:
	friend bool operator==(const rebind_method& rhs, const rebind_method& lhs) noexcept {
		return rhs._object == lhs._object && rhs._method == lhs._method;
	}
};

} // namespace detail

event_dispatcher::~event_dispatcher() noexcept {
	BOOST_ASSERT(_connections.empty());
}
	
template <typename T, typename E>
inline void event_dispatcher::subscribe(void(T::*mf)(E*), const T* obj, const identifier& target) {
	static_assert(std::is_base_of<event, E>::value, "`E` must be derived from event");
	
	_subscriptions.emplace(target, ObjectHandler{obj, detail::rebind_method<T, E>(mf, obj)});
	_connections.emplace(obj, target);
}

template <typename T, typename E>
inline bool event_dispatcher::subscribed(void(T::*mf)(E*), const T* obj, const identifier& target) const noexcept {
	static_assert(std::is_base_of<event, E>::value, "`E` must be derived from event");
	
	auto subscriptions = _subscriptions.equal_range(target);
	for (auto subscr = subscriptions.first; subscr != subscriptions.second; ++subscr) {
		auto&& handler = (*subscr).second.second;
		auto&& target = handler.template target<detail::rebind_method<T, E>>();
		if (target && target->object() == obj && target->method() == mf)
			return true;
	}
	
	return false;
}

template <typename T, typename E>
inline bool event_dispatcher::unsubscribe(void(T::*mf)(E*), const T* obj, const identifier& target) noexcept {
	static_assert(std::is_base_of<event, E>::value, "`E` must be derived from event");
	
	auto subscriptions = _subscriptions.equal_range(target);
	for (auto subscr = subscriptions.first; subscr != subscriptions.second; ++subscr) {
		auto&& handler = (*subscr).second.second;
		auto&& target = handler.template target<detail::rebind_method<T, E>>();
		if (target && target->object() == obj && target->method() == mf) {
			// Remove object subscription
			_subscriptions.erase(subscr);
			// Remove object connection
			auto connections = _connections.equal_range(obj);
			for (auto conn = connections.first; conn != connections.second; ++conn) {
				if ((*conn).second == target) {
					_connections.erase(conn);
					break;
				}
			}
			return true;
		}
	}
	
	return false;
}

inline bool event_dispatcher::connected(const void* obj, const identifier& target) const noexcept {
	auto connections = _connections.equal_range(obj);
	for (auto conn = connections.first; conn != connections.second; ++conn) {
		if ((*conn).second == target)
			return true;
	}
	return false;
}

inline void event_dispatcher::disconnect(const void* obj, const identifier& target) {
	// Remove object subscriptions
	auto subscriptions = _subscriptions.equal_range(target);
	for (auto subscr = subscriptions.first; subscr != subscriptions.second; /**/) {
		auto&& object = (*subscr).second.first;
		subscr = (object == obj) ? _subscriptions.erase(subscr) : ++subscr;
	}
	
	// Remove object connections
	auto connections = _connections.equal_range(obj);
	for (auto conn = connections.first; conn != connections.second; /**/)
		conn = ((*conn).second == target) ? _connections.erase(conn) : ++conn;
}

inline void event_dispatcher::disconnect_all(const void* obj) {
	auto connections = _connections.equal_range(obj);
	for (auto conn = connections.first; conn != connections.second; ++conn) {
		auto subscriptions = _subscriptions.equal_range((*conn).second);
		for (auto subscr = subscriptions.first; subscr != subscriptions.second; /**/) {
			auto&& object = (*subscr).second.first;
			subscr = (object == obj) ? _subscriptions.erase(subscr) : ++subscr;
		}
	}
	
	if (connections.first != connections.second)
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

inline void event_dispatcher::post(const identifier& target, const ref_ptr<event>& event) {
	_queue.emplace_back(target, event);
}

inline void event_dispatcher::post(const identifier& target, ref_ptr<event>&& event) {
	_queue.emplace_back(target, std::move(event));
}

inline bool event_dispatcher::pending(const identifier& target) const noexcept {
	for (auto&& p : _queue) {
		if (p.first == target)
			return true;
	}
	return false;
}

inline size_t event_dispatcher::pending_count(const identifier& target) const noexcept {
	size_t count = 0;
	
	for (auto&& p : _queue) {
		if (p.first == target)
			++count;
	}
	
	return count;
}

inline bool event_dispatcher::abort_first(const identifier& target) {
	auto it = std::find_if(_queue.begin(), _queue.end(), [&](auto&& v) { return v.first == target; });
	if (it != _queue.end()) {
		_queue.erase(it);
		return true;
	}
	return false;
}

inline bool event_dispatcher::abort_last(const identifier& target) {
	auto it = std::find_if(_queue.rbegin(), _queue.rend(), [&](auto&& v) { return v.first == target; });
	if (it != _queue.rend()) {
		_queue.erase(--it.base());
		return true;
	}
	return false;
}

inline bool event_dispatcher::abort_all(const identifier& target) {
	auto old_count = _queue.size();
	
	_queue.erase(std::remove_if(_queue.begin(), _queue.end(),
		[&](auto&& v) {
			return v.first == target;
		}),
		_queue.end());
	
	return _queue.size() != old_count;
}

inline void event_dispatcher::dispatch(clock_type::duration timeout) {
	EventQueue queue;
	
	std::swap(_queue, queue);
	
	auto start = clock_type::now();
	
	while (!queue.empty() && (timeout == clock_type::duration() || clock_type::now() - start < timeout)) {
		auto&& p = queue.front();
		invoke(p.first, p.second);
		queue.pop_front();
	}
	
	// Return not processed events to the queue
	if (!queue.empty())
		_queue.insert(_queue.begin(), std::make_move_iterator(queue.begin()), std::make_move_iterator(queue.end()));
}

inline void event_dispatcher::invoke(const ref_ptr<event>& event) {
	auto subscriptions = _subscriptions.equal_range(event->target());
	for (auto it = subscriptions.first; it != subscriptions.second; ++it)
		(*it).second.second(event.get());
}

inline void event_dispatcher::invoke(const identifier& target, const ref_ptr<event>& event) {
	auto subscriptions = _subscriptions.equal_range(target);
	for (auto it = subscriptions.first; it != subscriptions.second; ++it)
		(*it).second.second(event.get());
}
	
////////////////////////////////////////////////////////////////////////////////
// event_handler
//

template <typename T>
inline event_handler<T>::event_handler(event_dispatcher& dispatcher) noexcept
	: _dispatcher(dispatcher)
{
}

template <typename T>
inline event_handler<T>::~event_handler() {
	_dispatcher.disconnect_all(this);
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
inline void event_handler<T>::unsubscribe(handler<E> handler, const identifier& target) noexcept {
	BOOST_VERIFY(_dispatcher.unsubscribe(target, handler, static_cast<T*>(this)));
}

template <typename T>
template <typename E>
inline void event_handler<T>::respond(const identifier& target, handler<E> handler) {
	_dispatcher.subscribe(handler, static_cast<T*>(this), target);
}

template <typename T>
template <typename E>
inline bool event_handler<T>::responds(const identifier& target, handler<E> handler) const noexcept {
	return _dispatcher.subscribed(handler, static_cast<const T*>(this), target);
}

template <typename T>
inline bool event_handler<T>::connected(const identifier& target) const noexcept {
	return _dispatcher.connected(this, target);
}

} // namespace cobalt

#endif // COBALT_EVENTS_HPP_INCLUDED
