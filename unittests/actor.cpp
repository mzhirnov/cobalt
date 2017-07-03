#include "catch.hpp"
#include <cobalt/actor.hpp>
#include <cobalt/utility/factory.hpp>

using namespace cobalt;

using component_factory = auto_factory<actor_component()>;

class renderer_component : public actor_component {
	IMPLEMENT_OBJECT_TYPE(renderer_component)
public:
	explicit renderer_component(int& instances) : _instances(instances) { ++_instances; }
	~renderer_component() { --_instances; }
	
	virtual void draw() const { }
	
private:
	int& _instances;
};

class audio_component : public actor_component {
	IMPLEMENT_OBJECT_TYPE(audio_component)
	CO_REGISTER_AUTO_FACTORY_WITH_NAME(component_factory, audio_component, "audio")
public:
};

TEST_CASE("information") {
	printf("sizeof(void*) := %zu\n", sizeof(void*));
	printf("sizeof(actor) := %zu (%zu x void*)\n", sizeof(actor), sizeof(actor) / sizeof(void*));
	printf("sizeof(actor_component) := %zu (%zu x void*)\n", sizeof(actor_component), sizeof(actor_component) / sizeof(void*));
}

TEST_CASE("actor") {
	auto actor1 = make_ref<actor>();
	actor1->transform(new transform_component());

	SECTION("attach component") {
		int renderer_instances = 0;
		auto renderer = new renderer_component(renderer_instances);
		
		REQUIRE(renderer_instances == 1);
		REQUIRE(renderer != nullptr);
		REQUIRE(renderer->actor() == nullptr);
		
		actor1->add_component(renderer);
		
		REQUIRE(renderer->actor() == actor1.get());
		
		auto audio = component_factory::create(identifier("audio"));
		
		REQUIRE(audio != nullptr);
		REQUIRE(audio->actor() == nullptr);
		
		actor1->add_component(audio);
		
		REQUIRE(audio->actor() == actor1.get());
		
		//renderer->detach();
		actor1.reset();
		
		REQUIRE(renderer_instances == 0);
	}
}
