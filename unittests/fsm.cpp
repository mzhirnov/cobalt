#include "catch.hpp"
#include <cobalt/fsm.hpp>

using namespace cobalt;

// State machine interface
class updatable {
public:
	virtual void update() = 0;
};

using state_machine = fsm::state_machine<updatable>;

class state_b;
class state_c;

class state_a : public state_machine::state_type {
public:
	state_a() noexcept = default;
	state_a(std::initializer_list<fsm::transition> transitions)
		: state_machine::state_type(transitions)
	{}
	
	virtual void enter(state_base* from) override {  }
	virtual void leave(state_base* to) override {  }
	
	virtual void update() override {
		machine()->enter<state_b>();
	}
};

class state_b : public state_machine::state_type {
public:
	state_b() noexcept = default;
	state_b(std::initializer_list<fsm::transition> transitions)
		: state_machine::state_type(transitions)
	{}
	
	virtual void enter(state_base* from) override {  }
	virtual void leave(state_base* to) override {  }
	
	virtual void update() override {
		machine()->enter<state_c>();
	}
};

class state_c : public state_machine::state_type {
public:
	state_c() noexcept = default;
	state_c(std::initializer_list<fsm::transition> transitions)
		: state_machine::state_type(transitions)
	{}
	
	virtual void enter(state_base* from) override {  }
	virtual void leave(state_base* to) override {  }
	
	virtual void update() override {
		machine()->terminate();
	}
};

struct event_next {};
struct event_prev {};
struct event_up {};

TEST_CASE("fsm", "[fsm]") {
	state_machine machine{
		fsm::make_state<state_a>({
			fsm::make_transition<event_next, state_b>(),
			fsm::make_transition<event_prev, state_c>()
		}),
		fsm::make_state<state_b>({
			fsm::make_transition<event_next, state_c>(),
			fsm::make_transition<event_prev, state_a>()
		}),
		fsm::make_state<state_c>({
			fsm::make_transition<event_next, state_a>(),
			fsm::make_transition<event_prev, state_b>()
		})
	};
	
	SECTION("events") {
		REQUIRE(machine.enter<state_a>());
	
		REQUIRE(machine.send<event_next>());
		REQUIRE(machine.send<event_next>());
		REQUIRE(machine.send<event_next>());
		REQUIRE(machine.send<event_next>());
		
		REQUIRE(!machine.send<event_up>());
		REQUIRE(!machine.send<event_up>());
		
		REQUIRE(machine.send<event_prev>());
		REQUIRE(machine.send<event_prev>());
		REQUIRE(machine.send<event_prev>());
		REQUIRE(machine.send<event_prev>());
		
		REQUIRE(machine.terminate());
	}
	
	SECTION("interface") {
		machine.enter<state_a>();
		while (!machine.terminated())
			machine.current_state()->update();
	}
}
