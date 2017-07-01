#ifndef COBALT_FSM_HPP_INCLUDED
#define COBALT_FSM_HPP_INCLUDED

#pragma once

#include "fsm_fwd.hpp"

#include <type_traits>

#include <boost/assert.hpp>

namespace cobalt {
namespace fsm {

inline state_base::state_base(std::initializer_list<transition> transitions) {
	add(transitions);
}

inline void state_base::add(std::initializer_list<transition> transitions) {
	for (auto&& transition : transitions)
		BOOST_VERIFY(add(transition));
}

inline bool state_base::add(const transition& transition) {
	return _transitions.insert(transition).second;
}

inline bool state_base::add(transition&& transition) {
	return _transitions.insert(std::move(transition)).second;
}

inline bool state_base::can_transit_to(type_index state_type) const noexcept {
	for (auto&& kvp : _transitions) {
		if (kvp.second == state_type)
			return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

template <typename T>
inline state<T>::state(std::initializer_list<transition> transitions)
	: state_base(transitions)
{
}

template <typename T>
inline bool state<T>::accept_event(type_index event_type) {
	auto it = _transitions.find(event_type);
	if (it != _transitions.end())
		return machine()->enter((*it).second);
	return false;
}

template <typename T>
template <typename State>
inline bool state<T>::can_transit_to() const noexcept {
	return can_transit_to(type_id<State>());
}

template <typename T>
template <typename Event>
inline bool state<T>::accept_event() {
	return accept_event(type_id<Event>());
}

template <typename T>
inline typename state<T>::machine_type* state<T>::machine() const noexcept {
	BOOST_ASSERT(_machine);
	return _machine;
}

template <typename T>
inline void state<T>::machine(typename state<T>::machine_type* machine) noexcept {
	BOOST_ASSERT(machine);
	_machine = machine;
}

////////////////////////////////////////////////////////////////////////////////

template <typename T>
inline state_machine<T>::state_machine(std::initializer_list<std::unique_ptr<state_type>> states) {
	add(states);
}

template <typename T>
inline void state_machine<T>::add(std::initializer_list<std::unique_ptr<state_type>> states) {
	for (auto&& state : states) {
		BOOST_VERIFY(add(std::move(const_cast<std::unique_ptr<state_type>&>(state))));
	}
}

template <typename T>
inline bool state_machine<T>::add(std::unique_ptr<state_type> state) {
	state->machine(this);
	return _states.emplace(type_id_runtime(*state), std::move(state)).second;
}

template <typename T>
inline bool state_machine<T>::send(type_index event_type) {
	return _current_state ? _current_state->accept_event(event_type) : false;
}

template <typename T>
inline bool state_machine<T>::can_enter(type_index state_type) const noexcept {
	return _current_state ? _current_state->can_transit_to(state_type) : true;
}

template <typename T>
inline bool state_machine<T>::enter(type_index state_type) {
	if (!can_enter(state_type))
		return false;
	
	if (auto state = state_for(state_type)) {
		if (_current_state)
			_current_state->leave(state);
		state->enter(_current_state);
		_current_state = state;
		return true;
	}
	
	return false;
}

template <typename T>
inline typename state_machine<T>::state_type* state_machine<T>::state_for(type_index state_type) const noexcept {
	auto it = _states.find(state_type);
	if (it == _states.end())
		return nullptr;
	return (*it).second.get();
}

template <typename T>
inline typename state_machine<T>::state_type* state_machine<T>::current_state() const noexcept {
	BOOST_ASSERT(_current_state); return _current_state;
}

template <typename T>
inline bool state_machine<T>::terminated() const noexcept {
	return !_current_state;
}

template <typename T>
inline bool state_machine<T>::terminate() {
	if (_current_state) {
		_current_state->leave(nullptr);
		_current_state = nullptr;
		return true;
	}
	return false;
}

template <typename T>
template <typename State>
inline bool state_machine<T>::can_enter() const noexcept {
	return can_enter(type_id<State>());
}

template <typename T>
template <typename State>
inline bool state_machine<T>::enter() {
	return enter(type_id<State>());
}

template <typename T>
template <typename State>
inline typename state_machine<T>::state_type* state_machine<T>::state_for() const noexcept {
	return state_for(type_id<State>());
}

template <typename T>
template <typename Event>
inline bool state_machine<T>::send() {
	return send(type_id<Event>());
}

////////////////////////////////////////////////////////////////////////////////

template <typename Event, typename State, typename = std::enable_if_t<std::is_base_of<state_base, State>::value>>
inline transition make_transition() noexcept {
	return std::make_pair(type_id<Event>(), type_id<State>());
}

template <typename State, typename = std::enable_if_t<std::is_base_of<state_base, State>::value>>
inline std::unique_ptr<State> make_state(std::initializer_list<transition> transitions) {
	return std::make_unique<State>(transitions);
}

} // namespace fsm
} // namespace cobalt

#endif // COBALT_FSM_HPP_INCLUDED
