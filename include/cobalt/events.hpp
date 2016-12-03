#ifndef COBALT_EVENTS_HPP_INCLUDED
#define COBALT_EVENTS_HPP_INCLUDED

#pragma once

// Classes in this file:
//     event
//     basic_event
//     rebind_event_handler
//     event_dispatcher
//     event_subscriber
//     event_target

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

CO_DEFINE_ENUM_FLAGS_CLASS(
	subscribe_mode, uint8_t,
	type = 1 << 0,
	name = 1 << 1,
	type_and_name = type | name
)

namespace cobalt {

/// event
class event : public ref_counter<event> {
public:
	typedef hash_type target_type;

	virtual ~event() = default;
	virtual target_type target() const = 0;
};

/// basic_event
template <event::target_type Target>
class basic_event : public event {
public:
	static constexpr target_type type = Target;

	virtual target_type target() const override { return Target; }
};

#define CO_DEFINE_EVENT_CLASS(event_class_name) class event_class_name : public basic_event<#event_class_name##_hash>

/// rebind_event_handler
template
<
	typename T,
	typename E,
	typename = typename std::enable_if<std::is_base_of<event, E>::value>::type
>
class rebind_event_handler {
public:
	rebind_event_handler(void(T::*mf)(E*), T* obj)
		: _obj(obj)
		, _mf(mf)
	{
	}

	void operator()(event* ev) {
		(_obj->*_mf)(static_cast<E*>(ev));
	}

	friend bool operator==(const rebind_event_handler& rhs, const rebind_event_handler& lhs) {
		return rhs._obj == lhs._obj && rhs._mf == lhs._mf;
	}

private:
	T* _obj;
	void(T::*_mf)(E*);
};

/// event_dispatcher
class event_dispatcher {
public:
	typedef event::target_type target_type;
	typedef std::function<void(event*)> handler_type;
	typedef std::chrono::high_resolution_clock clock_type;

	event_dispatcher() = default;

	// subscribe/subscribed/unsubscribe

	size_t subscribe(target_type target, handler_type handler) {
		size_t handler_hash = handler.target_type().hash_code();
		_handlers.emplace(target, std::move(handler));
		return handler_hash;
	}

	bool subscribed(target_type target, handler_type handler) const {
		return subscribed(target, handler.target_type().hash_code());
	}

	bool subscribed(target_type target, size_t handler_hash) const {
		auto range = _handlers.equal_range(target);
		for (auto it = range.first; it != range.second; ++it) {
			auto&& typeinfo = (*it).second.target_type();
			if (typeinfo.hash_code() == handler_hash)
				return true;
		}
		return false;
	}

	bool unsubscribe(target_type target, handler_type handler) {
		return unsubscribe(target, handler.target_type().hash_code());
	}

	bool unsubscribe(target_type target, size_t handler_hash) {
		auto range = _handlers.equal_range(target);
		for (auto it = range.first; it != range.second; ++it) {
			auto&& typeinfo = (*it).second.target_type();
			if (typeinfo.hash_code() == handler_hash) {
				_handlers.erase(it);
				return true;
			}
		}
		return false;
	}

	// subscribe/subscribed/unsubscribe templated overloads for member functions

	template <typename T, typename E>
	size_t subscribe(void(T::*mf)(E*), T* obj) {
		subscribe(E::type, rebind_event_handler<T, E>(mf, obj));
		return typeid(rebind_event_handler<T, E>).hash_code();
	}

	template <typename T, typename E>
	bool subscribed(void(T::*mf)(E*), T* obj) const {
		auto range = _handlers.equal_range(E::type);
		for (auto it = range.first; it != range.second; ++it) {
			auto target = (*it).second.template target<rebind_event_handler<T, E>>();
			if (target && *target == rebind_event_handler<T, E>(mf, obj))
				return true;
		}
		return false;
	}

	template <typename T, typename E>
	bool unsubscribe(void(T::*mf)(E*), T* obj, size_t* handler_hash = nullptr) {
		auto range = _handlers.equal_range(E::type);
		for (auto it = range.first; it != range.second; ++it) {
			auto target = (*it).second.template target<rebind_event_handler<T, E>>();
			if (target && *target == rebind_event_handler<T, E>(mf, obj)) {
				if (handler_hash)
					*handler_hash = (*it).second.target_type().hash_code();
				_handlers.erase(it);
				return true;
			}
		}
		return false;
	}

	// post/dispatch/invoke

	bool empty() const {
		return _queues[0].empty();
	}

	void post(const ref_ptr<event>& event) {
		_queues[0].emplace_back(event->target(), event);
	}
	
	void post(ref_ptr<event>&& event) {
		_queues[0].emplace_back(event->target(), std::move(event));
	}

	void post(target_type target, const ref_ptr<event>& event) {
		_queues[0].emplace_back(target, event);
	}

	void post(target_type target, ref_ptr<event>&& event) {
		_queues[0].emplace_back(target, std::move(event));
	}

	bool posted(target_type target) const {
		for (auto&& p : _queues[0]) {
			if (p.first == target)
				return true;
		}
		return false;
	}

