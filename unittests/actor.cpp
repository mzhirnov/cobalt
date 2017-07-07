#include "catch.hpp"
#include <cobalt/actor.hpp>
#include <cobalt/utility/factory.hpp>

using namespace cobalt;

using component_factory = auto_factory<actor_component()>;
using component_factory2 = auto_factory<actor_component(int)>;

class sample_component : public actor_component {
	IMPLEMENT_OBJECT_TYPE(sample_component)
public:
	explicit sample_component(int& instances) : _instances(instances) { ++_instances; }
	~sample_component() { --_instances; }
	
private:
	int& _instances;
};

class audio_component : public actor_component {
	IMPLEMENT_OBJECT_TYPE(audio_component)
	REGISTER_AUTO_FACTORY_WITH_NAME(component_factory, audio_component, "audio")
	REGISTER_AUTO_FACTORY_WITH_NAME(component_factory2, audio_component, "audio")
public:
	audio_component() = default;
	audio_component(int) {}
	
	void play() {}
	void stop() {}
};

class mesh_component : public transform_component {
	IMPLEMENT_OBJECT_TYPE(mesh_component)
	REGISTER_AUTO_FACTORY_WITH_NAME(component_factory, mesh_component, "mesh")
public:
};

class bone_component : public transform_component {
	IMPLEMENT_OBJECT_TYPE(bone_component)
	REGISTER_AUTO_FACTORY_WITH_NAME(component_factory, bone_component, "bone")

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
	
	body->add_child(new mesh_component());
	
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
	
		int sample_instances = 0;
		sample_component* sample = new sample_component(sample_instances);
		
		REQUIRE(sample_instances == 1);
		REQUIRE(sample->actor() == nullptr);
		
		actor1->add_component(sample);
		
		REQUIRE(sample->actor() == actor1.get());
		
		actor_component* audio = component_factory2::create("audio", 1);
		
		REQUIRE(audio != nullptr);
		REQUIRE(audio->actor() == nullptr);
		
		actor1->add_component(audio);
		
		REQUIRE(audio->actor() == actor1.get());
		
		//sample->detach();
		actor1.reset();
		
		REQUIRE(sample_instances == 0);
	}
	
	SECTION("find component") {
		auto actor1 = create_actor();
		
		auto audio1 = actor1->find_component_by_type(audio_component::static_type());
		REQUIRE(audio1);
		REQUIRE(audio1->actor() == actor1);
		
		auto mesh1 = actor1->find_component_by_type(mesh_component::static_type());
		REQUIRE(mesh1);
		REQUIRE(mesh1->actor() == actor1);
		
		std::vector<actor_component*> components;
		auto num = actor1->find_components_by_type(bone_component::static_type(), components);
		REQUIRE(num == 8);
		REQUIRE(num == components.size());
	}
}
