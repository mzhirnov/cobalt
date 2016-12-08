#include "nonius.hpp"

#include <cobalt/events.hpp>

#include <boost/signals2.hpp>

using namespace cobalt;

CO_DEFINE_EVENT_CLASS(my_event) {};

class my_subscriber : public event_subscriber<my_subscriber> {
public:
	typedef my_subscriber self;
	
	my_subscriber(event_dispatcher& dispatcher) : event_subscriber(dispatcher)
	{
		subscribe(&self::on_my_event);
	}
	
	void on_my_event(my_event*) {}
};

NONIUS_BENCHMARK("cobalt events", [](nonius::chronometer meter) {
	event_dispatcher dispatcher;
	my_subscriber subscriber(dispatcher);
	
	auto ev = make_ref<my_event>();
	
	meter.measure([&](int i) {
		for (int k = 0; k < 10; ++k)
			dispatcher.invoke(ev);
	});
	
})

NONIUS_BENCHMARK("boost signals2", [](nonius::chronometer meter) {
	typedef boost::signals2::signal<void(my_event*)> my_signal;
	
	my_signal sig;
	
	event_dispatcher dispatcher;
	my_subscriber subscriber(dispatcher);
	
	sig.connect(std::bind(&my_subscriber::on_my_event, &subscriber, std::placeholders::_1));
	
	my_event ev;
	
	meter.measure([&](int i) {
		for (int k = 0; k < 10; ++k)
			sig(&ev);
	});
})
