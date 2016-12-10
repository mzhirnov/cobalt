#include "catch.hpp"
#include <cobalt/tasks.hpp>

#include <iostream>

using namespace cobalt;

class test_task : public task {
public:
	explicit test_task(size_t steps)
		: _steps(steps)
	{
		if (!_steps)
			finish();
	}
	
	size_t get_steps() const noexcept { return _steps; }
	
protected:
	virtual task* step() override {
		if (!--_steps)
			finish();
		return nullptr;
	}
	
private:
	size_t _steps = 1;
};

class wait_task : public task {
public:
	explicit wait_task(size_t frames) : _frames(frames) {}
	
	size_t get_frames() const noexcept { return _frames; }
	size_t get_state() const noexcept { return _state; }

protected:
	virtual task* step() override {
		switch (++_state) {
		case 1:
			break;
		case 2:
			return wait_for_frames(_frames);
		case 3:
			finish();
			break;
		default:
			break;
		}
		return nullptr;
	}
	
private:
	size_t _frames = 0;
	size_t _state = 0;
};

TEST_CASE("tasks") {
	task_scheduler scheduler;
	
	SECTION("schedule one step task") {
		auto task = make_ref<test_task>(1);
		
		REQUIRE(scheduler.empty());
		REQUIRE(scheduler.count(task_state::uninitialized) == 0);
		REQUIRE(scheduler.count(task_state::running) == 0);
		REQUIRE(task->use_count() == 1);
		
		scheduler.schedule(task);
		
		REQUIRE_FALSE(scheduler.empty());
		REQUIRE(scheduler.count(task_state::uninitialized) == 1);
		REQUIRE(scheduler.count(task_state::running) == 0);
		REQUIRE(task->use_count() == 2);
		
		scheduler.run_one_step();
		
		REQUIRE(scheduler.empty());
		REQUIRE(scheduler.count(task_state::uninitialized) == 0);
		REQUIRE(scheduler.count(task_state::running) == 0);
		REQUIRE(task->use_count() == 1);
	}
	
	SECTION("wait_for_frames") {
		auto task = make_ref<wait_task>(2);
		
		scheduler.schedule(task);
		
		scheduler.run_one_step();
		
		REQUIRE(task->state() == task_state::running);
		REQUIRE(task->get_state() == 1);
		
		scheduler.run_one_step();
		
		REQUIRE(task->state() == task_state::running);
		REQUIRE(task->get_state() == 2);
		
		for (int i = 0; i < task->get_frames(); ++i) {
			std::cout << i + 1 << std::endl;
			
			scheduler.run_one_step();

			REQUIRE(task->state() == task_state::running);
			REQUIRE(task->get_state() == 2);
		}
		
		scheduler.run_one_step();
		
		REQUIRE(task->state() == task_state::removed);
		REQUIRE(task->get_state() == 3);
	}
}
