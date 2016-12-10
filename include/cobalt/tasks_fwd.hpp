#ifndef COBALT_TASKS_FWD_HPP_INCLUDED
#define COBALT_TASKS_FWD_HPP_INCLUDED

#pragma once

// Classes in this file:
//     task
//     task_scheduler

#include <cobalt/utility/enumerator.hpp>
#include <cobalt/utility/intrusive.hpp>
#include <cobalt/utility/enum_traits.hpp>

#include <array>
#include <deque>
#include <algorithm>

CO_DEFINE_ENUM_CLASS(
	task_state, uint8_t,
	uninitialized,
	// alive states
	running,
	paused,
	// finished states
	succeeded,
	failed,
	aborted,
	removed
)

CO_DEFINE_ENUM_CLASS(
	task_result, uint8_t,
	success,
	fail,
	abort
)

namespace cobalt {

/// task
class task : public ref_counter<task> {
public:
	task() = default;

	task(const task&) = delete;
	task& operator=(const task&) = delete;

	virtual ~task();

	/// Get raw state
	task_state state() const noexcept { return _state; }
	
	/// Get running state
	bool alive() const noexcept;
	bool finished() const noexcept;

	/// Pause/resume in alive state
	void pause(bool pausing = true) noexcept;
	
	/// Finish task with specified result
	void finish(task_result result = task_result::success) noexcept;

	/// Set continuation resetting old one if any
	/// @return Added task
	task* next(const ref_ptr<task>& next) noexcept;
	task* next(ref_ptr<task>&& next) noexcept;
	
	/// @return First continuation
	task* next() const noexcept { return _next.get(); }
	
	/// Find last continuation in the list including this
	task* last() noexcept;
	
	/// Create a task for waiting specified number of frames
	static task* wait_for_frames(size_t frames);

protected:
	/// Perform one step in the task lifecycle
	/// @return Task for the next step to make `interruption` or nullptr/this to continue
	virtual task* step() = 0;
	
	/// Resets task to uninitialized state to call on_init() from scratch
	virtual void reset() { _state = task_state::uninitialized; }

	/// Called if in uninitialized state before running
	/// @return False for immediately abort the task
	///         True for start running
	virtual bool on_init() { return true; }
	/// Called if finished successfully
	virtual void on_success() {}
	/// Called if finished unsuccessfully
	virtual void on_fail() {}
	/// Called if explicitly aborted or didn't run before destruction
	virtual void on_abort() {}

private:
	friend class task_scheduler;

	/// Set raw state
	void state(task_state state) noexcept { _state = state; }
	
	/// @return Detach continuation
	ref_ptr<task> detach_next() noexcept { return std::move(_next); }

private:
	ref_ptr<task> _next;
	task_state _state = task_state::uninitialized;
};

/// task_scheduler
class task_scheduler {
public:
	task_scheduler() = default;

	task_scheduler(const task_scheduler&) = delete;
	task_scheduler& operator=(const task_scheduler&) = delete;

	~task_scheduler();

	/// Schedule task to execute
	/// @return Added task
	task* schedule(const ref_ptr<task>& task);

	/// Schedule task to execute
	/// @return Added task
	task* schedule(ref_ptr<task>&& task);

	/// Advance tasks with one step
	void run_one_step();

	/// Pause all tasks
	void pause_all(bool pause = true) noexcept;

	/// Abort all tasks
	void abort_all() noexcept;

	/// @return Number of tasks with specified state
	size_t count(task_state state) const noexcept;

	/// @return True if no tasks
	bool empty() const noexcept;
	
	/// Enumerates all tasks with `void handler(task*)`
	template <typename Handler>
	void enumerate(Handler&& handler) const;

private:
	typedef std::deque<ref_ptr<task>> Tasks;
	std::array<Tasks, 2> _tasks;
};

/// Waits for specified amount of seconds
//class wait_for_seconds : public task {
//public:
//	explicit wait_for_seconds(float seconds)
//		: _time(seconds)
//	{
//		if (_time <= 0)
//			exit();
//	}
//
//protected:
//	task* step() override {
//		if ((_time -= dt) <= 0)
//			exit();
//
//		return nullptr;
//	}
//
//private:
//	float _time = 0;
//};
	
} // namespace cobalt

#endif // COBALT_TASKS_FWD_HPP_INCLUDED
