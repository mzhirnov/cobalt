#include "catch2/catch.hpp"
#include <cobalt/com.hpp>
#include <cobalt/utility/intrusive.hpp>

#include <memory>

using namespace cobalt;

namespace test {

// Interfaces

DECLARE_INTERFACE(test, updatable)
struct updatable : com::any {
	virtual void update(float dt) noexcept = 0;
};

DECLARE_INTERFACE(test, drawable)
struct drawable : com::any {
	virtual void draw() noexcept = 0;
};

DECLARE_INTERFACE(test, lifetime)
struct lifetime : com::any {
	virtual std::weak_ptr<void> guard() noexcept = 0;
};

// Implementation

class updatable_impl : public updatable {
public:
	virtual void update(float dt) noexcept override { puts(__PRETTY_FUNCTION__); }
};

class drawable_impl : public drawable {
public:
	virtual void draw() noexcept override { puts(__PRETTY_FUNCTION__); }
};

class drawable_tear_off_impl
	: public com::tear_off_object_base<drawable_tear_off_impl>
	, public drawable
{
public:
	drawable_tear_off_impl() { /*puts(__PRETTY_FUNCTION__);*/ }
	~drawable_tear_off_impl() { /*puts(__PRETTY_FUNCTION__);*/ }

	BEGIN_CAST_MAP(drawable_tear_off_impl)
		CAST_ENTRY(drawable)
	END_CAST_MAP()
	
	virtual void draw() noexcept override { puts(__PRETTY_FUNCTION__); }
};

class lifetime_impl : public lifetime {
public:
	virtual std::weak_ptr<void> guard() noexcept override { return _object; }

private:
	std::shared_ptr<void> _object = std::make_shared<int>(1);
};

// Classes

class my_object_base
	: public com::object_base
	, public lifetime_impl
{
public:
	BEGIN_CAST_MAP(my_object_base)
		CAST_ENTRY(lifetime)
	END_CAST_MAP()
};

DECLARE_CLASS(test, my_object)
class my_object
	: public my_object_base
	, public updatable_impl
	, public com::coclass<my_object>
{
public:
	BEGIN_CAST_MAP(my_object)
		CAST_ENTRY(updatable)
		CAST_ENTRY_CHAIN(my_object_base)
	END_CAST_MAP()
	
	void hello_world() noexcept {}
};

} // namespace test

TEST_CASE("uid", "[com]") {
	REQUIRE(UIDOF(test::updatable) == UIDSTR("test::updatable"));
	REQUIRE(UIDOF(com::any) == UIDSTR("com::any"));
	REQUIRE(UIDOF(test::updatable).name() == std::string_view("test::updatable"));
	REQUIRE(UIDOF(com::any).name() == std::string_view("com::any"));
	REQUIRE_FALSE(UIDOF(test::updatable) == UIDOF(com::any));
}

TEST_CASE("stack_object/chain_cast", "[com]") {
	std::weak_ptr<void> guard;
	
	{
		com::stack_object<test::my_object> object;
		REQUIRE(object.init_result());
		
		object.hello_world();
		
		auto upd = com::cast<test::updatable>(object.identity());
		REQUIRE(upd);
		
		auto lft = com::cast<test::lifetime>(upd);
		REQUIRE(lft);
		
		REQUIRE(com::identical(upd, lft));
		
		guard = lft->guard();
		REQUIRE_FALSE(guard.expired());
	}
	
	REQUIRE(guard.expired());
}

TEST_CASE("object/chain_cast", "[com]") {
	std::weak_ptr<void> guard;
	
	{
		auto object = com::object<test::my_object>::create_instance();
		
		object->hello_world();

		auto upd = com::cast<test::updatable>(object->identity());
		REQUIRE(upd);
		
		auto lft = com::cast<test::lifetime>(upd);
		REQUIRE(lft);
		
		REQUIRE(com::identical(upd, lft));
		
		guard = lft->guard();
		REQUIRE_FALSE(guard.expired());
	}
	
	REQUIRE(guard.expired());
}

TEST_CASE("coclass/chain_cast", "[com]") {
	std::weak_ptr<void> guard;
	
	{
		auto upd = test::my_object::s_create_instance<test::updatable>();
		REQUIRE(upd);
		
		auto lft = com::cast<test::lifetime>(upd);
		REQUIRE(lft);
		
		REQUIRE(com::identical(upd, lft));
		
		guard = lft->guard();
		REQUIRE_FALSE(guard.expired());
	}
	
	REQUIRE(guard.expired());
}

namespace test {

DECLARE_CLASS(test, my_object2)
class my_object2
	: public com::object_base
	, public lifetime_impl
	, public updatable_impl
	, public com::coclass<my_object2>
{
public:
	BEGIN_CAST_MAP(my_object2)
		CAST_ENTRY(lifetime)
		CAST_ENTRY_TEAR_OFF(UIDOF(drawable), drawable_tear_off_impl)
		CAST_ENTRY(updatable)
	END_CAST_MAP()
};

} // namespace test

