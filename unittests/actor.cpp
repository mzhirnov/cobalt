#include "catch.hpp"
#include <cobalt/actor.hpp>
#include <cobalt/utility/factory.hpp>

#include <boost/container/small_vector.hpp>

using namespace cobalt;

using my_component_factory = auto_factory<actor_component(int), const char*>;

#define REGISTER_FACTORY2(F, C) REGISTER_FACTORY_WITH_KEY(F, C, #C)

class sample_component : public actor_component {
	IMPLEMENT_OBJECT_TYPE(sample_component)
public:
	sample_component() { ++_instances; }
	~sample_component() { --_instances; }
	
public:
	static int _instances;
};

int sample_component::_instances = 0;

class audio_component : public actor_component {
	IMPLEMENT_OBJECT_TYPE(audio_component)
	REGISTER_FACTORY2(my_component_factory, audio_component)	
public:
	audio_component() = default;
	audio_component(int) {}
	
	void play() {}
	void stop() {}
};

class mesh_component : public transform_component {
	IMPLEMENT_OBJECT_TYPE(mesh_component)
public:
};

class bone_component : public transform_component {
	IMPLEMENT_OBJECT_TYPE(bone_component)

public:
	bone_component() = default;	
	explicit bone_component(const char* name_) {
		name(identifier(name_));
	}
};

ref_ptr<actor> create_actor() {
	auto ac = make_ref<actor>();
	
	auto body = new bone_component("body");
	
	auto head = new bone_component("head");
	head->add_child(new bone_component("jaw"));
	head->add_child(new bone_component("left_eye"));
	head->add_child(new bone_component("right_eye"));
	body->add_child(head);
	
	body->add_child(new bone_component("left_fin"));
	body->add_child(new bone_component("right_fin"));
	body->add_child(new bone_component("tail"));
	
	body->add_child(object::create_instance<mesh_component>());
	
	ac->transform(body);
	
	ac->add_component(new audio_component());
	
	return ac;
}

TEST_CASE("information") {
	printf("sizeof(void*) := %zu\n", sizeof(void*));
	printf("sizeof(actor) := %zu (%zu x void*)\n", sizeof(actor), sizeof(actor) / sizeof(void*));
	printf("sizeof(actor_component) := %zu (%zu x void*)\n", sizeof(actor_component), sizeof(actor_component) / sizeof(void*));
}

TEST_CASE("actor") {
	SECTION("attach component") {
		auto actor1 = make_ref<actor>();
		actor1->transform(new transform_component());
	
		sample_component* sample = object::create_instance<sample_component>();
		
		REQUIRE(sample_component::_instances == 1);
		REQUIRE(sample->actor() == nullptr);
		
		actor1->add_component(sample);
		
		REQUIRE(sample->actor() == actor1.get());
		
		actor_component* audio = my_component_factory::create("audio_component", 1);
		
		REQUIRE(audio != nullptr);
		REQUIRE(audio->actor() == nullptr);
		
		actor1->add_component(audio);
		
		REQUIRE(audio->actor() == actor1.get());
		
		//sample->detach();
		actor1.reset();
		
		REQUIRE(sample_component::_instances == 0);
	}
	
	SECTION("find component") {
		auto actor1 = create_actor();
		
		auto audio1 = actor1->find_component<audio_component>();
		REQUIRE(audio1);
		REQUIRE(audio1->actor() == actor1);
		
		auto mesh1 = actor1->find_component<mesh_component>();
		REQUIRE(mesh1);
		REQUIRE(mesh1->actor() == actor1);
		
		boost::container::small_vector<bone_component*, 256> components;
		auto num = actor1->find_components<bone_component>(components);
		REQUIRE(num == 8);
		REQUIRE(num == components.size());
	}
}
