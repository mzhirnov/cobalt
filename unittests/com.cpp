#include "catch.hpp"
#include <cobalt/com.hpp>

using namespace cobalt;

class updatable : public com::unknown {
public:
	virtual void update(float dt) = 0;
};

class drawable : public com::unknown {
public:
	virtual void draw() = 0;
};

class updatable_impl : public updatable {
public:
	virtual void update(float dt) override {}
};

class drawable_impl : public drawable {
public:
	virtual void draw() override {}
};

class my_object
	: public com::object_base
	, public com::coclass<my_object>
	, public updatable_impl
	, public drawable_impl
{
public:
	BEGIN_CAST_MAP(my_object)
		CAST_ENTRY(updatable)
		CAST_ENTRY(drawable)
	END_CAST_MAP()
};

class my_module : public com::module<my_module> {
public:
	BEGIN_OBJECT_MAP(my_module)
		OBJECT_ENTRY(my_object)
	END_OBJECT_MAP()
};

//	if (auto o = com::object<my_object>::create_instance()) {
//		o->retain();
//		
//		if (boost::intrusive_ptr<updatable> u = o->cast<updatable>()) {
//			u->update(0);
//		}
//		
//		if (auto d = o->cast<drawable>())
//			d->draw();
//		
//		o->release();
//	}

//	if (boost::intrusive_ptr<drawable> d = my_object::create_instance<drawable>()) {
//		d->draw();
//		
//		if (auto u = d->cast<updatable>())
//			u->update(0);
//	}

TEST_CASE("module") {
	my_module m;
	
	SECTION("get_class_object") {
		boost::intrusive_ptr<com::unknown> cf_unk = m.get_class_object(IIDOF(my_object));
		REQUIRE(cf_unk);
		
		boost::intrusive_ptr<com::class_factory> cf = cf_unk->cast<com::class_factory>();
		REQUIRE(cf);
		
		boost::intrusive_ptr<com::unknown> upd_unk = cf->create_instance(nullptr, IIDOF(updatable));
		REQUIRE(upd_unk);
		
		boost::intrusive_ptr<updatable> upd = upd_unk->cast<updatable>();
		REQUIRE(upd);
	}
	
	//SECTION("create_instance") {
		boost::intrusive_ptr<drawable> drw = m.create_instance<drawable>(IIDOF(my_object));
		REQUIRE(drw);
	//}
}

