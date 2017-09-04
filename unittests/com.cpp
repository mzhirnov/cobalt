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

class lifetime_impl : public lifetime {
public:
	virtual std::weak_ptr<void> get_object() override { return _object; }

private:
	std::shared_ptr<void> _object = std::make_shared<int>(1);
};

class my_object_base
	: public com::object_base
	, public updatable_impl
	, public lifetime_impl
{
public:
	BEGIN_CAST_MAP(my_object_base)
		CAST_ENTRY(updatable)
		CAST_ENTRY(lifetime)
	END_CAST_MAP()
};

class my_object
	: public my_object_base
	, public com::coclass<my_object>
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
	, public drawable_impl
{
public:
	BEGIN_CAST_MAP(my_object2)
		CAST_ENTRY(drawable)
		CAST_ENTRY_CHAIN(my_object_base)
	END_CAST_MAP()
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
		ref_ptr<com::unknown> unk = m.get_class_object(IIDOF(my_object));
		REQUIRE(unk);
		
		auto cf = unk->cast<com::class_factory>();
		REQUIRE(cf);
		
		auto lft = cf->create_instance<lifetime>();
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
		
		REQUIRE_FALSE(com::same_objects(lft.get(), lft2.get()));
		
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
		
		auto upd = object.get_unknown()->cast<updatable>();
		REQUIRE(upd);
		
		auto lft = object.get_unknown()->cast<lifetime>();
		REQUIRE(lft);
		
		REQUIRE(com::same_objects(upd.get(), lft.get()));
		
		obj = lft->get_object();
		REQUIRE_FALSE(obj.expired());
	}
	
	REQUIRE(obj.expired());
}

TEST_CASE("object", "[com]") {
	std::weak_ptr<void> obj;
	
	{
		auto object = com::object<my_object>::create_instance();

		auto upd = object->get_unknown()->cast<updatable>();
		REQUIRE(upd);
		
		auto lft = object->get_unknown()->cast<lifetime>();
		REQUIRE(lft);
		
		REQUIRE(com::same_objects(upd.get(), lft.get()));
		
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
		
		auto lft = upd->cast<lifetime>();
		REQUIRE(lft);
		
		REQUIRE(com::same_objects(upd.get(), lft.get()));
		
		obj = lft->get_object();
		REQUIRE_FALSE(obj.expired());
	}
	
	REQUIRE(obj.expired());
}
