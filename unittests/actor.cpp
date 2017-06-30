#include "catch.hpp"
#include <cobalt/actor.hpp>
#include <cobalt/utility/factory.hpp>

using namespace cobalt;

using component_factory = auto_factory<actor_component()>;

class renderer : public basic_actor_component<"renderer"_hash> {
public:
	renderer(int& instances) : _instances(instances)
		{ ++_instances; }
	~renderer()
		{ --_instances; }
	
	virtual void draw() const { }
	
private:
	int& _instances;
};

class transform : public basic_actor_component<"transform"_hash> {
	CO_REGISTER_AUTO_FACTORY(component_factory, transform)
public:
};

class my_component : public actor_component {
	CO_REGISTER_AUTO_FACTORY(component_factory, my_component)
public:
	virtual uint32_t type() const noexcept override { return "my_component"_hash; }
};

TEST_CASE("information") {
	printf("sizeof(void*) := %zu\n", sizeof(void*));
	printf("sizeof(actor) := %zu (%zu x void*)\n", sizeof(actor), sizeof(actor) / sizeof(void*));
	printf("sizeof(actor_component) := %zu (%zu x void*)\n", sizeof(actor_component), sizeof(actor_component) / sizeof(void*));
}

TEST_CASE("actor") {
	auto o = make_ref<actor>(identifier("root"));
	
	SECTION("add child") {
		auto child = o->add_child(new actor());
	
		REQUIRE(child->parent() == o.get());
		
		SECTION("remove child") {
			auto removed = o->remove_child(child);
			
			REQUIRE(removed.get() == child);
		}
		
		SECTION("detach child") {
			child->detach();
			
			REQUIRE(o->remove_child(child) == nullptr);
		}
	}
	
	SECTION("add component") {
		int renderer_instances = 0;
		auto r = new renderer(renderer_instances);
		
		auto c = o->add_component(r);
		
		auto xform = component_factory::create(identifier("transform"));
		
		REQUIRE(xform != nullptr);
		
		o->add_component(xform);
		
		SECTION("check preconditions") {
			REQUIRE(c == r);
			REQUIRE(c->actor() == o.get());
			REQUIRE(renderer_instances == 1);
		}
		
		SECTION("find component by type") {
			auto f1 = (const renderer*)find_component(o.get(), renderer::component_type);
			
			REQUIRE(f1 == r);
			REQUIRE_FALSE(find_component(o.get(), transform::component_type) == nullptr);
			
			f1->draw(); // will compile
		}
		
		SECTION("find component by hash") {
			auto f1 = (const renderer*)find_component(o.get(), "renderer"_hash);
			
			REQUIRE(f1 == r);
			REQUIRE_FALSE(find_component(o.get(), "transform"_hash) == nullptr);
			REQUIRE(find_component(o.get(), "my_component"_hash) == nullptr);
			
			f1->draw(); // will compile
		}
		
		SECTION("find component by name") {
			auto f1 = (const renderer*)find_component(o.get(), "renderer");
			
			REQUIRE(f1 == r);
			REQUIRE_FALSE(find_component(o.get(), "transform") == nullptr);
			REQUIRE(find_component(o.get(), "my_component") == nullptr);
			
			f1->draw(); // will compile
		}
		
		SECTION("find component with template") {
			auto f2 = find_component<renderer>(o.get());
			
			REQUIRE(f2 == r);
			REQUIRE_FALSE(find_component<transform>(o.get()) == nullptr);
			//o->find_component<my_component>(); // won't compile due to enable_if<>
			
			f2->draw(); // will compile
		}
		
		SECTION("find components by type") {
			std::vector<const actor_component*> vec;
			find_components(o.get(), renderer::component_type, std::back_inserter(vec));
			
			REQUIRE(vec.size() == 1);
			REQUIRE(vec[0] == r);
		}
		
		SECTION("find components with template") {
			std::vector<const renderer*> vec;
			find_components<renderer>(o.get(), std::back_inserter(vec));
			
			REQUIRE(vec.size() == 1);
			REQUIRE(vec[0] == r);
		}
		
		SECTION("remove component") {
			r->detach();
			
			REQUIRE(renderer_instances == 0);
		}
	}
	
	SECTION("find with path") {
		auto o1 = o->add_child(new actor(identifier("child1")));
		auto o2 = o1->add_child(new actor(identifier("child2")));
		auto o3 = o2->add_child(new actor(identifier("child3")));
		
		auto c2 = find_object_with_path(o.get(), "child1/child2");
		REQUIRE(c2);
		
		auto found = find_object_with_path(c2, "child1/child2/child3");
		REQUIRE_FALSE(found);
		
		found = find_object_with_path(c2, "/child1/child2/child3");
		REQUIRE(found);
	}
}
