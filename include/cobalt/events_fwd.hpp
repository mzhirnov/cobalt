#ifndef COBALT_EVENTS_HPP_INCLUDED
#define COBALT_EVENTS_HPP_INCLUDED

#pragma once

// Classes in this file:
//     event
//     basic_event
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

namespace cobalt {

/// event
class event : public ref_counter<event> {
public:
	typedef hash_type target_type;

	virtual ~event() = default;
	virtual target_type target() const noexcept = 0;
	
	static target_type create_custom_target(const char* name, target_type base_target = 0) {
		return murmur3(name, base_target);
	}
};

/// basic_event
template <event::target_type Target>
class basic_event : public event {
public:
	enum : event::target_type { type = Target };

	virtual event::target_type target() const noexcept override { return Target; }
};

#define CO_DEFINE_EVENT_CLASS(event_class) class event_class : public basic_event<#event_class##_hash>
#define CO_DEFINE_EVENT_CLASS_WITH_TARGET_NAME(event_class, target_name) class event_class : public basic_event<target_name##_hash>

/// event_dispatcher
class event_dispatcher {
public:
	typedef std::function<void(event*)> handler_type;
	typedef std::chrono::high_resolution_clock clock_type;

	event_dispatcher() = default;
	
	event_dispatcher(const event_dispatcher&) = delete;
	event_dispatcher& operator=(const event_dispatcher&) = delete;

	// subscribe/subscribed/unsubscribe

	size_t subscribe(event::target_type target, const handler_type& handler) {
		size_t handler_hash = handler.target_type().hash_code();
		_handlers.emplace(target, handler);
		return handler_hash;
	}
	
	size_t subscribe(event::target_type target, handler_type&& handler) {
		size_t handler_hash = handler.target_type().hash_code();
		_handlers.emplace(target, std::move(handler));
		return handler_hash;
	}

	bool subscribed(event::target_type target, const handler_type& handler) const noexcept {
		return subscribed(target, handler.target_type().hash_code());
	}

	bool subscribed(event::target_type target, size_t handler_hash) const noexcept {
		auto range = _handlers.equal_range(target);
		for (auto it = range.first; it != range.second; ++it) {
			if ((*it).second.target_type().hash_code() == handler_hash)
				return true;
		}
		return false;
	}

	bool unsubscribe(event::target_type target, const handler_type& handler) noexcept {
		return unsubscribe(target, handler.target_type().hash_code());
	}

	bool unsubscribe(event::target_type target, size_t handler_hash) noexcept {
		auto range = _handlers.equal_range(target);
		for (auto it = range.first; it != range.second; ++it) {
			if ((*it).second.target_type().hash_code() == handler_hash) {
				_handlers.erase(it);
				return true;
			}
		}
		return false;
	}

	// subscribe/subscribed/unsubscribe templated overloads for member functions

	template <typename T, typename E>
	size_t subscribe(event::target_type target, void(T::*mf)(E*), T* obj) {
		subscribe(target, rebind_method_event_handler<T, E>(mf, obj));
		return typeid(rebind_method_event_handler<T, E>).hash_code();
	}
	
	template <typename T, typename E>
	size_t subscribe(void(T::*mf)(E*), T* obj) {
		return subscribe(E::type, mf, obj);
	}

