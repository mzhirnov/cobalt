#include "catch.hpp"
#include <cobalt/utility/factory.hpp>
#include <cobalt/utility/identifier.hpp>

using namespace cobalt;

//
class component {
public:
	virtual ~component() = default;
	virtual const std::string& name() const noexcept = 0;
};

using component_factory = auto_factory<component(const char*)>;

//
class my_component : public component {
	REGISTER_AUTO_FACTORY(component_factory, my_component)
public:
	explicit my_component(const char* name) : _name(name) {}
	
	virtual const std::string& name() const noexcept override { return _name.get(); }
	
private:
	identifier _name;
};

//
class my_component2 : public component {
	REGISTER_AUTO_FACTORY(component_factory, my_component2)
public:
	explicit my_component2(const char* name) : _name(name) {}
	
	virtual const std::string& name() const noexcept override { return _name.get(); }
	
private:
	identifier _name;
};

TEST_CASE("factory") {
	REQUIRE(component_factory::can_create("my_component"));
	
	std::unique_ptr<component> c(component_factory::create("my_component", "comp1"));
	REQUIRE(c);
	
	REQUIRE(c->name() == "comp1");
	
	std::unique_ptr<component> c2(component_factory::create("my_component2", "comp2"));
	REQUIRE(c2);
	
	REQUIRE(c2->name() == "comp2");
	
	std::unique_ptr<component> c3(component_factory::create("my_component3", "comp3"));
	REQUIRE_FALSE(c3);
}
