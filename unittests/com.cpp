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

class my_object
	: public com::object_base
	, public com::coclass<my_object>
	, public updatable_impl
	, public drawable_impl
	, public lifetime_impl
{
public:
	BEGIN_CAST_MAP(my_object)
		CAST_ENTRY(updatable)
		CAST_ENTRY(drawable)
		CAST_ENTRY(lifetime)
	END_CAST_MAP()
};

class my_module : public com::module<my_module> {
public:
	BEGIN_OBJECT_MAP(my_module)
		OBJECT_ENTRY(my_object)
	END_OBJECT_MAP()
};

//	if (boost::intrusive_ptr<drawable> d = my_object::create_instance<drawable>()) {
//		d->draw();
//		
//		if (auto u = d->cast<updatable>())
//			u->update(0);
//	}

TEST_CASE("module") {
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
		
		obj = lft->get_object();
		REQUIRE_FALSE(obj.expired());
		
		lft.reset();
		REQUIRE(obj.expired());
	}
}

TEST_CASE("stack_object") {
	std::weak_ptr<void> obj;
	
	{
		com::stack_object<my_object> object;
		
		auto lft = object.cast<lifetime>();
		REQUIRE(lft);
		
		obj = lft->get_object();
		REQUIRE_FALSE(obj.expired());
	}
	
	REQUIRE(obj.expired());
}

TEST_CASE("object") {
	std::weak_ptr<void> obj;
	
	{
		auto object = com::object<my_object>::create_instance();
		
		auto upd = object->cast<updatable>();
		REQUIRE(upd);
		
		auto drw = object->cast<drawable>();
		REQUIRE(drw);
		
		auto lft = object->cast<lifetime>();
		REQUIRE(lft);
		
		obj = lft->get_object();
		REQUIRE_FALSE(obj.expired());
	}
	
	REQUIRE(obj.expired());
}