	template <typename T, typename E>
	bool subscribed(event::target_type target, void(T::*mf)(E*), const T* obj) const noexcept {
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
	bool subscribed(void(T::*mf)(E*), const T* obj) const noexcept {
		return subscribed(E::type, mf, obj);
	}

	template <typename T, typename E>
	bool unsubscribe(event::target_type target, void(T::*mf)(E*), T* obj, size_t* handler_hash = nullptr) noexcept {
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
	
	template <typename T, typename E>
	bool unsubscribe(void(T::*mf)(E*), T* obj, size_t* handler_hash = nullptr) noexcept {
		return unsubscribe(E::type, mf, obj, handler_hash);
	}

	// post/dispatch/invoke

	bool empty() const noexcept {
		return _queues[0].empty();
	}

	void post(const ref_ptr<event>& event) {
		_queues[0].emplace_back(event->target(), event);
	}
	
	void post(ref_ptr<event>&& event) {
		_queues[0].emplace_back(event->target(), std::move(event));
	}

	void post(event::target_type target, const ref_ptr<event>& event) {
		_queues[0].emplace_back(target, event);
	}

	void post(event::target_type target, ref_ptr<event>&& event) {
		_queues[0].emplace_back(target, std::move(event));
	}

	bool posted(event::target_type target) const noexcept {
		for (auto&& p : _queues[0]) {
			if (p.first == target)
				return true;
		}
		return false;
	}

	bool abort_first(event::target_type target) {
		auto it = std::find_if(_queues[0].begin(), _queues[0].end(), [&](auto&& v) { return v.first == target; });
		if (it != _queues[0].end()) {
			_queues[0].erase(it);
			return true;
		}
		return false;
	}

	bool abort_last(event::target_type target) {
		auto it = std::find_if(_queues[0].rbegin(), _queues[0].rend(), [&](auto&& v) { return v.first == target; });
		if (it != _queues[0].rend()) {
			_queues[0].erase(--it.base());
			return true;
		}
		return false;
	}

	void abort_all(event::target_type target) {
		auto it = std::remove_if(_queues[0].begin(), _queues[0].end(), [&](auto&& v) { return v.first == target; });
		if (it != _queues[0].end())
			_queues[0].erase(it, _queues[0].end());
	}

	/// Dispatches events with timeout
	/// @return Number of invoked handlers
	size_t dispatch(clock_type::duration timeout = clock_type::duration()) {
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

	/// Invokes the event immediately
	/// @return Number of invoked handlers
	size_t invoke(const ref_ptr<event>& event) {
		auto range = _handlers.equal_range(event->target());
		for (auto it = range.first; it != range.second; ++it)
			(*it).second(event.get());
		return std::distance(range.first, range.second);
	}

	/// Invokes the event for specified target immediately
	/// @return Number of invoked handlers
	size_t invoke(event::target_type target, const ref_ptr<event>& event) {
		auto range = _handlers.equal_range(target);
		for (auto it = range.first; it != range.second; ++it)
			(*it).second(event.get());
		return std::distance(range.first, range.second);
	}

private:
	typedef std::unordered_multimap<event::target_type, handler_type, dont_hash<event::target_type>> Handlers;
	Handlers _handlers;

	typedef std::deque<std::pair<event::target_type, ref_ptr<event>>> Events;
	std::array<Events, 2> _queues;

	template <typename T, typename E, typename = typename std::enable_if<std::is_base_of<event, E>::value>::type>
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
};

/// Helper base class for objects to subscribe to and automatically unsubscribe from events
template <typename T>
class event_subscriber {
public:
	explicit event_subscriber(event_dispatcher& dispatcher) noexcept
		: _dispatcher(dispatcher)
	{ }

	~event_subscriber() {
		for (auto&& s : _subscriptions) {
			BOOST_VERIFY(_dispatcher.unsubscribe(s.first, s.second));
		}
	}
	
	event_subscriber(const event_subscriber&) = delete;
	event_subscriber& operator=(const event_subscriber&) = delete;

	template <typename E>
	void subscribe(void(T::*mf)(E*)) {
		_subscriptions.emplace_back(E::type, _dispatcher.subscribe(mf, static_cast<T*>(this)));
	}

	template <typename E>
	bool subscribed(void(T::*mf)(E*)) const noexcept {
		return _dispatcher.subscribed(mf, static_cast<const T*>(this));
	}

	template <typename E>
	void unsubscribe(void(T::*mf)(E*)) noexcept {
		size_t handler_hash = 0;
		BOOST_VERIFY(_dispatcher.unsubscribe(mf, static_cast<T*>(this), &handler_hash));
		// Remove handler hash for event type from subscriptions
		auto it = std::find(_subscriptions.begin(), _subscriptions.end(), std::make_pair(E::type, handler_hash));
		if (it != _subscriptions.end())
			_subscriptions.erase(it);
	}

private:
	event_dispatcher& _dispatcher;

	typedef std::deque<std::pair<event::target_type, size_t>> Subscriptions;
	Subscriptions _subscriptions;
};

/// Base class for objects to make them event targets
template <typename T, typename E>
class event_target {
public:
	event_target(event_dispatcher& dispatcher, event::target_type target)
		: _dispatcher(dispatcher)
		, _target(target)
	{
		BOOST_ASSERT(_target != 0);
		
		_handler = _dispatcher.subscribe(_target, static_cast<void(T::*)(E*)>(&T::on_event), static_cast<T*>(this));
	}
	
	event_target(event_dispatcher& dispatcher, const char* name)
		: event_target(dispatcher, event::create_custom_target(name))
	{ }
	
	event_target(event_dispatcher& dispatcher, const char* name, event::target_type base_target)
		: event_target(dispatcher, event::create_custom_target(name, base_target))
	{ }

	virtual ~event_target() noexcept {
		BOOST_VERIFY(_dispatcher.unsubscribe(_target, _handler));
	}
	
	event_target(const event_target&) = delete;
	event_target& operator=(const event_target&) = delete;
	
	event_dispatcher& dispatcher() const { return _dispatcher; }

	event::target_type target() const noexcept { return _target; }

private:
	event_dispatcher& _dispatcher;
	event::target_type _target = {};
	size_t _handler = 0;
};

} // namespace cobalt

#endif // COBALT_EVENTS_HPP_INCLUDED