	bool abort_first(target_type target) {
		auto it = std::find_if(_queues[0].begin(), _queues[0].end(),
			[&](const Events::value_type& v) {
				return v.first == target;
			});

		if (it != _queues[0].end()) {
			_queues[0].erase(it);
			return true;
		}

		return false;
	}

	bool abort_last(target_type target) {
		auto it = std::find_if(_queues[0].rbegin(), _queues[0].rend(),
			[&](const Events::value_type& v) {
				return v.first == target;
			});

		if (it != _queues[0].rend()) {
			_queues[0].erase(--it.base());
			return true;
		}

		return false;
	}

	void abort_all(target_type target) {
		auto it = std::remove_if(_queues[0].begin(), _queues[0].end(),
			[&](const Events::value_type& v) {
				return v.first == target;
			});
		
		if (it != _queues[0].end())
			_queues[0].erase(it, _queues[0].end());
	}

	/// Dispatches events with timeout
	/// @return Number of invoked handlers
	size_t dispatch(clock_type::duration timeout) {
		size_t count = 0;

		std::swap(_queues[0], _queues[1]);

		auto start = clock_type::now();

		while (!_queues[1].empty()) {
			auto&& p = _queues[1].front();
			count += invoke(p.first, p.second);
			_queues[1].pop_front();

			if (clock_type::now() - start >= timeout)
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

	/// Invokes the event immediately
	/// @return Number of invoked handlers
	size_t invoke(const ref_ptr<event>& event) {
		size_t count = 0;
		auto range = _handlers.equal_range(event->target());
		for (auto it = range.first; it != range.second; ++it, ++count)
			(*it).second(event.get());
		return count;
	}

	/// Invokes the event immediately
	/// @return Number of invoked handlers
	size_t invoke(target_type target, const ref_ptr<event>& event) {
		size_t count = 0;
		auto range = _handlers.equal_range(target);
		for (auto it = range.first; it != range.second; ++it, ++count)
			(*it).second(event.get());
		return count;
	}

private:
	typedef std::unordered_multimap<target_type, handler_type, dont_hash<target_type>> Handlers;
	Handlers _handlers;

	typedef std::deque<std::pair<target_type, ref_ptr<event>>> Events;
	std::array<Events, 2> _queues;
};

/// Helper base class for objects to subscribe to and automatically unsubscribe from events
template <typename T>
class event_subscriber {
public:
	explicit event_subscriber(event_dispatcher& dispatcher)
		: _dispatcher(dispatcher)
	{
	}

	~event_subscriber() {
		for (auto&& s : _subscriptions) {
			BOOST_VERIFY(_dispatcher.unsubscribe(s.first, s.second));
		}
	}

	template <typename E>
	void subscribe(void(T::*mf)(E*)) {
		auto handler_hash = _dispatcher.subscribe(mf, static_cast<T*>(this));
		_subscriptions.emplace_back(E::type, handler_hash);
	}

	template <typename E>
	bool subscribed(void(T::*mf)(E*)) const {
		return _dispatcher.subscribed(mf, static_cast<T*>(this));
	}

	template <typename E>
	void unsubscribe(void(T::*mf)(E*)) {
		size_t handler_hash = 0;
		BOOST_VERIFY(_dispatcher.unsubscribe(mf, static_cast<T*>(this), &handler_hash));
		// Remove handler hash for event type from subscriptions
		auto it = std::find(_subscriptions.begin(), _subscriptions.end(), std::make_pair(E::type, handler_hash));
		if (it != _subscriptions.end())
			_subscriptions.erase(it);
	}

private:
	event_dispatcher& _dispatcher;

	typedef std::deque<std::pair<event_dispatcher::target_type, size_t>> Subscriptions;
	Subscriptions _subscriptions;
};

/// Base class for objects to make them event targets
template
<
	typename T,
	subscribe_mode Mode = subscribe_mode::type_and_name
>
class event_target {
public:
	explicit event_target(event_dispatcher& dispatcher, const char* target_name = nullptr)
		: _dispatcher(dispatcher)
		, _target(get_target(target_name))
	{
		using std::placeholders::_1;
		
		event_dispatcher::handler_type handler = std::bind(&event_target::on_event, static_cast<T*>(this), _1);
		_handler_hash = _dispatcher.subscribe(_target, handler);
	}

	virtual ~event_target() {
		BOOST_VERIFY(_dispatcher.unsubscribe(_target, _handler_hash));
	}

	virtual void on_event(event*) = 0;

	static event_dispatcher::target_type get_target(const char* target_name) {
		static_assert(std::is_same<event_dispatcher::target_type, hash_type>::value, "target_type is not hash_type");
		
		hash_type hash_value = 0;

		if ((Mode & subscribe_mode::type) == subscribe_mode::type)
			hash_value = typeid(T).hash_code();

		if ((Mode & subscribe_mode::name) == subscribe_mode::name && target_name != nullptr)
			hash_value = murmur3(target_name, std::strlen(target_name), hash_value);

		BOOST_ASSERT_MSG(hash_value != 0, "Cannot identify target");
		
		return hash_value;
	}

private:
	event_dispatcher& _dispatcher;
	event_dispatcher::target_type _target = 0;
	size_t _handler_hash = 0;
};

} // namespace cobalt

#endif // COBALT_EVENTS_HPP_INCLUDED
