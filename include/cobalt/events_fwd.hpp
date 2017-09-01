#ifndef COBALT_EVENTS_FWD_HPP_INCLUDED
#define COBALT_EVENTS_FWD_HPP_INCLUDED

#pragma once

// Classes in this file:
//     event
//     event_dispatcher
//     event_handler

#include <cobalt/utility/intrusive.hpp>
#include <cobalt/utility/enum_traits.hpp>
#include <cobalt/utility/identifier.hpp>

#include <boost/assert.hpp>

#include <deque>
#include <unordered_map>
#include <chrono>

namespace cobalt {

/// Event
class event : public local_ref_counter<event> {
public:
	event() noexcept = default;
	
	event(event&&) noexcept = default;
	event& operator=(event&&) noexcept = default;
	
	event(const event&) = delete;
	event& operator=(const event&) = delete;
	
	virtual ~event() noexcept = default;
	
	virtual const identifier& target() const noexcept = 0;
	
	bool handled() const noexcept { return _handled; }
	void handled(bool handled) noexcept { _handled = handled; }

private:
	friend class event_dispatcher;
	
	bool _handled = false;
};

#define IMPLEMENT_EVENT_TARGET(TargetName) \
public: \
	static const identifier& static_target() noexcept { static identifier target(TargetName); return target; } \
	virtual const identifier& target() const noexcept override { return static_target(); }

/// Event dispatcher
///
/// Manages event queue and tracks event subscribers.
class event_dispatcher {
public:
	using handler_type = std::function<void(event*)>;
	using clock_type = std::chrono::high_resolution_clock;

	event_dispatcher() noexcept = default;
	
	event_dispatcher(event_dispatcher&&) noexcept = default;
	event_dispatcher& operator=(event_dispatcher&&) noexcept = default;
	
	event_dispatcher(const event_dispatcher&) = delete;
	event_dispatcher& operator=(const event_dispatcher&) = delete;
	
	~event_dispatcher() noexcept;

	//
	// Subscription
	//
	
	/// Subscribe object to event target
	template <typename T, typename E>
	void subscribe(void(T::*mf)(E*), const T* obj, const identifier& target = E::static_target());
	/// Check if object is subscribed to event target
	template <typename T, typename E>
	bool subscribed(void(T::*mf)(E*), const T* obj, const identifier& target = E::static_target()) const noexcept;
	/// Unsubscribe object from event target
	template <typename T, typename E>
	bool unsubscribe(void(T::*mf)(E*), const T* obj, const identifier& target = E::static_target()) noexcept;
	
	/// Check if object connected to any events with this target
	bool connected(const void* obj, const identifier& target) const noexcept;
	/// Disconnect object from all events with this target
	void disconnect(const void* obj, const identifier& target);
	/// Disconnect object from all event targets
	void disconnect_all(const void* obj);

	//
	// Event queue
	//
	
	/// Check if event queue is empty
	bool empty() const noexcept;
	
	/// Post event to the event queue
	void post(const ref_ptr<event>& event);
	void post(ref_ptr<event>&& event);
	
	/// Post event with explicitly specified target to event queue
	void post(const identifier& target, const ref_ptr<event>& event);
	void post(const identifier& target, ref_ptr<event>&& event);
	
	/// Check if there is penging event with this target
	bool pending(const identifier& target) const noexcept;
	/// @return Number of pending events with this target
	size_t pending_count(const identifier& target) const noexcept;
	
	/// Remove event with this target from event queue
	/// @return True if event was aborted, false otherwise, or number of aborted events
	bool abort_first(const identifier& target);
	bool abort_last(const identifier& target);
	size_t abort_all(const identifier& target);

	/// Invoke pending events with timeout
	/// @return Number of invoked handlers
	size_t dispatch(clock_type::duration timeout = clock_type::duration());

	/// Invoke event immediately
	/// @return Number of invoked handlers
	size_t invoke(const ref_ptr<event>& event);

	/// Invoke event for specified event target immediately
	/// @return Number of invoked handlers
	size_t invoke(const identifier& target, const ref_ptr<event>& event);

private:
	using Subscriptions =  std::unordered_multimap<identifier, std::pair<const void*, handler_type>, boost::hash<identifier>>;
	using Connections = std::unordered_multimap<const void*, identifier>;
	using EventQueue = std::deque<std::pair<identifier, ref_ptr<event>>>;
	
	Subscriptions _subscriptions;
	Connections _connections;
	EventQueue _queue;
};

/// Helper base class for objects to subscribe to and automatically unsubscribe from events
template <typename T>
class event_handler {
public:
	using this_type = T;
	
	template <typename E> using handler = void(T::*)(E*);
	
	explicit event_handler(event_dispatcher& dispatcher) noexcept;

	~event_handler();
	
	event_handler(const event_handler&) = delete;
	event_handler& operator=(const event_handler&) = delete;
	
	event_dispatcher& dispatcher() { return _dispatcher; }

	/// Subscribe method to event
	template <typename E> void subscribe(handler<E> = &T::on_event);
	/// Check if method subscribed to event
	template <typename E> bool subscribed(handler<E> = &T::on_event) const noexcept;
	/// Unsubscribe method from event
	template <typename E> void unsubscribe(handler<E> = &T::on_event, const identifier& target = E::static_target()) noexcept;
	
	/// Register method as event responder
	template <typename E> void respond(const identifier& target, handler<E> = &T::on_target_event);
	/// Check if method responds to event
	template <typename E> bool responds(const identifier& target, handler<E> = &T::on_target_event) const noexcept;
	
	/// Check if this object connected to any events with this target
	bool connected(const identifier& target) const noexcept;

private:
	event_dispatcher& _dispatcher;
};

} // namespace cobalt

#endif // COBALT_EVENTS_FWD_HPP_INCLUDED
