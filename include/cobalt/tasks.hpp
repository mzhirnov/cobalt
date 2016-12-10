#ifndef COBALT_TASKS_HPP_INCLUDED
#define COBALT_TASKS_HPP_INCLUDED

#pragma once

#include <cobalt/tasks_fwd.hpp>

namespace cobalt {

////////////////////////////////////////////////////////////////////////////////
// task

inline task::~task() {
	// Abort continuation if it hasn't been run
	if (_next)
		_next->on_abort();
}

inline bool task::alive() const noexcept {
	switch (_state) {
	case task_state::running:
	case task_state::paused:
		return true;
	default:
		return false;
	}
}

inline bool task::finished() const noexcept {
	switch (_state) {
	case task_state::succeeded:
	case task_state::failed:
	case task_state::aborted:
	case task_state::removed:
		return true;
	default:
		return false;
	}
}

inline void task::pause(bool pausing) noexcept {
	BOOST_ASSERT(alive());
	state(pausing ? task_state::paused : task_state::running);
}

inline void task::finish(task_result result) noexcept {
	BOOST_ASSERT(!finished());
	switch (result) {
	case task_result::success:
		state(task_state::succeeded);
		break;
	case task_result::fail:
		state(task_state::failed);
		break;
	case task_result::abort:
		state(task_state::aborted);
		break;
	default:
		BOOST_ASSERT_MSG(false, "Unknown task result");
	}
}

inline task* task::next(const ref_ptr<task>& next) noexcept {
	_next = next;
	return _next.get();
}

inline task* task::next(ref_ptr<task>&& next) noexcept {
	_next = std::move(next);
	return _next.get();
}

inline task* task::last() noexcept {
	// Find last node in the task list
	auto last = this;
	while (auto p = last->next())
		last = p;
	return last;
}

inline task* task::wait_for_frames(size_t frames) {
	class wait_for_frames_task : public task {
	public:
		explicit wait_for_frames_task(size_t frames) noexcept
			: _count(frames)
		{
			// Finish immediantely if no frames to wait
			if (!_count)
				finish();
		}

		task* step() override {
			if (!--_count)
				finish();
			return nullptr;
		}
	
	private:
		size_t _count = 0;
	};
	
	return new wait_for_frames_task(frames);
}

////////////////////////////////////////////////////////////////////////////////
// task_scheduler

inline task_scheduler::~task_scheduler() {
	// Abort all unfinished tasks
	for (auto&& task : _tasks[0])
		task->on_abort();
}

inline task* task_scheduler::schedule(const ref_ptr<task>& task) {
	BOOST_ASSERT(task);
	_tasks[0].push_back(task);
	return _tasks[0].back().get();
}

inline task* task_scheduler::schedule(ref_ptr<task>&& task) {
	BOOST_ASSERT(task);
	_tasks[0].push_back(std::move(task));
	return _tasks[0].back().get();
}

inline void task_scheduler::run_one_step() {
	// Swap queues for safe scheduling new tasks from handlers
	std::swap(_tasks[0], _tasks[1]);

	for (auto&& curr : _tasks[1]) {
		if (curr->state() == task_state::uninitialized)
			curr->state(curr->on_init() ? task_state::running : task_state::aborted);

		if (curr->state() == task_state::running) {
			// Interruption task
			ref_ptr<task> intr = curr->step();
			
			if (intr && intr != curr) {
				// Move current task to the end of the continuation list
				intr->last()->next(std::move(curr));
				curr = std::move(intr);
			}
		}

		if (curr->finished()) {
			switch (curr->state()) {
			case task_state::succeeded:
				curr->on_success();
				// Schedule continuation on success
				if (auto next = curr->detach_next())
					schedule(std::move(next));
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
	}

	// Remove completed tasks
	_tasks[1].erase(std::remove_if(_tasks[1].begin(), _tasks[1].end(),
		[](auto&& task) {
			if (task->finished()) {
				task->state(task_state::removed);
				return true;
			}
			return false;
		}),
		_tasks[1].end()
	);

	// Append new tasks added during this step to the main queue
	if (!_tasks[0].empty()) {
		_tasks[1].insert(_tasks[1].end(),
			std::make_move_iterator(_tasks[0].begin()),
			std::make_move_iterator(_tasks[0].end()));
		
		_tasks[0].clear();
	}

	// Swap queues back
	std::swap(_tasks[0], _tasks[1]);
}

inline void task_scheduler::pause_all(bool pause) noexcept {
	for (auto&& task : _tasks[0])
		task->pause(pause);
}

inline void task_scheduler::abort_all() noexcept {
	for (auto&& task : _tasks[0])
		task->finish(task_result::abort);
}

inline size_t task_scheduler::count(task_state state) const noexcept {
	size_t count = 0;
	for (auto&& task : _tasks[0])
		count += task->state() == state ? 1 : 0;
	return count;
}

inline bool task_scheduler::empty() const noexcept {
	return _tasks[0].empty();
}

template <typename Handler>
inline void task_scheduler::enumerate(Handler&& handler) const {
	for (auto&& task : _tasks[0])
		handler(task.get());
}

} // namespace cobalt

#endif // COBALT_TASKS_HPP_INCLUDED
