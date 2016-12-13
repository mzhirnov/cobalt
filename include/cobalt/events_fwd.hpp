#ifndef COBALT_EVENTS_FWD_HPP_INCLUDED
#define COBALT_EVENTS_FWD_HPP_INCLUDED

#pragma once

// Classes in this file:
//     event
//     basic_event
//     event_dispatcher
//     event_subscriber

#include <cobalt/utility/intrusive.hpp>
#include <cobalt/utility/hash.hpp>
#include <cobalt/utility/enum_traits.hpp>

#include <boost/assert.hpp>

#include <array>
#include <deque>
#include <unordered_map>
#include <chrono>

CO_DEFINE_ENUM_CLASS(
	event_phase, uint8_t,
	bubbling,
	capture,
	sinking
)

namespace cobalt {

/// event
class event : public ref_counter<event> {
public:
	typedef hash_type target_type;
	
	event() noexcept = default;
	
	virtual ~event() noexcept = default;
	
	virtual target_type target() const noexcept = 0;
	
	event_phase phase() const noexcept { return _phase; }
	
	bool handled() const noexcept { return _handled; }
	void handle() noexcept { handled(true); }
	
	static target_type create_custom_target(const char* name, target_type base_target = 0);
	
protected:
	void handled(bool handled) noexcept { _handled = handled; }
	
private:
	friend class event_dispatcher;
	
	void phase(event_phase phase) noexcept { _phase = phase; }
	
private:
	event_phase _phase = event_phase::capture;
	bool _handled = false;
};

/// basic_event
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

/// event_dispatcher
class event_dispatcher {
public:
	typedef std::function<void(event*)> handler_type;
	typedef std::chrono::high_resolution_clock clock_type;

	event_dispatcher() noexcept = default;
	
	event_dispatcher(const event_dispatcher&) = delete;
	event_dispatcher& operator=(const event_dispatcher&) = delete;

	size_t subscribe(event::target_type target, const handler_type& handler);
	size_t subscribe(event::target_type target, handler_type&& handler);
	
	bool subscribed(event::target_type target, const handler_type& handler) const noexcept;
	bool subscribed(event::target_type target, size_t handler_hash) const noexcept;
	
	bool unsubscribe(event::target_type target, const handler_type& handler) noexcept;
	bool unsubscribe(event::target_type target, size_t handler_hash) noexcept;

	template <typename T, typename E>
	size_t subscribe(void(T::*mf)(E*), T* obj, event::target_type target = E::type);
	
	template <typename T, typename E>
	bool subscribed(void(T::*mf)(E*), const T* obj, event::target_type target = E::type) const noexcept;

	template <typename T, typename E>
	bool unsubscribe(void(T::*mf)(E*), T* obj, event::target_type target = E::type, size_t* handler_hash = nullptr) noexcept;

	bool empty() const noexcept;
	
	void post(const ref_ptr<event>& event);
	void post(ref_ptr<event>&& event);
	void post(event::target_type target, const ref_ptr<event>& event);
	void post(event::target_type target, ref_ptr<event>&& event);
	
	bool posted(event::target_type target) const noexcept;
	
	bool abort_first(event::target_type target);
	bool abort_last(event::target_type target);
	void abort_all(event::target_type target);

	/// Dispatch events with timeout
	/// @return Number of invoked handlers
	size_t dispatch(clock_type::duration timeout = clock_type::duration());

	/// Invoke the event immediately
	/// @return Number of invoked handlers
	size_t invoke(const ref_ptr<event>& event);

	/// Invoke the event for specified target immediately
	/// @return Number of invoked handlers
	size_t invoke(event::target_type target, const ref_ptr<event>& event);

private:
	typedef std::unordered_multimap<event::target_type, handler_type, dont_hash<event::target_type>> Handlers;
	Handlers _handlers;

	typedef std::deque<std::pair<event::target_type, ref_ptr<event>>> Events;
	std::array<Events, 2> _queues;
};

/// Helper base class for objects to subscribe to and automatically unsubscribe from events
template <typename T>
class event_subscriber {
public:
	explicit event_subscriber(event_dispatcher& dispatcher) noexcept;

	~event_subscriber();
	
	event_subscriber(const event_subscriber&) = delete;
	event_subscriber& operator=(const event_subscriber&) = delete;

	template <typename E> void subscribe(void(T::*mf)(E*) = &T::on_event);
	template <typename E> bool subscribed(void(T::*mf)(E*) = &T::on_event) const noexcept;
	template <typename E> void unsubscribe(void(T::*mf)(E*) = &T::on_event, event::target_type target = E::type) noexcept;
	
	template <typename E> void respond(event::target_type target, void(T::*mf)(E*) = &T::on_target_event);
	template <typename E> bool responding(event::target_type target, void(T::*mf)(E*) = &T::on_target_event) const noexcept;

private:
	event_dispatcher& _dispatcher;

	typedef std::deque<std::pair<event::target_type, size_t>> Subscriptions;
	Subscriptions _subscriptions;
};

} // namespace cobalt

#endif // COBALT_EVENTS_FWD_HPP_INCLUDED
