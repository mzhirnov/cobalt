#include "catch.hpp"
#include <cobalt/events.hpp>

using namespace cobalt;

class simple_event : public event {
	IMPLEMENT_EVENT_TARGET("simple_event")
};

class test_event : public event {
	IMPLEMENT_EVENT_TARGET("test_event")
public:
	explicit test_event(const char* name)
		: _name(name)
	{
	}
	
	const char* name() const { return _name.c_str(); }
	
private:
	std::string _name;
};

struct my_subscriber : event_handler<my_subscriber> {
	explicit my_subscriber(event_dispatcher& dispatcher) : event_handler(dispatcher) {
		// Subscribe custom method
		subscribe(&this_type::on_test_event);
	}
	
	void on_test_event(test_event* event) {
		event->handled(true);
	}
};

struct my_subscriber2 : event_handler<my_subscriber2> {
	explicit my_subscriber2(event_dispatcher& dispatcher) : event_handler(dispatcher) {
		// Subscribe default method
		subscribe<test_event>();
	}
	
	void on_event(test_event* event) {
		event->handled(true);
	}
};

struct my_event_target : event_handler<my_event_target> {
	explicit my_event_target(event_dispatcher& dispatcher) : event_handler(dispatcher) {
		// Respond with default method
		respond<test_event>(identifier("do a test"));
		// Respond with custom method
		respond(identifier("do another test"), &this_type::on_simple_event);
	}
	
	void on_target_event(test_event* event) {
		event->handled(true);
	}
	
	void on_simple_event(simple_event* event) {
		event->handled(true);
	}
};

TEST_CASE("event_dispatcher") {
	event_dispatcher dispatcher;
	
	auto event = make_ref<test_event>("inst1");
	
	SECTION("event preconditions") {
		REQUIRE(event->use_count() == 1);
		REQUIRE(event->handled() == false);
	}
	
	SECTION("post event w/o subscriber") {
		dispatcher.post(event);
		
		REQUIRE(event->use_count() == 2);
		REQUIRE(event->handled() == false);
		
		dispatcher.dispatch();
		
		REQUIRE(event->use_count() == 1);
		REQUIRE(event->handled() == false);
	}
	
	SECTION("post event with subscriber") {
		my_subscriber subscriber(dispatcher);

		REQUIRE(dispatcher.subscribed(&my_subscriber::on_test_event, &subscriber));
		REQUIRE(subscriber.subscribed(&my_subscriber::on_test_event));
		
		REQUIRE(dispatcher.connected(&subscriber, test_event::static_target()));
		REQUIRE(subscriber.connected(test_event::static_target()));
		
		dispatcher.post(event);

		REQUIRE(event->handled() == false);
		
		dispatcher.dispatch();

		REQUIRE(event->handled() == true);
	}
	
	SECTION("post event with subscriber 2") {
		my_subscriber2 subscriber(dispatcher);

		REQUIRE((dispatcher.subscribed<my_subscriber2, test_event>(&my_subscriber2::on_event, &subscriber)));
		REQUIRE(subscriber.subscribed<test_event>());
		
		REQUIRE(dispatcher.connected(&subscriber, test_event::static_target()));
		REQUIRE(subscriber.connected(test_event::static_target()));
		
		dispatcher.post(event);

		REQUIRE(event->handled() == false);
		
		dispatcher.dispatch();

		REQUIRE(event->handled() == true);
	}
	
	SECTION("invoke event w/o subscriber") {
		REQUIRE(event->handled() == false);
		REQUIRE(event->use_count() == 1);
		
		dispatcher.invoke(event);

		REQUIRE(event->handled() == false);
		REQUIRE(event->use_count() == 1);
	}
	
	SECTION("invoke event with subscriber") {
		my_subscriber subscriber(dispatcher);
		
		REQUIRE(event->handled() == false);
		REQUIRE(event->use_count() == 1);
		
		dispatcher.invoke(event);

		REQUIRE(event->handled() == true);
		REQUIRE(event->use_count() == 1);
	}
	
	SECTION("invoke event target") {
		my_event_target target(dispatcher);
		
		SECTION("invoke with created custom target") {
			REQUIRE(target.responds<test_event>(identifier("do a test")));
			REQUIRE_FALSE(target.responds<test_event>(identifier("do another test")));
			REQUIRE_FALSE(target.responds(identifier("do a test"), &my_event_target::on_simple_event));
		
			REQUIRE(event->handled() == false);
			
			dispatcher.invoke(identifier("do a test"), event);

			REQUIRE(event->handled() == true);
		}
		
		SECTION("invoke with created custom target 2") {			
			auto ev = make_ref<simple_event>();
			
			REQUIRE(ev->handled() == false);
			
			dispatcher.invoke(identifier("do another test"), ev);
			
			REQUIRE(ev->handled() == true);
		}
	}
}