TEST_CASE("coclass/tear_off", "[com]") {
	std::weak_ptr<void> guard;
	
	{
		auto drw = test::my_object2::s_create_instance<test::drawable>();
		REQUIRE(drw);
		
		auto lft = com::cast<test::lifetime>(drw);
		REQUIRE(lft);
		
		auto upd = com::cast<test::updatable>(lft);
		REQUIRE(upd);
		
		REQUIRE(com::identical(upd, lft));
		REQUIRE(com::identical(lft, drw));
		REQUIRE(com::identical(drw, upd));
		
		guard = lft->guard();
		REQUIRE_FALSE(guard.expired());
		
		upd.reset();
		lft.reset();
		
		REQUIRE_FALSE(guard.expired());
		
		drw.reset();
		REQUIRE(guard.expired());
	}
}

namespace test {

DECLARE_CLASS(test, my_object3)
class my_object3
	: public com::object_base
	, public lifetime_impl
	, public updatable_impl
	, public com::coclass<my_object3>
{
public:
	BEGIN_CAST_MAP(my_object3)
		CAST_ENTRY(updatable)
		CAST_ENTRY(lifetime)
		CAST_ENTRY_CACHED_TEAR_OFF(UIDOF(drawable), drawable_tear_off_impl, _drawable)
	END_CAST_MAP()
	
private:
	ref_ptr<drawable> _drawable;
};

} // namespace test

TEST_CASE("coclass/cached_tear_off", "[com]") {
	std::weak_ptr<void> guard;
	
	{
		auto upd = test::my_object3::s_create_instance<test::updatable>();
		REQUIRE(upd);
		
		auto lft = com::cast<test::lifetime>(upd);
		REQUIRE(lft);
		
		auto drw = com::cast<test::drawable>(lft);
		REQUIRE(drw);
		
		REQUIRE(com::identical(upd, lft));
		REQUIRE(com::identical(lft, drw));
		REQUIRE(com::identical(drw, upd));
		
		guard = lft->guard();
		REQUIRE_FALSE(guard.expired());
		
		upd.reset();
		lft.reset();
		
		REQUIRE_FALSE(guard.expired());
		
		drw.reset();
		REQUIRE(guard.expired());
	}
}

namespace test {

DECLARE_CLASS(test, my_object4)
class my_object4
	: public com::object_base
	, public lifetime_impl
	, public com::coclass<my_object4>
{
public:
	BEGIN_CAST_MAP(my_object4)
		CAST_ENTRY(lifetime)
		CAST_ENTRY_AUTOAGGREGATE_CLASS(UIDOF(drawable), _object3, my_object3)
	END_CAST_MAP()

private:
	ref_ptr<any> _object3;
};

} // namespace test

TEST_CASE("coclass/auto_aggregate_class", "[com]") {
	std::weak_ptr<void> guard;
	
	{
		auto lft = test::my_object4::s_create_instance<test::lifetime>();
		REQUIRE(lft);
		
		auto drw = com::cast<test::drawable>(lft);
		REQUIRE(drw);
		
		auto upd = com::cast<test::updatable>(drw);
		REQUIRE_FALSE(upd);
		
		REQUIRE(com::identical(lft, drw));
		
		guard = lft->guard();
		REQUIRE_FALSE(guard.expired());
		
		lft.reset();
		
		REQUIRE_FALSE(guard.expired());
		
		drw.reset();
		REQUIRE(guard.expired());
	}
}

namespace test {

DECLARE_CLASS(test, my_object5)
class my_object5
	: public com::object_base
	, public lifetime_impl
	, public com::coclass<my_object5>
{
public:
	BEGIN_CAST_MAP(my_object5)
		CAST_ENTRY(lifetime)
		CAST_ENTRY_AUTOAGGREGATE_CLASS_BLIND(_object3, my_object3)
	END_CAST_MAP()

private:
	ref_ptr<any> _object3;
};

} // namespace test

TEST_CASE("coclass/auto_aggregate_class_blind", "[com]") {
	std::weak_ptr<void> guard;
	
	{
		auto lft = test::my_object5::s_create_instance<test::lifetime>();
		REQUIRE(lft);
		
		auto drw = com::cast<test::drawable>(lft);
		REQUIRE(drw);
		
		auto upd = com::cast<test::updatable>(drw);
		REQUIRE(upd);
		
		REQUIRE(com::identical(upd, lft));
		REQUIRE(com::identical(lft, drw));
		REQUIRE(com::identical(drw, upd));
		
		guard = lft->guard();
		REQUIRE_FALSE(guard.expired());
		
		lft.reset();
		upd.reset();
		
		REQUIRE_FALSE(guard.expired());
		
		drw.reset();
		REQUIRE(guard.expired());
	}
}

