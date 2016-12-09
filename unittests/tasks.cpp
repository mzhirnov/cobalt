#include "catch.hpp"
#include <cobalt/tasks.hpp>

using namespace cobalt;

class my_task : public task {
public:
	my_task() = default;
	
protected:
	virtual task* step() override {
		exit(task_exit::success);
		return nullptr;
	}
};

TEST_CASE("tasks") {
	task_manager manager;
	
	auto task = make_ref<my_task>();
	
	REQUIRE(manager.empty());
	REQUIRE(manager.count(task_state::uninitialized) == 0);
	REQUIRE(manager.count(task_state::running) == 0);
	REQUIRE(task->use_count() == 1);
	
	manager.schedule(task);
	
	REQUIRE_FALSE(manager.empty());
	REQUIRE(manager.count(task_state::uninitialized) == 1);
	REQUIRE(manager.count(task_state::running) == 0);
	REQUIRE(task->use_count() == 2);
	
	manager.process();
	
	REQUIRE(manager.empty());
	REQUIRE(manager.count(task_state::uninitialized) == 0);
	REQUIRE(manager.count(task_state::running) == 0);
	REQUIRE(task->use_count() == 1);
}
