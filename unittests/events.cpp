#include "catch.hpp"
#include <cobalt/events.hpp>

using namespace cobalt;

CO_DEFINE_EVENT(simple_event)

CO_DEFINE_EVENT_CLASS(test_event) {
public:
	explicit test_event(const char* name)
		: _name(name)
	{
	}
	
	const char* name() const { return _name.c_str(); }
	
private:
	std::string _name;
	bool _handled = false;
};

struct my_subscriber : event_handler<my_subscriber> {
	typedef my_subscriber self;
	
	explicit my_subscriber(event_dispatcher& dispatcher)
		: event_handler(dispatcher)
	{
		subscribe(&self::on_test_event);
	}
	
	void on_test_event(test_event* event) {
		event->handled(true);
	}
};

struct my_subscriber2 : event_handler<my_subscriber2> {
	explicit my_subscriber2(event_dispatcher& dispatcher)
		: event_handler(dispatcher)
	{
		subscribe<test_event>();
	}
	
	void on_event(test_event* event) {
		event->handled(true);
	}
};

struct my_event_target : event_handler<my_event_target>
{
	typedef my_event_target self;
	
	explicit my_event_target(event_dispatcher& dispatcher)
		: event_handler(dispatcher)
	{
		respond<test_event>(event::create_custom_target("do a test"));
		respond<simple_event>(event::create_custom_target("do another test"));
	}
	
	void on_target_event(test_event* event) {
		event->handled(true);
	}
	
	void on_target_event(simple_event* event) {
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
		
		REQUIRE(dispatcher.connected(&subscriber, test_event::type));
		REQUIRE(subscriber.connected(test_event::type));
		
		dispatcher.post(event);

		REQUIRE(event->handled() == false);
		
		dispatcher.dispatch();

		REQUIRE(event->handled() == true);
	}
	
	SECTION("post event with subscriber 2") {
		my_subscriber2 subscriber(dispatcher);

		REQUIRE((dispatcher.subscribed<my_subscriber2, test_event>(&my_subscriber2::on_event, &subscriber)));
		REQUIRE(subscriber.subscribed<test_event>());
		
		REQUIRE(dispatcher.connected(&subscriber, test_event::type));
		REQUIRE(subscriber.connected(test_event::type));
		
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
			REQUIRE(target.responds<test_event>(event::create_custom_target("do a test")));
			REQUIRE_FALSE(target.responds<test_event>(event::create_custom_target("do another test")));
			REQUIRE_FALSE(target.responds<simple_event>(event::create_custom_target("do a test")));
		
			REQUIRE(event->handled() == false);
			
			dispatcher.invoke(event::create_custom_target("do a test"), event);

			REQUIRE(event->handled() == true);
		}
		
		SECTION("invoke with created custom target 2") {			
			auto ev = make_ref<simple_event>();
			
			REQUIRE(ev->handled() == false);
			
			dispatcher.invoke(event::create_custom_target("do another test"), ev);
			
			REQUIRE(ev->handled() == true);
		}
	}
}
