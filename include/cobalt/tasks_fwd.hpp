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
	running,
	paused,
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

	/// Query state
	task_state state() const noexcept { return _state; }

	/// Query state
	bool running() const noexcept { return _state == task_state::running; }
	bool paused() const noexcept { return _state == task_state::paused; }
	bool succeeded() const noexcept { return _state == task_state::succeeded; }
	bool failed() const noexcept { return _state == task_state::failed; }
	bool aborted() const noexcept { return _state == task_state::aborted; }
	bool removed() const noexcept { return _state == task_state::removed; }
	
	/// Query state
	bool alive() const noexcept { return running() || paused(); }
	bool completed() const noexcept { return succeeded() || failed() || aborted() || removed(); }

	/// Pause/resume
	void pause(bool pausing = true) noexcept;

	/// Attach continuation
	/// @return Added task
	task* then(const ref_ptr<task>& next) noexcept;
	task* then(ref_ptr<task>&& next) noexcept;

protected:
	/// Exit task fith specified state
	void finish(task_result result = task_result::success) noexcept;

	/// @return Task for next step to make `interruption`, or nullptr to continue with this one
	virtual task* step() = 0;

	/// @return False to immediately abort the task
	virtual bool on_init() { return true; }
	virtual void on_success() {}
	virtual void on_fail() {}
	virtual void on_abort() {}

private:
	friend class task_scheduler;

	/// Set raw state
	void state(task_state state) noexcept { _state = state; }

	/// @return Continuation
	task* next() const noexcept { return _next.get(); }
	
	/// @return Detach continuation
	ref_ptr<task> detach_next() noexcept { return std::move(_next); }

	/// Append task to the continuation list
	void append_tail(const ref_ptr<task>& next) noexcept;
	void append_tail(ref_ptr<task>&& next) noexcept;

	/// Find the last task in the continuation list
	task* find_last() noexcept;

private:
	task_state _state = task_state::uninitialized;
	ref_ptr<task> _next;
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

private:
	typedef std::deque<ref_ptr<task>> Tasks;
	std::array<Tasks, 2> _tasks;
};

/// Waits for specified amount of frames
class wait_for_frames : public task {
public:
	explicit wait_for_frames(size_t frames) noexcept
		: _count(frames)
	{
		if (!_count)
			finish();
	}

protected:
	task* step() override {
		if (!--_count)
			finish();

		return nullptr;
	}

private:
	size_t _count = 0;
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
