#include "catch.hpp"
#include <cobalt/object.hpp>

using namespace cobalt;

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
public:
};

CO_DEFINE_COMPONENT_FACTORY(transform)

class my_component : public component {
public:
	virtual hash_type type() const noexcept override { return "my_component"_hash; }
};

CO_DEFINE_COMPONENT_FACTORY(my_component)

TEST_CASE("information") {
	printf("sizeof(void*) := %zu\n", sizeof(void*));
	printf("sizeof(std::string) := %zu\n", sizeof(std::string));
	printf("sizeof(std::vector<object>) := %zu\n", sizeof(std::vector<object>));
	printf("sizeof(std::deque<object>) := %zu\n", sizeof(std::deque<object>));
	printf("sizeof(std::forward_list<object>) := %zu\n", sizeof(std::forward_list<object>));
	printf("sizeof(intrusive_list<object>) := %zu\n", sizeof(intrusive_list<object>));
	printf("sizeof(intrusive_slist<object>) := %zu\n", sizeof(intrusive_slist<object>));
	printf("sizeof(object) := %zu\n", sizeof(object));
	printf("sizeof(component) := %zu\n", sizeof(component));
	printf("sizeof(renderer) := %zu\n", sizeof(renderer));
	printf("sizeof(my_component) := %zu\n", sizeof(my_component));
}

TEST_CASE("object") {
	ref_ptr<object> o = new object();
	
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
		
		auto xform = component_factory::create("transform"_hash);
		
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
		
		SECTION("find component by name") {
			auto f1 = (const renderer*)o->find_component("renderer"_hash);
			
			REQUIRE(f1 == r);
			REQUIRE_FALSE(o->find_component("transform"_hash) == nullptr);
			REQUIRE(o->find_component("my_component"_hash) == nullptr);
			
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
}
