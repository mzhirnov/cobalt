#include "catch.hpp"
#include <cobalt/object.hpp>
#include <cobalt/utility/factory.hpp>

using namespace cobalt;

using component_factory = factory<component()>;

class renderer : public basic_component<"renderer"_hash> {
public:
	renderer(int& instances) : _instances(instances)
		{ ++_instances; }
	~renderer()
		{ --_instances; }
	
	virtual void draw() const { }
	
private:
	int& _instances;
};

class transform : public basic_component<"transform"_hash> {
CO_REGISTER_FACTORY(component_factory, transform)
public:
};

class my_component : public component {
CO_REGISTER_FACTORY(component_factory, my_component)
public:
	virtual hash_type type() const noexcept override { return "my_component"_hash; }
};

TEST_CASE("information") {
	printf("sizeof(void*) := %zu\n", sizeof(void*));
	printf("sizeof(std::forward_list<object>) := %zu\n", sizeof(std::forward_list<object>));
	printf("sizeof(intrusive_list<object>) := %zu\n", sizeof(intrusive_list<object>));
	printf("sizeof(intrusive_slist<object>) := %zu\n", sizeof(intrusive_slist<object>));
	printf("sizeof(object) := %zu\n", sizeof(object));
	printf("sizeof(component) := %zu\n", sizeof(component));
}

TEST_CASE("object") {
	auto o = make_counted<object>();
	
	SECTION("add child") {
		auto child = o->attach(new object());
	
		REQUIRE(child->parent() == o.get());
		
		SECTION("remove child") {
			auto removed = o->detach(child);
			
			REQUIRE(removed.get() == child);
		}
		
		SECTION("self remove child") {
			child->detach();
			
			REQUIRE(o->detach(child) == nullptr);
		}
	}
	
	SECTION("add component") {
		int renderer_instances = 0;
		auto r = new renderer(renderer_instances);
		
		auto c = o->attach(r);
		
		auto xform = component_factory::create(identifier("transform"));
		
		REQUIRE(xform != nullptr);
		
		o->attach(xform);
		
		SECTION("check preconditions") {
			REQUIRE(c == r);
			REQUIRE(c->object() == o.get());
			REQUIRE(renderer_instances == 1);
		}
		
		SECTION("find component by type") {
			auto f1 = (const renderer*)o->find_component(renderer::component_type);
			
			REQUIRE(f1 == r);
			REQUIRE_FALSE(o->find_component(transform::component_type) == nullptr);
			
			f1->draw(); // will compile
		}
		
		SECTION("find component by hash") {
			auto f1 = (const renderer*)o->find_component("renderer"_hash);
			
			REQUIRE(f1 == r);
			REQUIRE_FALSE(o->find_component("transform"_hash) == nullptr);
			REQUIRE(o->find_component("my_component"_hash) == nullptr);
			
			f1->draw(); // will compile
		}
		
		SECTION("find component by name") {
			auto f1 = (const renderer*)o->find_component("renderer");
			
			REQUIRE(f1 == r);
			REQUIRE_FALSE(o->find_component("transform") == nullptr);
			REQUIRE(o->find_component("my_component") == nullptr);
			
			f1->draw(); // will compile
		}
		
		SECTION("find component with template") {
			auto f2 = o->find_component<renderer>();
			
			REQUIRE(f2 == r);
			REQUIRE_FALSE(o->find_component<transform>() == nullptr);
			//o->find_component<my_component>(); // won't compile due to enable_if<>
			
			f2->draw(); // will compile
		}
		
		SECTION("find components by type") {
			std::vector<const component*> vec;
			o->find_components(renderer::component_type, std::back_inserter(vec));
			
			REQUIRE(vec.size() == 1);
			REQUIRE(vec[0] == r);
		}
		
		SECTION("find components with template") {
			std::vector<const renderer*> vec;
			o->find_components<renderer>(std::back_inserter(vec));
			
			REQUIRE(vec.size() == 1);
			REQUIRE(vec[0] == r);
		}
		
		SECTION("remove component") {
			r->detach();
			
			REQUIRE(renderer_instances == 0);
		}
	}
	
	SECTION("find with path") {
		auto o1 = o->attach(new object(identifier("child1")));
		auto o2 = o1->attach(new object(identifier("child2")));
		auto o3 = o2->attach(new object(identifier("child3")));
		
		auto found = o->find_object_with_path("child1/child2/child3");
		REQUIRE(found);
	}
}
