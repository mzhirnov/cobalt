#include "catch.hpp"
#include <cobalt/events.hpp>

using namespace cobalt;

CO_DEFINE_EVENT_CLASS(simple_event) {};

CO_DEFINE_EVENT_CLASS(test_event) {
public:
	explicit test_event(const char* name)
		: _name(name)
	{
	}
	
	const char* name() const { return _name.c_str(); }
	
	bool handled() const { return _handled; }
	void handle() { _handled = true; }
	
private:
	std::string _name;
	bool _handled = false;
};

struct my_subscriber : event_subscriber<my_subscriber> {
	typedef my_subscriber self;
	
	my_subscriber(event_dispatcher& dispatcher)
		: event_subscriber(dispatcher)
	{
		subscribe(&self::on_test_event);
	}
	
	void on_test_event(test_event* event) {
		event->handle();
	}
};

struct my_event_target
	: event_target<my_event_target, test_event>
	, event_target<my_event_target, simple_event>
{
	my_event_target(event_dispatcher& dispatcher)
		: event_target<my_event_target, test_event>(dispatcher, "do a test")
		, event_target<my_event_target, simple_event>(dispatcher, "do another test")
	{ }
	
	void on_event(test_event* event) {
		event->handle();
	}
	
	void on_event(simple_event* event) {
		puts(__PRETTY_FUNCTION__);
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

		REQUIRE(dispatcher.subscribed(&my_subscriber::on_test_event, &subscriber) == true);
		REQUIRE(subscriber.subscribed(&my_subscriber::on_test_event) == true);
		
		dispatcher.post(event);

		REQUIRE(event->handled() == false);
		
		dispatcher.dispatch();

		REQUIRE(event->handled() == true);
	}
	
	SECTION("post event with custom subscriber") {
		auto handler = [](class event* ev) {
			test_event* event = dynamic_cast<test_event*>(ev);
			REQUIRE_FALSE(event == nullptr);
			
			event->handle();
		};
		
		SECTION("post w/o subscription") {
			REQUIRE(dispatcher.subscribed(test_event::type, handler) == false);
			
			dispatcher.post(event);

			REQUIRE(event->handled() == false);
			
			dispatcher.dispatch();

			REQUIRE(event->handled() == false);
		}
		
		SECTION("post with subscription") {
			dispatcher.subscribe(test_event::type, handler);
			
			REQUIRE(dispatcher.subscribed(test_event::type, handler) == true);
			
			dispatcher.post(event);

			REQUIRE(event->handled() == false);
			
			dispatcher.dispatch();

			REQUIRE(event->handled() == true);
		}
	}
	
	SECTION("invoke event w/o subscriber") {
		REQUIRE(event->handled() == false);
		
		dispatcher.invoke(event);

		REQUIRE(event->handled() == false);
	}
	
	SECTION("invoke event with subscriber") {
		my_subscriber subscriber(dispatcher);
		
		REQUIRE(event->handled() == false);
		
		dispatcher.invoke(event);

		REQUIRE(event->handled() == true);
	}
	
	SECTION("invoke event target") {
		my_event_target target(dispatcher);
		
		SECTION("invoke with created custom target") {
			REQUIRE(event->handled() == false);
			
			dispatcher.invoke(event::create_custom_target("do a test"), event);

			REQUIRE(event->handled() == true);
		}
		
		SECTION("invoke with created custom target 2") {
			dispatcher.invoke(target.event_target<my_event_target, simple_event>::target(), new simple_event());
			dispatcher.invoke(event::create_custom_target("do another test"), new simple_event());
		}
	}
}
