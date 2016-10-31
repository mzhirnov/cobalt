#pragma once

#include "enumerator.h"

#include <array>
#include <deque>
#include <algorithm>

#include <boost/smart_ptr/intrusive_ptr.hpp>
#include <boost/smart_ptr/intrusive_ref_counter.hpp>

enum class task_state {
	uninitialized,
	running,
	paused,
	succeeded,
	failed,
	aborted,
	removed
};

enum class task_exit {
	success,
	fail,
	abort
};

typedef boost::intrusive_ptr<class task> task_ptr;

/// task
class task : public boost::intrusive_ref_counter<task, boost::thread_unsafe_counter> {
public:
	task() = default;

	task(const task&) = delete;
	task& operator=(const task&) = delete;

	virtual ~task() {
		// abort continuation if it hasn't been run
		if (_next)
			_next->on_abort();
	}

	/// query state
	task_state state() const { return _state; }

	/// query state
	bool running() const { return _state == task_state::running; }
	bool paused() const { return _state == task_state::paused; }
	bool succeeded() const { return _state == task_state::succeeded; }
	bool failed() const { return _state == task_state::failed; }
	bool aborted() const { return _state == task_state::aborted; }
	bool removed() const { return _state == task_state::removed; }
	
	/// query state
	bool alive() const { return running() || paused(); }
	bool completed() const { return succeeded() || failed() || aborted() || removed(); }

	/// pause/resume
	void pause(bool pausing = true) { BOOST_ASSERT(alive()); state(pausing ? task_state::paused : task_state::running); }

	/// attach continuation
	/// @return added task
	task* then(const task_ptr& next) { _next = next; return _next.get(); }
	task* then(task_ptr&& next) { _next = std::move(next); return _next.get(); }

protected:
	/// exit task fith specified state
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
			BOOST_ASSERT_MSG(false, "unknown exit state");
		}
	}

	/// @return next step task to make `interruption`, or nullptr to continue with this one
	virtual task* step() = 0;

	/// @return false to immediately abort the task
	virtual bool on_init() { return true; }
	virtual void on_success() {}
	virtual void on_fail() {}
	virtual void on_abort() {}

private:
	friend class task_manager;

	/// set raw state
	void state(task_state state) { _state = state; }

	/// @return continuation
	task* next() const { return _next.get(); }
	
	/// @return and detach continuation
	task_ptr detach_next() { return std::move(_next); }

	/// append task to the continuation list
	void append_tail(const task_ptr& next) { find_last()->then(next); }
	void append_tail(task_ptr&& next) { find_last()->then(std::move(next)); }

	/// find the last task in the continuation list
	task* find_last() {
		// find last node in the task list
		auto last = this;
		while (auto p = last->next())
			last = p;
		return last;
	}

private:
	task_state _state = task_state::uninitialized;
	task_ptr _next;
};

/// task_manager
class task_manager {
public:
	task_manager() = default;

	task_manager(const task_manager&) = delete;
	task_manager& operator=(const task_manager&) = delete;

	~task_manager() {
		// abort all unfinished tasks
		for (auto&& task : _tasks[0])
			task->on_abort();
	}

	/// schedule task to execute
	/// @return added task
	task* schedule(const task_ptr& task) {
		BOOST_ASSERT(task && !task->completed());
		_tasks[0].push_back(task);
		return _tasks[0].back().get();
	}

	/// schedule task to execute
	/// @return added task
	task* schedule(task_ptr&& task) {
		BOOST_ASSERT(task && !task->completed());
		_tasks[0].push_back(std::move(task));
		return _tasks[0].back().get();
	}

	/// process tasks
	void process() {
		// swap queues for safe scheduling new tasks from handlers
		std::swap(_tasks[0], _tasks[1]);

		for (auto&& task : _tasks[1]) {
			if (task->state() == task_state::uninitialized)
				task->state(task->on_init() ? task_state::running : task_state::aborted);

			task_ptr next;

			if (task->running())
				next = task->step();

			if (task->completed()) {
				switch (task->state()) {
				case task_state::succeeded:
					task->on_success();
					if (task->next())
						schedule(task->detach_next());
					break;
				case task_state::failed:
					task->on_fail();
					break;
				case task_state::aborted:
					task->on_abort();
					break;
				}
			}
			else if (next && next != task && !next->completed()) {
				// move current task to the end of the next task list
				next->append_tail(std::move(task));
				// overwrite current task with the next one to allow `interruptions`
				task = std::move(next);
			}
		}

		// remove completed tasks
		_tasks[1].erase(std::remove_if(_tasks[1].begin(), _tasks[1].end(),
			[](task_ptr& task) {
				if (task->completed()) {
					task->state(task_state::removed);
					return true;
				}
				return false;
			}),
			_tasks[1].end()
		);

		// append new tasks to the main queue
		if (!_tasks[0].empty()) {
			_tasks[1].insert(_tasks[1].end(),
				std::make_move_iterator(_tasks[0].begin()),
				std::make_move_iterator(_tasks[0].end()));
			
			_tasks[0].clear();
		}

		// swap queues back
		std::swap(_tasks[0], _tasks[1]);
	}

	/// pause all tasks
	void pause_all(bool pause = true) {
		for (auto&& task : _tasks[0])
			task->pause(pause);
	}

	/// abort all tasks
	void abort_all() {
		for (auto&& task : _tasks[0])
			task->exit(task_exit::abort);
	}

	/// @return number of tasks with specified state
	size_t count(task_state state = task_state::running) const {
		size_t count = 0;
		for (auto&& task : _tasks[0])
			count += task->state() == state ? 1 : 0;
		return count;
	}

	/// @return true if no tasks
	bool empty() const {
		return _tasks[0].empty();
	}

private:
	typedef std::deque<task_ptr> Tasks;
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
