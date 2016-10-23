#pragma once

#include <array>
#include <deque>
#include <unordered_map>
#include <chrono>

#include <boost/smart_ptr/intrusive_ptr.hpp>
#include <boost/smart_ptr/intrusive_ref_counter.hpp>

#include "hash.h"

/// event
class event : public boost::intrusive_ref_counter<event> {
public:
	typedef size_t target_type;

	virtual ~event() = default;
	virtual target_type target() const = 0;
};

typedef boost::intrusive_ptr<event> event_ptr;

/// basic_event
template <event::target_type Target>
class basic_event : public event {
public:
	constexpr static target_type type = Target;

	virtual target_type target() const override { return Target; }
};

#define DEFINE_EVENT_CLASS(event_class_name) class event_class_name : public basic_event<#event_class_name##_hash>

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

/// event_manager
class event_manager {
public:
	typedef event::target_type target_type;
	typedef std::function<void(event*)> handler_type;

	event_manager() = default;

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

	// template overloads for member functions

	template <typename T, typename E>
	size_t subscribe(void(T::*mf)(E*), T* obj) {
		subscribe(E::type, rebind_event_handler<T, E>(mf, obj));
		return typeid(rebind_event_handler<T, E>).hash_code();
	}

	template <typename T, typename E>
	bool subscribed(void(T::*mf)(E*), T* obj) const {
		auto range = _handlers.equal_range(E::type);
		for (auto it = range.first; it != range.second; ++it) {
			auto target = (*it).second.target<rebind_event_handler<T, E>>();
			if (target && *target == rebind_event_handler<T, E>(mf, obj))
				return true;
		}
		return false;
	}

	template <typename T, typename E>
	bool unsubscribe(void(T::*mf)(E*), T* obj, size_t* handler_hash = nullptr) {
		auto range = _handlers.equal_range(E::type);
		for (auto it = range.first; it != range.second; ++it) {
			auto target = (*it).second.target<rebind_event_handler<T, E>>();
			if (target && *target == rebind_event_handler<T, E>(mf, obj)) {
				if (handler_hash)
					*handler_hash = (*it).second.target_type().hash_code();
				_handlers.erase(it);
				return true;
			}
		}
		return false;
	}

	// post/process/invoke

	bool empty() const {
		return _queues[0].empty();
	}

	void post(const event_ptr& event) {
		_queues[0].emplace_back(event->target(), event);
	}
	
	void post(event_ptr&& event) {
		_queues[0].emplace_back(event->target(), std::move(event));
	}

	void post(target_type target, const event_ptr& event) {
		_queues[0].emplace_back(target, event);
	}

	void post(target_type target, event_ptr&& event) {
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
		_queues[0].erase(
			std::remove_if(_queues[0].begin(), _queues[0].end(),
				[&](const Events::value_type& v) {
					return v.first == target;
				}),
			_queues[0].end());
	}

	/// process events with timeout
	/// @return number of invoked handlers
	size_t process(std::chrono::high_resolution_clock::duration timeout) {
		size_t count = 0;

		std::swap(_queues[0], _queues[1]);

		auto start = std::chrono::high_resolution_clock::now();

		while (!_queues[1].empty()) {
			auto&& p = _queues[1].front();
			count += invoke(p.first, p.second);
			_queues[1].pop_front();

			if (std::chrono::high_resolution_clock::now() - start >= timeout)
				break;
		}

		// insert not processed events to the main queue
		if (!_queues[1].empty()) {
			_queues[0].insert(_queues[0].begin(),
				std::make_move_iterator(_queues[1].begin()),
				std::make_move_iterator(_queues[1].end()));
			
			_queues[1].clear();
		}

		return count;
	}

	/// invoke the event immediately
	/// @return number of invocations
	size_t invoke(const event_ptr& event) {
		size_t count = 0;
		auto range = _handlers.equal_range(event->target());
		for (auto it = range.first; it != range.second; ++it, ++count)
			(*it).second(event.get());
		return count;
	}

	/// invoke the event immediately
	/// @return number of invocations
	size_t invoke(target_type target, const event_ptr& event) {
		size_t count = 0;
		auto range = _handlers.equal_range(target);
		for (auto it = range.first; it != range.second; ++it, ++count)
			(*it).second(event.get());
		return count;
	}

private:
	typedef std::unordered_multimap<target_type, handler_type, already_hash<target_type>> Handlers;
	Handlers _handlers;

	typedef std::deque<std::pair<target_type, event_ptr>> Events;
	std::array<Events, 2> _queues;
};

/// base class for objects which want to subscribe to and automatically unsubscribe from events
template <typename T> class event_subscriber {
public:
	explicit event_subscriber(event_manager& manager)
		: _manager(manager)
	{
	}

	~event_subscriber() {
		for (auto&& s : _subscriptions) {
			bool res = _manager.unsubscribe(s.first, s.second);
			BOOST_ASSERT(res);
		}
	}

	template <typename E>
	void subscribe(void(T::*mf)(E*)) {
		auto handler_hash = _manager.subscribe(mf, static_cast<T*>(this));
		_subscriptions.emplace_back(E::type, handler_hash);
	}

	template <typename E>
	bool subscribed(void(T::*mf)(E*)) const {
		return _manager.subscribed(mf, obj);
	}

	template <typename E>
	void unsubscribe(void(T::*mf)(E*)) {
		size_t handler_hash = 0;
		bool res = _manager.unsubscribe(mf, static_cast<T*>(this), &handler_hash);
		BOOST_ASSERT(res);
		
		auto it = std::find(_subscriptions.begin(), _subscriptions.end(), std::make_pair(E::type, handler_hash));
		if (it != _subscriptions.end())
			_subscriptions.erase(it);
	}

private:
	event_manager& _manager;

	typedef std::deque<std::pair<event_manager::target_type, size_t>> Subscriptions;
	Subscriptions _subscriptions;
};

enum class subscription_mode {
	by_type = 1,
	by_name = 2,
	by_type_and_name = by_type | by_name
};

/// base class for objects which want to be event targets
template <typename T, subscription_mode Mode = subscription_mode::by_type_and_name> class event_target {
public:
	explicit event_target(event_manager& manager, const char* target_name = nullptr)
		: _manager(manager)
		, _target(get_target(target_name))
	{
		event_manager::handler_type handler = std::bind(&event_target::on_event, static_cast<T*>(this), std::placeholders::_1);
		_handler_hash = _manager.subscribe(_target, handler);
	}

	virtual ~event_target() {
		bool res = _manager.unsubscribe(_target, _handler_hash);
		BOOST_ASSERT(res);
	}

	virtual void on_event(event*) = 0;

	static size_t get_target(const char* target_name) {
		typedef std::underlying_type<subscription_mode>::type int_type;

		size_t hash_value = 0;

		int_type mode_ = (int_type)Mode;

		if (mode_ & (int_type)subscription_mode::by_type)
			hash_value = typeid(T).hash_code();

		if (mode_ & (int_type)subscription_mode::by_name) {
			if (target_name)
				hash_value = hash::murmur3_32(target_name, std::strlen(target_name), hash_value);
		}

		return hash_value;
	}

private:
	event_manager& _manager;
	event_manager::target_type _target = 0;
	size_t _handler_hash = 0;
};