namespace test {

DECLARE_CLASS(test, my_object6)
class my_object6
	: public com::object_base
	, public lifetime_impl
	, public com::coclass<my_object6>
{
public:
	BEGIN_CAST_MAP(my_object6)
		CAST_ENTRY(lifetime)
		CAST_ENTRY_AGGREGATE(UIDOF(drawable), _object3)
	END_CAST_MAP()
	
	bool init() noexcept {
		_object3 = my_object3::s_create_instance<any>(controlling_object());
		return !!_object3;
	}

private:
	ref_ptr<any> _object3;
};

} // namespace test

TEST_CASE("coclass/aggregate", "[com]") {
	std::weak_ptr<void> guard;
	
	{
		auto lft = test::my_object6::s_create_instance<test::lifetime>();
		REQUIRE(lft);
		
		auto drw = com::cast<test::drawable>(lft);
		REQUIRE(drw);
		
		auto upd = com::cast<test::updatable>(drw);
		REQUIRE_FALSE(upd);
		
		REQUIRE(com::identical(lft, drw));
		
		guard = lft->guard();
		REQUIRE_FALSE(guard.expired());
		
		lft.reset();
		
		REQUIRE_FALSE(guard.expired());
		
		drw.reset();
		REQUIRE(guard.expired());
	}
}

namespace test {

DECLARE_CLASS(test, my_object7)
class my_object7
	: public com::object_base
	, public lifetime_impl
	, public com::coclass<my_object7>
{
public:
	BEGIN_CAST_MAP(my_object7)
		CAST_ENTRY(lifetime)
		CAST_ENTRY_AGGREGATE_BLIND(_object3)
	END_CAST_MAP()
	
	bool init() noexcept {
		_object3 = my_object3::s_create_instance<any>(controlling_object());
		return !!_object3;
	}

private:
	ref_ptr<any> _object3;
};

} // namespace test

TEST_CASE("coclass/aggregate_blind", "[com]") {
	std::weak_ptr<void> guard;
	
	{
		auto lft = test::my_object7::s_create_instance<test::lifetime>();
		REQUIRE(lft);
		
		auto drw = com::cast<test::drawable>(lft);
		REQUIRE(drw);
		
		auto upd = com::cast<test::updatable>(drw);
		REQUIRE(upd);
		
		REQUIRE(com::identical(upd, lft));
		REQUIRE(com::identical(lft, drw));
		REQUIRE(com::identical(drw, upd));
		
		guard = lft->guard();
		REQUIRE_FALSE(guard.expired());
		
		lft.reset();
		upd.reset();
		
		REQUIRE_FALSE(guard.expired());
		
		drw.reset();
		REQUIRE(guard.expired());
	}
}

namespace test {

DECLARE_CLASS(test, my_object8)
class my_object8
	: public com::object_base
	, public lifetime_impl
	, public com::coclass<my_object8>
{
public:
	BEGIN_CAST_MAP(my_object8)
		CAST_ENTRY(lifetime)
		//CAST_ENTRY_AUTOAGGREGATE_BLIND(_object2, UIDOF(my_object2))
	END_CAST_MAP()

private:
	ref_ptr<any> _object2;
};

class my_module : public com::module<my_module> {
public:
	BEGIN_OBJECT_MAP(my_module)
		OBJECT_ENTRY(my_object)
		OBJECT_ENTRY(my_object2)
		OBJECT_ENTRY_NON_CREATEABLE(my_object3)
		OBJECT_ENTRY_NON_CREATEABLE(my_object4)
		OBJECT_ENTRY_NON_CREATEABLE(my_object5)
		OBJECT_ENTRY_NON_CREATEABLE(my_object6)
		OBJECT_ENTRY(my_object7)
		OBJECT_ENTRY_NON_CREATEABLE(my_object8)
	END_OBJECT_MAP()
};

static my_module m;

} // namespace test

TEST_CASE("module", "[com]") {
	std::weak_ptr<void> guard;
	
	SECTION("get_class_object") {
		auto cf = com::get_class_object(UIDOF(test::my_object7));
		REQUIRE(cf);
		
		auto lft = com::cast<test::lifetime>(cf->create_instance(nullptr, UIDOF(test::lifetime)));
		REQUIRE(lft);
		
		guard = lft->guard();
		REQUIRE_FALSE(guard.expired());
		
		lft.reset();
		REQUIRE(guard.expired());
	}
	
	SECTION("create_instance") {
		std::error_code ec;
		
		auto lft = com::create_instance<test::lifetime>(UIDSTR("test::my_object"), ec);
		REQUIRE(!ec);
		REQUIRE(lft);
		
		auto lft2 = com::create_instance<test::lifetime>(UIDSTR("test::my_object2"), ec);
		REQUIRE(!ec);
		REQUIRE(lft2);
		
		auto lft3 = com::create_instance<test::lifetime>(UIDSTR("test::my_object3"), ec);
		REQUIRE(ec);
		REQUIRE_FALSE(lft3);
		
		REQUIRE_FALSE(com::identical(lft, lft2));
		
		guard = lft->guard();
		REQUIRE_FALSE(guard.expired());
		
		lft.reset();
		REQUIRE(guard.expired());
	}
}
