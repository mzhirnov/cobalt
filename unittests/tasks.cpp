#include "catch.hpp"
#include <cobalt/tasks.hpp>

using namespace cobalt;

class test_task : public task {
public:
	explicit test_task(size_t steps) : _steps(steps) { if (!_steps) finish(); }
	
	size_t steps() const { return _steps; }
	
protected:
	virtual task* step() override { if (!--_steps) finish(); return nullptr; }
	
private:
	size_t _steps = 1;
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
}
