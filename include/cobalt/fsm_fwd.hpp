#ifndef COBALT_FSM_FWD_HPP_INCLUDED
#define COBALT_FSM_FWD_HPP_INCLUDED

#pragma once

// Classes in this file:
//     state
//     state_machine

#include <boost/type_index.hpp>
#include <boost/functional/hash.hpp>

#include <memory>
#include <unordered_map>

namespace cobalt {
namespace fsm {

using boost::typeindex::type_index;
using boost::typeindex::type_id;
using boost::typeindex::type_id_runtime;

using transition_t = std::pair<type_index, type_index>;

/// Abstract base class for state
class state_base {
public:
	state_base() noexcept = default;
	state_base(std::initializer_list<transition_t> transitions);
	
	virtual ~state_base() = default;
	
	virtual void enter(state_base* from) = 0;
	virtual void leave(state_base* to) = 0;
	
	void add(std::initializer_list<transition_t> transitions);
	bool add(const transition_t& transition);
	bool add(transition_t&& transition);
	
	bool can_transit_to(type_index state_type) const noexcept;
	
protected:
	using Transitions = std::unordered_map<type_index, type_index, boost::hash<type_index>>;
	Transitions _transitions;
};

template <typename T> class state_machine;

/// State class
template <typename T>
class state : public state_base, public T {
public:
	using machine_type = state_machine<T>;
	
	state() noexcept = default;
	state(std::initializer_list<transition_t> transitions);
	
	using state_base::can_transit_to;
	
	bool accept_event(type_index event_type);
	
	template <typename State> bool can_transit_to() const noexcept;
	template <typename Event> bool accept_event();
	
	machine_type* machine() const noexcept;
	
private:
	void machine(machine_type* machine) noexcept;

private:
	friend class state_machine<T>;
	
	machine_type* _machine = nullptr;
};

/// State machine
template <typename T>
class state_machine {
public:
	using state_type = state<T>;
	
	state_machine() noexcept = default;
	state_machine(std::initializer_list<std::unique_ptr<state_type>> states);
	
	void add(std::initializer_list<std::unique_ptr<state_type>> states);
	bool add(std::unique_ptr<state_type> state);
	
	bool can_enter(type_index state_type) const noexcept;
	bool enter(type_index state_type);
	bool send(type_index event_type);
	
	state_type* state_for(type_index state_type) const noexcept;
	state_type* current_state() const noexcept;
	
	bool terminated() const noexcept;
	bool terminate();
	
	template <typename State> bool can_enter() const noexcept;
	template <typename State> bool enter();
	template <typename Event> bool send();
	template <typename State> state_type* state_for() const noexcept;
	
private:
	using States = std::unordered_map<type_index, std::unique_ptr<state_type>, boost::hash<type_index>>;
	States _states;
	
	state_type* _current_state = nullptr;
	
};

} // namespace fsm
} // namespace cobalt

#endif // COBALT_FSM_FWD_HPP_INCLUDED
