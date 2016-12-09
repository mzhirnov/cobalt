#ifndef COBALT_TASKS_HPP_INCLUDED
#define COBALT_TASKS_HPP_INCLUDED

#pragma once

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
	task_exit, uint8_t,
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

	virtual ~task() {
		// Abort continuation if it hasn't been run
		if (_next)
			_next->on_abort();
	}

	/// Query state
	task_state state() const { return _state; }

	/// Query state
	bool running() const { return _state == task_state::running; }
	bool paused() const { return _state == task_state::paused; }
	bool succeeded() const { return _state == task_state::succeeded; }
	bool failed() const { return _state == task_state::failed; }
	bool aborted() const { return _state == task_state::aborted; }
	bool removed() const { return _state == task_state::removed; }
	
	/// Query state
	bool alive() const { return running() || paused(); }
	bool completed() const { return succeeded() || failed() || aborted() || removed(); }

	/// Pause/resume
	void pause(bool pausing = true) { BOOST_ASSERT(alive()); state(pausing ? task_state::paused : task_state::running); }

	/// Attach continuation
	/// @return Added task
	task* then(const ref_ptr<task>& next) { _next = next; return _next.get(); }
	task* then(ref_ptr<task>&& next) { _next = std::move(next); return _next.get(); }

protected:
	/// Exit task fith specified state
	void exit(task_exit exit_state = task_exit::success) {
		switch (exit_state) {
		case task_exit::success:
			state(task_state::succeeded);
			break;
		case task_exit::fail:
			state(task_state::failed);
			break;
		case task_exit::abort:
			state(task_state::aborted);
			break;
		default:
			BOOST_ASSERT_MSG(false, "Unknown exit state");
		}
	}

	/// @return Next step task to make `interruption`, or nullptr to continue with this one
	virtual task* step() = 0;

	/// @return False to immediately abort the task
	virtual bool on_init() { return true; }
	virtual void on_success() {}
	virtual void on_fail() {}
	virtual void on_abort() {}

private:
	friend class task_manager;

	/// Set raw state
	void state(task_state state) { _state = state; }

	/// @return Continuation
	task* next() const { return _next.get(); }
	
	/// @return Detach continuation
	ref_ptr<task> detach_next() { return std::move(_next); }

	/// Append task to the continuation list
	void append_tail(const ref_ptr<task>& next) { find_last()->then(next); }
	void append_tail(ref_ptr<task>&& next) { find_last()->then(std::move(next)); }

	/// Find the last task in the continuation list
	task* find_last() {
		// Find last node in the task list
		auto last = this;
		while (auto p = last->next())
			last = p;
		return last;
	}

private:
	task_state _state = task_state::uninitialized;
	ref_ptr<task> _next;
};

/// task_manager
class task_manager {
public:
	task_manager() = default;

	task_manager(const task_manager&) = delete;
	task_manager& operator=(const task_manager&) = delete;

	~task_manager() {
		// Abort all unfinished tasks
		for (auto&& task : _tasks[0])
			task->on_abort();
	}

	/// Schedule task to execute
	/// @return Sdded task
	task* schedule(const ref_ptr<task>& task) {
		BOOST_ASSERT(task && !task->completed());
		_tasks[0].push_back(task);
		return _tasks[0].back().get();
	}

	/// Schedule task to execute
	/// @return Sdded task
	task* schedule(ref_ptr<task>&& task) {
		BOOST_ASSERT(task && !task->completed());
		_tasks[0].push_back(std::move(task));
		return _tasks[0].back().get();
	}

	/// Process tasks
	void process() {
		// Swap queues for safe scheduling new tasks from handlers
		std::swap(_tasks[0], _tasks[1]);

		for (auto&& curr : _tasks[1]) {
			if (curr->state() == task_state::uninitialized)
				curr->state(curr->on_init() ? task_state::running : task_state::aborted);

			// Interruption task
			ref_ptr<task> intr;

			if (curr->running())
				intr = curr->step();

			if (curr->completed()) {
				switch (curr->state()) {
				case task_state::succeeded:
					curr->on_success();
					// Schedule continuation on success
					if (curr->next())
						schedule(curr->detach_next());
					break;
				case task_state::failed:
					curr->on_fail();
					break;
				case task_state::aborted:
					curr->on_abort();
					break;
				default:
					break;
				}
			}
			else if (intr && intr != curr && !intr->completed()) {
				// Move current task to the end of the continuation task list
				intr->append_tail(std::move(curr));
				// Overwrite current task with the interruption one
				curr = std::move(intr);
			}
		}

		// Remove completed tasks
		_tasks[1].erase(std::remove_if(_tasks[1].begin(), _tasks[1].end(),
			[](ref_ptr<task>& task) {
				if (task->completed()) {
					task->state(task_state::removed);
					return true;
				}
				return false;
			}),
			_tasks[1].end()
		);

		// Append new tasks to the main queue
		if (!_tasks[0].empty()) {
			_tasks[1].insert(_tasks[1].end(),
				std::make_move_iterator(_tasks[0].begin()),
				std::make_move_iterator(_tasks[0].end()));
			
			_tasks[0].clear();
		}

		// Swap queues back
		std::swap(_tasks[0], _tasks[1]);
	}

	/// Pause all tasks
	void pause_all(bool pause = true) {
		for (auto&& task : _tasks[0])
			task->pause(pause);
	}

	/// Abort all tasks
	void abort_all() {
		for (auto&& task : _tasks[0])
			task->exit(task_exit::abort);
	}

	/// @return Number of tasks with specified state
	size_t count(task_state state) const {
		size_t count = 0;
		for (auto&& task : _tasks[0])
			count += task->state() == state ? 1 : 0;
		return count;
	}

	/// @return True if no tasks
	bool empty() const {
		return _tasks[0].empty();
	}

private:
	typedef std::deque<ref_ptr<task>> Tasks;
	std::array<Tasks, 2> _tasks;

public:
	enumerator<Tasks::const_iterator> tasks() const {
		return make_enumerator(_tasks[0]);
	}
};

/// Waits for specified amount of frames
class wait_for_frames : public task {
public:
	explicit wait_for_frames(size_t frames)
		: _count(frames)
	{
		if (!_count)
			exit();
	}

protected:
	task* step() override {
		if (!--_count)
			exit();

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

#endif // COBALT_TASKS_HPP_INCLUDED
