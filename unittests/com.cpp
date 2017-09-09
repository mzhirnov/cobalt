#include "catch.hpp"
#include <cobalt/com.hpp>
#include <cobalt/utility/intrusive.hpp>

#include <memory>

using namespace cobalt;

// Interfaces

class updatable : public com::unknown {
public:
	virtual void update(float dt) = 0;
};

class drawable : public com::unknown {
public:
	virtual void draw() = 0;
};

class lifetime : public com::unknown {
public:
	virtual std::weak_ptr<void> get_object() = 0;
};

// Implementation

class updatable_impl : public updatable {
public:
	virtual void update(float dt) override {}
};

class drawable_impl : public drawable {
public:
	virtual void draw() override {}
};

class drawable_tear_off
	: public com::tear_off_object_base<drawable_tear_off>
	, public drawable
{
public:
	drawable_tear_off() { puts(__PRETTY_FUNCTION__); }
	~drawable_tear_off() { puts(__PRETTY_FUNCTION__); }

	BEGIN_CAST_MAP(drawable_tear_off)
		CAST_ENTRY(drawable)
	END_CAST_MAP()
	
	virtual void draw() override { puts(__PRETTY_FUNCTION__); }
};

class lifetime_impl : public lifetime {
public:
	virtual std::weak_ptr<void> get_object() override { return _object; }

private:
	std::shared_ptr<void> _object = std::make_shared<int>(1);
};

class my_object_base
	: public com::object_base
	, public lifetime_impl
{
public:
	BEGIN_CAST_MAP(my_object_base)
		CAST_ENTRY(lifetime)
	END_CAST_MAP()
};

class my_object
	: public my_object_base
	, public com::coclass<my_object>
	, public updatable_impl
{
public:
	BEGIN_CAST_MAP(my_object)
		CAST_ENTRY(updatable)
		CAST_ENTRY_CHAIN(my_object_base)
	END_CAST_MAP()
};

class my_object2
	: public my_object_base
	, public com::coclass<my_object2>
	, public updatable_impl
{
public:
	BEGIN_CAST_MAP(my_object2)
		CAST_ENTRY(updatable)
		CAST_ENTRY_TEAR_OFF(IIDOF(drawable), drawable_tear_off)
		CAST_ENTRY_CHAIN(my_object_base)
	END_CAST_MAP()
};

class my_object3
	: public my_object_base
	, public com::coclass<my_object3>
	, public updatable_impl
{
public:
	BEGIN_CAST_MAP(my_object3)
		CAST_ENTRY(updatable)
		CAST_ENTRY_CACHED_TEAR_OFF(IIDOF(drawable), drawable_tear_off, _drawable)
		CAST_ENTRY_CHAIN(my_object_base)
	END_CAST_MAP()
	
private:
	ref_ptr<drawable> _drawable;
};

class my_module : public com::module<my_module> {
public:
	BEGIN_OBJECT_MAP(my_module)
		OBJECT_ENTRY(my_object)
		OBJECT_ENTRY(my_object2)
	END_OBJECT_MAP()
};

TEST_CASE("module", "[com]") {
	my_module m;
	
	std::weak_ptr<void> obj;
	
	SECTION("get_class_object") {
		auto unk = m.get_class_object(IIDOF(my_object));
		REQUIRE(unk);
		
		auto cf = com::cast<com::class_factory>(unk);
		REQUIRE(cf);
		
		auto lft = com::cast<lifetime>(cf->create_instance(nullptr, IIDOF(lifetime)));
		REQUIRE(lft);
		
		obj = lft->get_object();
		REQUIRE_FALSE(obj.expired());
		
		lft.reset();
		REQUIRE(obj.expired());
	}
	
	SECTION("create_instance") {
		auto lft = m.create_instance<lifetime>(IIDOF(my_object));
		REQUIRE(lft);
		
		auto lft2 = m.create_instance<lifetime>(IIDOF(my_object2));
		REQUIRE(lft2);
		
		REQUIRE_FALSE(com::same_objects(lft, lft2));
		
		obj = lft->get_object();
		REQUIRE_FALSE(obj.expired());
		
		lft.reset();
		REQUIRE(obj.expired());
	}
}

TEST_CASE("stack_object", "[com]") {
	std::weak_ptr<void> obj;
	
	{
		com::stack_object<my_object> object;
		
		auto upd = com::cast<updatable>(object.get_unknown());
		REQUIRE(upd);
		
		auto lft = com::cast<lifetime>(object.get_unknown());
		REQUIRE(lft);
		
		REQUIRE(com::same_objects(upd, lft));
		
		obj = lft->get_object();
		REQUIRE_FALSE(obj.expired());
	}
	
	REQUIRE(obj.expired());
}

TEST_CASE("object", "[com]") {
	std::weak_ptr<void> obj;
	
	{
		auto object = com::object<my_object>::create_instance();

		auto upd = com::cast<updatable>(object->get_unknown());
		REQUIRE(upd);
		
		auto lft = com::cast<lifetime>(object->get_unknown());
		REQUIRE(lft);
		
		REQUIRE(com::same_objects(upd, lft));
		
		obj = lft->get_object();
		REQUIRE_FALSE(obj.expired());
	}
	
	REQUIRE(obj.expired());
}

TEST_CASE("coclass", "[com]") {
	std::weak_ptr<void> obj;
	
	{
		auto upd = my_object::create_instance<updatable>();
		REQUIRE(upd);
		
		auto lft = com::cast<lifetime>(upd);
		REQUIRE(lft);
		
		REQUIRE(com::same_objects(upd, lft));
		
		obj = lft->get_object();
		REQUIRE_FALSE(obj.expired());
	}
	
	REQUIRE(obj.expired());
}

TEST_CASE("tear_off", "[com]") {
	std::weak_ptr<void> obj;
	
	{
		auto upd = my_object2::create_instance<updatable>();
		REQUIRE(upd);
		
		auto lft = com::cast<lifetime>(upd);
		REQUIRE(lft);
		
		auto drw = com::cast<drawable>(lft);
		REQUIRE(drw);
		
		drw->draw();
		
		REQUIRE(com::same_objects(upd, lft));
		REQUIRE(com::same_objects(lft, drw));
		REQUIRE(com::same_objects(drw, upd));
		
		obj = lft->get_object();
		REQUIRE_FALSE(obj.expired());
		
		upd.reset();
		lft.reset();
		
		REQUIRE_FALSE(obj.expired());
	}
	
	REQUIRE(obj.expired());
}

TEST_CASE("cached_tear_off", "[com]") {
	std::weak_ptr<void> obj;
	
	{
		auto upd = my_object3::create_instance<updatable>();
		REQUIRE(upd);
		
		auto lft = com::cast<lifetime>(upd);
		REQUIRE(lft);
		
		auto drw = com::cast<drawable>(lft);
		REQUIRE(drw);
		
		drw->draw();
		
		REQUIRE(com::same_objects(upd, lft));
		REQUIRE(com::same_objects(lft, drw));
		REQUIRE(com::same_objects(drw, upd));
		
		obj = lft->get_object();
		REQUIRE_FALSE(obj.expired());
		
		upd.reset();
		lft.reset();
		
		REQUIRE_FALSE(obj.expired());
	}
	
	REQUIRE(obj.expired());
}
