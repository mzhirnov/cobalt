#ifndef COBALT_EVENTS_FWD_HPP_INCLUDED
#define COBALT_EVENTS_FWD_HPP_INCLUDED

#pragma once

// Classes in this file:
//     event
//     basic_event
//     event_dispatcher
//     event_handler

#include <cobalt/utility/intrusive.hpp>
#include <cobalt/utility/hash.hpp>
#include <cobalt/utility/enum_traits.hpp>

#include <boost/assert.hpp>

#include <array>
#include <deque>
#include <unordered_map>
#include <chrono>

CO_DEFINE_ENUM(
	event_phase, uint8_t,
	bubbling,
	capture,
	sinking
)

namespace cobalt {

/// Event
class event : public ref_counter<event> {
public:
	typedef uint32_t target_type;
	
	event() noexcept = default;
	
	event(event&&) noexcept = default;
	event& operator=(event&&) noexcept = default;
	
	event(const event&) = delete;
	event& operator=(const event&) = delete;
	
	virtual ~event() noexcept = default;
	
	virtual target_type target() const noexcept = 0;
	
	event_phase phase() const noexcept { return _phase; }
	
	bool handled() const noexcept { return _handled; }
	void handled(bool handled) noexcept { _handled = handled; }
	
	static target_type create_custom_target(const char* name, target_type base_target = 0) noexcept;
	
protected:
	void phase(event_phase phase) noexcept { _phase = phase; }
	
	/// Resets object to initial state
	virtual void reset() noexcept;

private:
	friend class event_dispatcher;
	
	event_phase _phase = event_phase::capture;
	bool _handled = false;
};

/// Basic event implementation
template <event::target_type Target>
class basic_event : public event {
public:
	enum : event::target_type { type = Target };

	virtual event::target_type target() const noexcept override { return Target; }
};

#define CO_DEFINE_EVENT(event) using event = basic_event<#event##_hash>;
#define CO_DEFINE_EVENT_WITH_TARGET(event, target) using event = basic_event<target>;
#define CO_DEFINE_EVENT_WITH_TARGET_NAME(event, target_name) using event = basic_event<target_name##_hash>;

#define CO_DEFINE_EVENT_CLASS(event) class event : public basic_event<#event##_hash>
#define CO_DEFINE_EVENT_CLASS_WITH_TARGET(event, target) class event : public basic_event<target>
#define CO_DEFINE_EVENT_CLASS_WITH_TARGET_NAME(event, target_name) class event : public basic_event<target_name##_hash>

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
	
	/// Subscribe object's method to specified event target
	template <typename T, typename E>
	void subscribe(void(T::*mf)(E*), const T* obj, event::target_type target = E::type);
	/// Check if specified object's method is subscribed to the event target
	template <typename T, typename E>
	bool subscribed(void(T::*mf)(E*), const T* obj, event::target_type target = E::type) const noexcept;
	/// Unsubscribe object's method from the event target
	template <typename T, typename E>
	bool unsubscribe(void(T::*mf)(E*), const T* obj, event::target_type target = E::type) noexcept;
	
	/// Check if object is connected to at least one specified event target
	bool connected(const void* obj, event::target_type target) const noexcept;
	/// Disconnect object from all connections with specified event target
	void disconnect(const void* obj, event::target_type target);
	/// Disconnect object from all connections
	void disconnect(const void* obj);

	//
	// Event queue
	//
	
	/// Check if event queue is empty
	bool empty() const noexcept;
	
	/// Post event to the event queue
	void post(const ref_ptr<event>& event);
	void post(ref_ptr<event>&& event);
	
	/// Post event with explicitly specified target to event queue
	void post(event::target_type target, const ref_ptr<event>& event);
	void post(event::target_type target, ref_ptr<event>&& event);
	
	/// Check if event queue contains event with specified target
	bool posted(event::target_type target) const noexcept;
	
	/// Remove event with specified target from event queue
	/// @return True if event was removed, false otherwise
	bool abort_first(event::target_type target);
	bool abort_last(event::target_type target);
	bool abort_all(event::target_type target);

	/// Invoke posted events with timeout
	void dispatch(clock_type::duration timeout = clock_type::duration());

	/// Invoke the event immediately
	void invoke(const ref_ptr<event>& event);

	/// Invoke the event for specified target immediately
	void invoke(event::target_type target, const ref_ptr<event>& event);

private:
	using ObjectHandler = std::pair<const void*, handler_type>;
	
	using Subscriptions =  std::unordered_multimap<event::target_type, ObjectHandler, dont_hash<event::target_type>>;
	Subscriptions _subscriptions;
	
	using Connections = std::unordered_multimap<const void*, event::target_type>;
	Connections _connections;

	using EventQueue = std::deque<std::pair<event::target_type, ref_ptr<event>>>;
	EventQueue _queue;
};

/// Helper base class for objects to subscribe to and automatically unsubscribe from events
template <typename T>
class event_handler {
public:
	template <typename E>
	using handler = void(T::*)(E*);
	
	explicit event_handler(event_dispatcher& dispatcher) noexcept;

	~event_handler();
	
	event_handler(const event_handler&) = delete;
	event_handler& operator=(const event_handler&) = delete;
	
	event_dispatcher& dispatcher() { return _dispatcher; }

	/// Subscribe method to specified event target
	template <typename E> void subscribe(handler<E> = &T::on_event);
	/// Check if method is subscribed to specified event target
	template <typename E> bool subscribed(handler<E> = &T::on_event) const noexcept;
	/// Unsubscribe method from specified event target
	template <typename E> void unsubscribe(handler<E> = &T::on_event, event::target_type target = E::type) noexcept;
	
	/// Register method as specified unique event responder
	template <typename E> void respond(event::target_type target, handler<E> = &T::on_target_event);
	/// Check if method responds to specified unique event
	template <typename E> bool responds(event::target_type target, handler<E> = &T::on_target_event) const noexcept;
	
	/// Check if this object connected to at least one specified event target
	bool connected(event::target_type target) const noexcept;

private:
	event_dispatcher& _dispatcher;
};

} // namespace cobalt

#endif // COBALT_EVENTS_FWD_HPP_INCLUDED
