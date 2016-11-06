#include "catch.hpp"
#include <cobalt/object.hpp>

using namespace cobalt;

class renderer : public basic_component<"renderer"_hash> {
public:
	renderer(bool& destroyed) : _destroyed(destroyed) {}
	virtual ~renderer() { _destroyed = true; }
	
	virtual void draw() const { }
	
private:
	bool& _destroyed;
};

class transform : public basic_component<"transform"_hash> {
public:
};

class my_component : public component {
public:
	virtual hash_type type() const noexcept override { return "my_component"_hash; }
};

TEST_CASE("information") {
	printf("sizeof(std::string) := %zu\n", sizeof(std::string));
	printf("sizeof(std::vector<object>) := %zu\n", sizeof(std::vector<object>));
	printf("sizeof(std::deque<object>) := %zu\n", sizeof(std::deque<object>));
	printf("sizeof(object) := %zu\n", sizeof(object));
	printf("sizeof(component) := %zu\n", sizeof(component));
	printf("sizeof(renderer) := %zu\n", sizeof(renderer));
	printf("sizeof(my_component) := %zu\n", sizeof(my_component));
}

TEST_CASE("object") {
	ref_ptr<object> o = new object();
	
	SECTION("add child") {
		auto child = o->add_child(new object());
	
		REQUIRE(child->parent() == o.get());
		
		SECTION("remove child") {
			auto removed = o->remove_child(child);
			
			REQUIRE(removed.get() == child);
		}
		
		SECTION("self remove child") {
			child->remove_from_parent();
			
			REQUIRE(o->remove_child(child) == nullptr);
		}
	}
	
	SECTION("add component") {
		bool renderer_destroyed = false;
		auto r = new renderer(renderer_destroyed);
		
		auto c = o->add_component(r);
		
		o->add_component(new transform());
		
		SECTION("check preconditions") {
			REQUIRE(c == r);
			REQUIRE(c->object() == o.get());
			REQUIRE(renderer_destroyed == false);
		}
		
		SECTION("find component by type") {
			auto f1 = (renderer*)o->find_component(renderer::component_type);
			
			REQUIRE(f1 == r);
			REQUIRE_FALSE(o->find_component(transform::component_type) == nullptr);
			
			f1->draw(); // will compile
		}
		
		SECTION("find component by name") {
			auto f1 = (renderer*)o->find_component("renderer"_hash);
			
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
			std::vector<component*> vec;
			o->find_components(renderer::component_type, std::back_inserter(vec));
			
			REQUIRE(vec.size() == 1);
			REQUIRE(vec[0] == r);
		}
		
		SECTION("find components with template") {
			std::vector<renderer*> vec;
			o->find_components<renderer>(std::back_inserter(vec));
			
			REQUIRE(vec.size() == 1);
			REQUIRE(vec[0] == r);
		}
		
		SECTION("remove component") {
			r->remove_from_parent();
			
			REQUIRE(renderer_destroyed == true);
		}
	}
}