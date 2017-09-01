#ifndef COBALT_TASKS_FWD_HPP_INCLUDED
#define COBALT_TASKS_FWD_HPP_INCLUDED

#pragma once

// Classes in this file:
//     task
//     task_scheduler

#include <cobalt/utility/enum_traits.hpp>
#include <cobalt/utility/intrusive.hpp>
#include <cobalt/utility/enumerator.hpp>

#include <deque>

DEFINE_ENUM(
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

DEFINE_ENUM(
	task_result, uint8_t,
	success,
	fail,
	abort
)

namespace cobalt {

/// task
class task : public local_ref_counter<task> {
public:
	task() = default;
	
	task(task&&) noexcept = default;
	task& operator=(task&&) noexcept = default;

	task(const task&) = delete;
	task& operator=(const task&) = delete;

	virtual ~task() noexcept;

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
	virtual task* step() noexcept = 0;
	
	/// Resets task to uninitialized state
	virtual void reset() noexcept { _state = task_state::uninitialized; }

	/// Called if in uninitialized state before running
	/// @return False to immediately abort the task, or true to start running
	virtual bool on_init() noexcept { return true; }
	/// Called if finished successfully
	virtual void on_success() noexcept {}
	/// Called if finished unsuccessfully
	virtual void on_fail() noexcept {}
	/// Called if explicitly aborted or didn't run before destruction
	virtual void on_abort() noexcept {}

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
	task_scheduler() noexcept = default;
	
	task_scheduler(task_scheduler&&) noexcept = default;
	task_scheduler& operator=(task_scheduler&&) noexcept = default;

	task_scheduler(const task_scheduler&) = delete;
	task_scheduler& operator=(const task_scheduler&) = delete;

	~task_scheduler() noexcept;

	/// Schedule task to execute
	/// @return Added task
	task* schedule(const ref_ptr<task>& task);

	/// Schedule task to execute
	/// @return Added task
	task* schedule(ref_ptr<task>&& task);

	/// Advance tasks with one step
	void step();

	/// Pause all tasks
	void pause_all(bool pause = true) noexcept;

	/// Abort all tasks
	void abort_all() noexcept;

	/// @return Number of tasks with specified state
	size_t count(task_state state) const noexcept;

	/// @return True if no tasks
	bool empty() const noexcept;

private:
	using Tasks = std::deque<ref_ptr<task>>;
	Tasks _tasks;
	
public:
	/// @return Tasks enumerator
	enumerator<Tasks::iterator> tasks() noexcept;
	enumerator<Tasks::const_iterator> tasks() const noexcept;
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
