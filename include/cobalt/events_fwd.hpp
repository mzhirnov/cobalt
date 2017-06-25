#ifndef COBALT_EVENTS_FWD_HPP_INCLUDED
#define COBALT_EVENTS_FWD_HPP_INCLUDED

#pragma once

// Classes in this file:
//     event_target
//     event
//     event_dispatcher
//     event_handler

#include <cobalt/utility/intrusive.hpp>
#include <cobalt/utility/enum_traits.hpp>

#include <boost/flyweight.hpp>
#include <boost/assert.hpp>

#include <string>
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

namespace detail {

struct event_target_tag {};

} // namespace detail

/// Event target
///
/// Event subscribers' end point.
using event_target = boost::flyweight<
	std::string,
	boost::flyweights::tag<detail::event_target_tag>,
	boost::flyweights::hashed_factory<
		std::hash<std::string>,
		std::equal_to<std::string>,
		std::allocator<boost::mpl::_1>
	>,
	boost::flyweights::tracking<boost::flyweights::refcounted>
>;

/// Event
class event : public ref_counter<event> {
public:
	event() noexcept = default;
	
	event(event&&) noexcept = default;
	event& operator=(event&&) noexcept = default;
	
	event(const event&) = delete;
	event& operator=(const event&) = delete;
	
	virtual ~event() noexcept = default;
	
	virtual const event_target& target() const noexcept = 0;
	
	event_phase phase() const noexcept { return _phase; }
	
	bool handled() const noexcept { return _handled; }
	void handled(bool handled) noexcept { _handled = handled; }
	
protected:
	void phase(event_phase phase) noexcept { _phase = phase; }
	
	/// Resets object to initial state
	virtual void reset() noexcept;

private:
	friend class event_dispatcher;
	
	event_phase _phase = event_phase::capture;
	bool _handled = false;
};

#define CO_DEFINE_EVENT(Event) CO_DEFINE_EVENT_CLASS(Event) {};
#define CO_DEFINE_EVENT_WITH_TARGET_NAME(Event, TargetName) CO_DEFINE_EVENT_CLASS_WITH_TARGET_NAME(Event, TargetName) {};

#define CO_DEFINE_EVENT_CLASS(Event) CO_DEFINE_EVENT_CLASS_WITH_TARGET_NAME(Event, #Event)
#define CO_DEFINE_EVENT_CLASS_WITH_TARGET_NAME(Event, TargetName) \
class Event##_base : public event { \
public: \
	static const event_target& static_target() noexcept { static event_target target(TargetName); return target; } \
	virtual const event_target& target() const noexcept override { return static_target(); } \
}; \
class Event : public Event##_base

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
	void subscribe(void(T::*mf)(E*), const T* obj, const event_target& target = E::static_target());
	/// Check if object is subscribed to event target
	template <typename T, typename E>
	bool subscribed(void(T::*mf)(E*), const T* obj, const event_target& target = E::static_target()) const noexcept;
	/// Unsubscribe object from event target
	template <typename T, typename E>
	bool unsubscribe(void(T::*mf)(E*), const T* obj, const event_target& target = E::static_target()) noexcept;
	
	/// Check if object connected to any events with this target
	bool connected(const void* obj, const event_target& target) const noexcept;
	/// Disconnect object from all events with this target
	void disconnect(const void* obj, const event_target& target);
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
	void post(const event_target& target, const ref_ptr<event>& event);
	void post(const event_target& target, ref_ptr<event>&& event);
	
	/// Check if there is penging event with this target
	bool pending(const event_target& target) const noexcept;
	/// @return Number of pending events with this target
	size_t pending_count(const event_target& target) const noexcept;
	
	/// Remove event with this target from event queue
	/// @return True if event was removed, false otherwise
	bool abort_first(const event_target& target);
	bool abort_last(const event_target& target);
	bool abort_all(const event_target& target);

	/// Invoke pending events with timeout
	void dispatch(clock_type::duration timeout = clock_type::duration());

	/// Invoke event immediately
	void invoke(const ref_ptr<event>& event);

	/// Invoke event for specified event target immediately
	void invoke(const event_target& target, const ref_ptr<event>& event);

private:
	using ObjectHandler = std::pair<const void*, handler_type>;
	using Subscriptions =  std::unordered_multimap<event_target, ObjectHandler, boost::hash<event_target>>;
	using Connections = std::unordered_multimap<const void*, event_target>;
	using EventQueue = std::deque<std::pair<event_target, ref_ptr<event>>>;
	
	Subscriptions _subscriptions;
	Connections _connections;
	EventQueue _queue;
};

/// Helper base class for objects to subscribe to and automatically unsubscribe from events
template <typename T>
class event_handler {
public:
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
	template <typename E> void unsubscribe(handler<E> = &T::on_event, const event_target& target = E::static_target()) noexcept;
	
	/// Register method as event responder
	template <typename E> void respond(const event_target& target, handler<E> = &T::on_target_event);
	/// Check if method responds to event
	template <typename E> bool responds(const event_target& target, handler<E> = &T::on_target_event) const noexcept;
	
	/// Check if this object connected to any events with this target
	bool connected(const event_target& target) const noexcept;

private:
	event_dispatcher& _dispatcher;
};

} // namespace cobalt

#endif // COBALT_EVENTS_FWD_HPP_INCLUDED
