#ifndef COBALT_COM_BASE_HPP_INCLUDED
#define COBALT_COM_BASE_HPP_INCLUDED

#pragma once

// Classes in this file:
//     object_base
//     tear_off_object_base
//     stack_object
//     object
//     contained_object
//     agg_object
//     poly_object
//     tear_off_object
//     cached_tear_off_object
//     coclass
//     module

#include <cobalt/com/core.hpp>
#include <cobalt/com/utility.hpp>

#include <boost/intrusive/slist.hpp>
#include <boost/assert.hpp>

namespace cobalt {
namespace com {

#define _PACKSZ 8

#define OFFSETOFCLASS(base, derived) \
	(reinterpret_cast<size_t>(static_cast<base*>(reinterpret_cast<derived*>(_PACKSZ)))-_PACKSZ)
	
#define OFFSETOFCLASS2(base, base2, derived) \
	(reinterpret_cast<size_t>(static_cast<base*>(static_cast<base2*>(reinterpret_cast<derived*>(_PACKSZ))))-_PACKSZ)
	
#define OFFSETOFMEMBER(type, member) (reinterpret_cast<size_t>(&reinterpret_cast<type*>(0)->member))
	
#define SIMPLE_CAST_ENTRY (::cobalt::com::cast_fn)1

using create_fn = any* (*)(void* pv, const uid& iid);
using cast_fn = any* (*)(void* pv, const uid& iid, size_t data);

struct cast_entry {
	const uid* iid;
	size_t data;
	cast_fn func;
};

struct creator_data {
	create_fn func;
};

template <class Creator>
struct creator_thunk {
	static creator_data data;
};

template <class Creator>
creator_data creator_thunk<Creator>::data = { Creator::create_instance };

struct cache_data {
	size_t offset;
	create_fn func;
};

template <typename Creator, size_t Offset>
struct cache_thunk {
	static cache_data data;
};

template <typename Creator, size_t Offset>
cache_data cache_thunk<Creator, Offset>::data = { Offset, Creator::create_instance };

struct chain_data {
	size_t offset;
	const cast_entry* (*func)() noexcept;
};

template <typename Base, typename Derived>
struct chain_thunk {
	static chain_data data;
};

template <typename Base, typename Derived>
chain_data chain_thunk<Base, Derived>::data = { OFFSETOFCLASS(Base, Derived), Base::cast_entries };

class object_base {
public:
	object_base() noexcept = default;
	
	void set_void(void* pv) noexcept {}
	
	bool init() noexcept { return true; }
	void deinit() noexcept {}
	
	static void s_class_init() noexcept {}
	static void s_class_deinit() noexcept {}

protected:
	size_t internal_retain() noexcept { return ++_ref_count; }
	size_t internal_release() noexcept { return --_ref_count; }
	
	size_t outer_retain() noexcept { return _outer->retain(); }
	size_t outer_release() noexcept { return _outer->release(); }
	any* outer_cast(const uid& iid) noexcept { return _outer->cast(iid); }
	
	static any* s_internal_cast(void* pv, const cast_entry* entries, const uid& iid) noexcept {
		BOOST_ASSERT(pv);
		BOOST_ASSERT(entries);
		if (!pv || !entries) {
			last_error(make_error_code(std::errc::invalid_argument));
			return nullptr;
		}
		
		BOOST_ASSERT(entries[0].func == SIMPLE_CAST_ENTRY);
		if (entries[0].func != SIMPLE_CAST_ENTRY) {
			last_error(errc::failure);
			return nullptr;
		}
		
		if (iid == UIDOF(any)) {
			last_error(errc::success);
			return reinterpret_cast<any*>(reinterpret_cast<size_t>(pv) + entries[0].data);
		}
		
		for (auto entry = entries; entry->func; ++entry) {
			bool blind = !entry->iid;
			if (blind || entry->iid == &iid) {
				if (entry->func == SIMPLE_CAST_ENTRY) {
					BOOST_ASSERT(!blind);
					last_error(errc::success);
					return reinterpret_cast<any*>(reinterpret_cast<size_t>(pv) + entry->data);
				} else {
					auto id = entry->func(pv, iid, entry->data);
					if (last_error()) return nullptr;
					if (id || (!blind && !id)) {
						last_error(errc::success);
						return id;
					}
				}
			}
		}
		
		last_error(errc::no_such_interface);
		return nullptr;
	}
	
	static any* s_break(void* pv, const uid& iid, size_t data) noexcept {
		(void)pv;
		(void)iid;
		(void)data;
		// TODO: break into debugger
		BOOST_ASSERT(false);
		last_error(make_error_code(std::errc::operation_canceled));
		return nullptr;
	}
	
	static any* s_nointerface(void* pv, const uid& iid, size_t data) noexcept {
		last_error(errc::no_such_interface);
		return nullptr;
	}
	
	static any* s_create(void* pv, const uid& iid, size_t data) noexcept {
		last_error(errc::success);
		auto cd = reinterpret_cast<creator_data*>(data);
		return cd->func(reinterpret_cast<any*>(pv), iid);
	}
	
	static any* s_cache(void* pv, const uid& iid, size_t data) noexcept {
		last_error(errc::success);
		auto cd = reinterpret_cast<cache_data*>(data);
		auto id = reinterpret_cast<any**>((reinterpret_cast<size_t>(pv) + cd->offset));
		if (!*id && (*id = cd->func(reinterpret_cast<any*>(pv), UIDOF(any)))) {
			(*id)->retain();
			if (last_error()) {
				(*id)->release();
				*id = nullptr;
			}
		}
		return *id ? (*id)->cast(iid) : nullptr;
	}
	
	static any* s_delegate(void* pv, const uid& iid, size_t data) noexcept {
		last_error(errc::success);
		auto id = *reinterpret_cast<any**>(reinterpret_cast<size_t>(pv) + data);
		if (id) return id->cast(iid);
		last_error(make_error_code(std::errc::bad_address));
		return nullptr;
	}
	
	static any* s_chain(void* pv, const uid& iid, size_t data) noexcept {
		last_error(errc::success);
		auto cd = reinterpret_cast<chain_data*>(data);
		auto p = reinterpret_cast<void*>((reinterpret_cast<size_t>(pv) + cd->offset));
		return s_internal_cast(p, cd->func(), iid);
	}
	
protected:
	union {
		size_t _ref_count = 0;
		any* _outer;
	};
};

template <typename Owner>
class tear_off_object_base : public object_base {
public:
	using owner_type = Owner;
	
	void owner(owner_type* owner) noexcept { _owner = owner; }
	owner_type* owner() noexcept { return _owner; }

private:
	owner_type* _owner = nullptr;
};

#define BEGIN_CAST_MAP(x) public: \
	static const ::cobalt::com::cast_entry* cast_entries() noexcept { \
		using namespace ::cobalt::com; \
		using cast_map_class = x; \
		static const cast_entry entries[] = {

#define CAST_ENTRY_BREAK(x) \
			{ &UIDOF(x), 0, s_break },
	
#define CAST_ENTRY_NOINTERFACE(x) \
			{ &UIDOF(x), 0, s_nointerface },
	
#define CAST_ENTRY_FUNC(iid, data, func) \
			{ &Uid, data, func },

#define CAST_ENTRY_FUNC_BLIND(data, func) \
			{ nullptr, data, func }

#define CAST_ENTRY(x) \
			{ &UIDOF(x), OFFSETOFCLASS(x, cast_map_class), SIMPLE_CAST_ENTRY },
	
#define CAST_ENTRY_IID(iid, x) \
			{ &iid, OFFSETOFCLASS(x, cast_map_class), SIMPLE_CAST_ENTRY },

#define CAST_ENTRY2(x, x2) \
			{ &UIDOF(x), OFFSETOFCLASS2(x, x2, cast_map_class), SIMPLE_CAST_ENTRY },

#define CAST_ENTRY2_IID(iid, x, x2) \
			{ &iid, OFFSETOFCLASS2(x, x2, cast_map_class), SIMPLE_CAST_ENTRY },
	
#define CAST_ENTRY_TEAR_OFF(iid, x) \
			{ &iid, reinterpret_cast<size_t>(&creator_thunk<internal_creator<tear_off_object<x>>>::data), s_create },
	
#define CAST_ENTRY_CACHED_TEAR_OFF(iid, x, pid) \
			{ &iid, reinterpret_cast<size_t>(&cache_thunk<creator<cached_tear_off_object<x>>, offsetof(cast_map_class, pid)>::data), s_cache },
	
#define CAST_ENTRY_AGGREGATE(iid, pid) \
			{ &iid, OFFSETOFMEMBER(cast_map_class, pid), s_delegate },
	
#define CAST_ENTRY_AGGREGATE_BLIND(pid) \
			{ nullptr, OFFSETOFMEMBER(cast_map_class, pid), s_delegate },
	
#define CAST_ENTRY_AUTOAGGREGATE(iid, pid, clsid) \
			{ &iid, reinterpret_cast<size_t>(&cache_thunk<aggregate_creator<cast_map_class, &clsid>, offsetof(cast_map_class, pid)>::data), s_cache },
	
#define CAST_ENTRY_AUTOAGGREGATE_BLIND(pid, clsid) \
			{ nullptr, reinterpret_cast<size_t>(&cache_thunk<aggregate_creator<cast_map_class, &clsid>, offsetof(cast_map_class, pid)>::data), s_cache },
	
#define CAST_ENTRY_AUTOAGGREGATE_CLASS(iid, pid, x) \
			{ &iid, reinterpret_cast<size_t>(&cache_thunk<creator<aggregated_object<x>>, offsetof(cast_map_class, pid)>::data), s_cache },
	
#define CAST_ENTRY_AUTOAGGREGATE_CLASS_BLIND(pid, x) \
			{ nullptr, reinterpret_cast<size_t>(&cache_thunk<creator<aggregated_object<x>>, offsetof(cast_map_class, pid)>::data), s_cache },
	
#define CAST_ENTRY_CHAIN(class) \
			{ nullptr, reinterpret_cast<size_t>(&chain_thunk<class, cast_map_class>::data), s_chain },

#define END_CAST_MAP() \
			{ nullptr, 0, nullptr } \
		}; \
		return entries; \
	} \
	::cobalt::com::any* internal_cast(const ::cobalt::uid& iid) noexcept { \
		return s_internal_cast(this, cast_entries(), iid); \
	} \
	::cobalt::com::any* identity() const noexcept { \
		BOOST_ASSERT(cast_entries()->func == SIMPLE_CAST_ENTRY); \
		return reinterpret_cast<::cobalt::com::any*>(reinterpret_cast<size_t>(this) + cast_entries()->data); \
	} \
	virtual ::cobalt::com::any* controlling_object() const noexcept { \
		return identity(); \
	}

template <typename Base>
class stack_object : public Base {
public:
	using base_type = Base;
	
	explicit stack_object(any* outer = nullptr) noexcept {
		BOOST_ASSERT(!outer);
		this->set_void(nullptr);
		BOOST_VERIFY(_init_result = this->init());
	}
	
	~stack_object() {
		this->deinit();
		BOOST_ASSERT(this->_ref_count == 0);
	}
	
	bool init_result() const noexcept { return _init_result; }
	
	virtual size_t retain() noexcept override {
#ifdef BOOST_ASSERT_IS_VOID
		return 0;
#else
		return this->internal_retain();
#endif
	}
	virtual size_t release() noexcept override {
#ifdef BOOST_ASSERT_IS_VOID
		return 0;
#else
		return this->internal_release();
#endif
	}
	
	virtual any* cast(const uid& iid) noexcept override { return this->internal_cast(iid); }
	
private:
	bool _init_result = false;
};

template <typename Base>
class object : public Base {
public:
	using base_type = Base;
	
	explicit object(void* pv = nullptr) noexcept {}
	~object() { this->deinit(); }
	
	virtual size_t retain() noexcept override { return this->internal_retain(); }
	virtual size_t release() noexcept override {
		auto ref_count = this->internal_release();
		if (!ref_count)
			delete this;
		return ref_count;
	}
	virtual any* cast(const uid& iid) noexcept override { return this->internal_cast(iid); }
	
	static object<Base>* create_instance(any* outer = nullptr) noexcept {
		BOOST_ASSERT(!outer);
		if (outer)
			return nullptr;
		auto p = new(std::nothrow) object<Base>(nullptr);
		if (!p) {
			last_error(std::make_error_code(std::errc::not_enough_memory));
			return nullptr;
		}
		p->set_void(nullptr);
		if (!p->init()) {
			delete p;
			last_error(errc::failure);
			return nullptr;
		}
		last_error(errc::success);
		return p;
	}
};

template <typename Base>
class contained_object : public Base {
public:
	using base_type = Base;
	contained_object(void* pv) noexcept {
		BOOST_ASSERT(!this->_outer);
		BOOST_ASSERT(!!pv);
		this->_outer = reinterpret_cast<any*>(pv);
	}
	virtual size_t retain() noexcept override { return this->outer_retain(); }
	virtual size_t release() noexcept override { return this->outer_release(); }
	virtual any* cast(const uid& iid) noexcept override { return this->outer_cast(iid); }
	virtual any* controlling_object() const noexcept override { return this->_outer; }
};

template <class Contained>
class aggregated_object : public any, public object_base {
public:
	using base_type = Contained;
	
	explicit aggregated_object(void* pv) noexcept : _contained(pv) {}
	~aggregated_object() { this->deinit(); }
	
	bool init() noexcept { return object_base::init() && _contained.init(); }
	void deinit() noexcept { object_base::deinit(); _contained.deinit(); }
	
	virtual size_t retain() noexcept override { return this->internal_retain(); }
	virtual size_t release() noexcept override {
		auto ref_count = this->internal_release();
		if (!ref_count)
			delete this;
		return ref_count;
	}
	virtual any* cast(const uid& iid) noexcept override {
		if (iid == UIDOF(any)) {
			last_error(errc::success);
			return this;
		}
		return _contained.internal_cast(iid);
	}
	
	static aggregated_object<Contained>* create_instance(any* outer) noexcept {
		auto p = new(std::nothrow) aggregated_object<Contained>(outer);
		if (!p) {
			last_error(std::make_error_code(std::errc::not_enough_memory));
			return nullptr;
		}
		p->set_void(nullptr);
		if (!p->init()) {
			delete p;
			last_error(errc::failure);
			return nullptr;
		}
		last_error(errc::success);
		return p;
	}

private:
	contained_object<Contained> _contained;
};

template <class Contained>
class poly_object : public any, public object_base {
public:
	using base_type = Contained;
	
	explicit poly_object(void* pv) noexcept : _contained(pv ? pv : this) {}
	~poly_object() { this->deinit(); }
	
	bool init() noexcept { return object_base::init() && _contained.init(); }
	void deinit() noexcept { object_base::deinit(); _contained.deinit(); }
	
	virtual size_t retain() noexcept override { return this->internal_retain(); }
	virtual size_t release() noexcept override {
		auto ref_count = this->internal_release();
		if (!ref_count)
			delete this;
		return ref_count;
	}
	virtual any* cast(const uid& iid) noexcept override {
		if (iid == UIDOF(any)) {
			last_error(errc::success);
			return this;
		}
		return _contained.internal_cast(iid);
	}
	
	static poly_object<Contained>* create_instance(any* outer) noexcept {
		auto p = new(std::nothrow) poly_object<Contained>(outer);
		if (!p) {
			last_error(std::make_error_code(std::errc::not_enough_memory));
			return nullptr;
		}
		p->set_void(nullptr);
		if (!p->init()) {
			delete p;
			last_error(errc::failure);
			return nullptr;
		}
		last_error(errc::success);
		return p;
	}

private:
	contained_object<Contained> _contained;
};

template <typename Base>
class tear_off_object : public Base {
public:
	using base_type = Base;
	
	explicit tear_off_object(void* pv) noexcept {
		BOOST_ASSERT(!this->owner());
		BOOST_ASSERT(!!pv);
		this->owner(static_cast<typename Base::owner_type*>(pv));
		this->owner()->retain();
	}
	~tear_off_object() {
		this->deinit();
		this->owner()->release();
	}
	
	virtual size_t retain() noexcept override { return this->internal_retain(); }
	virtual size_t release() noexcept override {
		auto ref_count = this->internal_release();
		if (!ref_count)
			delete this;
		return ref_count;
	}
	virtual any* cast(const uid& iid) noexcept override { return this->owner()->cast(iid); }
};

template <class Contained>
class cached_tear_off_object : public any, public object_base {
public:
	using base_type = Contained;
	
	explicit cached_tear_off_object(void* pv) noexcept
		: _contained(static_cast<typename Contained::owner_type*>(pv)->controlling_object())
	{
		BOOST_ASSERT(!_contained.owner());
		BOOST_ASSERT(!!pv);
		_contained.owner(static_cast<typename Contained::owner_type*>(pv));
	}
	~cached_tear_off_object() { this->deinit(); }
	
	bool init() noexcept { return object_base::init() && _contained.init(); }
	void deinit() noexcept { object_base::deinit(); _contained.deinit(); }
	
	virtual size_t retain() noexcept override { return this->internal_retain(); }
	virtual size_t release() noexcept override {
		auto ref_count = this->internal_release();
		if (!ref_count)
			delete this;
		return ref_count;
	}
	virtual any* cast(const uid& iid) noexcept override {
		if (iid == UIDOF(any)) {
			last_error(errc::success);
			return this;
		}
		return _contained.internal_cast(iid);
	}

private:
	contained_object<Contained> _contained;
};

template <typename T>
struct creator {
	static any* create_instance(void* pv, const uid& iid) noexcept {
		auto p = new(std::nothrow) T(pv);
		if (!p) {
			last_error(std::make_error_code(std::errc::not_enough_memory));
			return nullptr;
		};
		p->set_void(pv);
		if (!p->init()) {
			delete p;
			last_error(errc::failure);
			return nullptr;
		}
		any* id = p->cast(iid);
		if (!id)
			delete p;
		return id;
	}
};

template <typename T>
struct internal_creator {
	static any* create_instance(void* pv, const uid& iid) noexcept {
		auto p = new(std::nothrow) T(pv);
		if (!p) {
			last_error(std::make_error_code(std::errc::not_enough_memory));
			return nullptr;
		};
		p->set_void(pv);
		if (!p->init()) {
			delete p;
			last_error(errc::failure);
			return nullptr;
		}
		any* id = p->internal_cast(iid);
		if (!id)
			delete p;
		return id;
	}
};

template <typename T, const uid* clsid>
struct aggregate_creator {
	static any* create_instance(void* pv, const uid& iid) noexcept {
		if (!pv) {
			last_error(std::make_error_code(std::errc::invalid_argument));
			return nullptr;
		}
		last_error(errc::success);
		T* p = static_cast<T*>(pv);
		return create_instance(p->controlling_object(), *clsid, UIDOF(any));
	}
};

template <typename T1, typename T2>
struct creator2 {
	static any* create_instance(void* pv, const uid& iid) noexcept {
		return !pv ? T1::create_instance(nullptr, iid) :
		             T2::create_instance(pv, iid);
	}
};

struct fail_creator {
	static any* create_instance(void* pv, const uid& iid) noexcept {
		last_error(errc::failure);
		return nullptr;
	}
};

#define DECLARE_AGGREGATABLE(x) public: \
	using creator_type = ::cobalt::com::creator2< \
	                         ::cobalt::com::creator<::cobalt::com::object<x>>, \
	                         ::cobalt::com::creator<::cobalt::com::aggregated_object<x>>>;
	
#define DECLARE_NOT_AGGREGATABLE(x) public: \
	using creator_type = ::cobalt::com::creator2< \
	                         ::cobalt::com::creator<::cobalt::com::object<x>>, \
	                         ::cobalt::com::fail_creator>;
	
#define DECLARE_ONLY_AGGREGATABLE(x) public: \
	using creator_type = ::cobalt::com::creator2< \
	                         ::cobalt::com::fail_creator, \
	                         ::cobalt::com::creator<::cobalt::com::aggregated_object<x>>>;

#define DECLARE_POLY_AGGREGATABLE(x) public: \
	using creator_type = ::cobalt::com::creator<::cobalt::com::poly_object<x>>;

class class_factory_impl : public object_base, public class_factory {
public:
	BEGIN_CAST_MAP(class_factory_impl)
		CAST_ENTRY(class_factory)
	END_CAST_MAP()
	
	void set_void(void* pv) noexcept { _creator = reinterpret_cast<create_fn>(pv); }
	
	virtual any* create_instance(any* outer, const uid& iid) noexcept override {
		BOOST_ASSERT(_creator);
		BOOST_ASSERT(!outer || (outer && iid == UIDOF(any)));
		if (outer && iid != UIDOF(any)) {
			last_error(errc::aggregation_not_supported);
			return nullptr;
		}
		return _creator(outer, iid);
	}
	
private:
	create_fn _creator = nullptr;
};

class class_factory_singleton_impl : public object_base, public class_factory {
public:
	BEGIN_CAST_MAP(class_factory_singleton_impl)
		CAST_ENTRY(class_factory)
	END_CAST_MAP()
	
	void set_void(void* pv) noexcept { _creator = reinterpret_cast<create_fn>(pv); }
	
	virtual any* create_instance(any* outer, const uid& iid) noexcept override {
		BOOST_ASSERT(!outer);
		if (outer) {
			last_error(errc::aggregation_not_supported);
			return nullptr;
		}
		if (!_object) {
			BOOST_ASSERT(_creator);
			_object = _creator(outer, iid);
			BOOST_ASSERT(_object);
		}
		return _object ? _object->cast(iid) : nullptr;
	}
	
private:
	create_fn _creator = nullptr;
	ref_ptr<any> _object;
};

#define DECLARE_CLASSFACTORY() \
	using class_factory_creator_type = ::cobalt::com::creator<::cobalt::com::object<::cobalt::com::class_factory_impl>>;
	
#define DECLARE_CLASSFACTORY_SINGLETON() \
	using class_factory_creator_type = ::cobalt::com::creator<::cobalt::com::object<::cobalt::com::class_factory_singleton_impl>>;

template <typename T>
class coclass {
protected:
	DECLARE_AGGREGATABLE(T)
	DECLARE_CLASSFACTORY()
	
	static const uid& s_class_uid() noexcept { return UIDOF(T); }
	
	template <typename Q>
	static ref_ptr<Q> s_create_instance(any* outer = nullptr) noexcept {
		return static_cast<Q*>(T::creator_type::create_instance(outer, UIDOF(Q)));
	}
};

struct object_entry {
	const uid* clsid;
	create_fn get_class_object;
	create_fn create_instance;
	mutable class_factory* factory;
	void (*class_init)();
	void (*class_deinit)();
};

#define BEGIN_OBJECT_MAP(x) public: \
	static const ::cobalt::com::object_entry* entries() noexcept { \
		using object_map_class = x; \
		static const ::cobalt::com::object_entry entries[] = {
	
#define OBJECT_ENTRY(class) { \
			&class::s_class_uid(), \
			&class::class_factory_creator_type::create_instance, \
			&class::creator_type::create_instance, \
			nullptr, \
			&class::s_class_init, \
			&class::s_class_deinit },
	
#define OBJECT_ENTRY_NON_CREATEABLE(class) { \
			&class::s_class_uid(), \
			nullptr, \
			nullptr, \
			nullptr, \
			&class::s_class_init, \
			&class::s_class_deinit },

#define END_OBJECT_MAP() \
			{ nullptr, nullptr, nullptr, nullptr, nullptr, nullptr } \
		}; \
		return entries; \
	}
	
class module_base;

namespace detail {

using namespace boost::intrusive;

using modules_list_hook = slist_base_hook<link_mode<normal_link>>;
using modules_list = slist<module_base, base_hook<modules_list_hook>, constant_time_size<false>>;

} // namespace detail
	
class module_base : public detail::modules_list_hook {
public:
	module_base() noexcept { modules().push_front(*this); }
	virtual ~module_base() = default;
	
	virtual class_factory* get_class_object(const uid& clsid) noexcept = 0;
	virtual any* create_instance(any* outer, const uid& clsid, const uid& iid) noexcept = 0;

protected:
	static void s_init(const object_entry* entries) noexcept {
		for (auto entry = entries; entry->clsid; ++entry) {
			entry->class_init();
		}
	}
	
	static void s_deinit(const object_entry* entries) noexcept {
		for (auto entry = entries; entry->clsid; ++entry) {
			entry->class_deinit();
			// Release factory
			if (entry->factory) {
				entry->factory->release();
				entry->factory = nullptr;
			}
		}
	}
	
	static class_factory* s_get_class_object(const object_entry* entries, const uid& clsid) noexcept {
		for (auto entry = entries; entry->clsid; ++entry) {
			if (entry->get_class_object && entry->clsid == &clsid) {
				if (!entry->factory && (entry->factory = static_cast<class_factory*>(
					entry->get_class_object(reinterpret_cast<any*>(entry->create_instance), UIDOF(any)))))
				{
					entry->factory->retain();
				}
				BOOST_ASSERT(!!entry->factory);
				last_error(errc::success);
				return entry->factory;
			}
		}
		
		last_error(errc::no_such_class);
		return nullptr;
	}
	
private:
	static detail::modules_list& modules() noexcept {
		static detail::modules_list instance;
		return instance;
	}
	
	// Global functions
	
	friend class_factory* get_class_object(const uid& clsid) noexcept {
		for (auto&& m : modules()) {
			if (auto p = m.get_class_object(clsid))
				return p;
		}
		return nullptr;
	}

	friend any* create_instance(any* outer, const uid& clsid, const uid& iid) noexcept {
		for (auto&& m : modules()) {
			if (auto p = m.create_instance(outer, clsid, iid))
				return p;
		}
		return nullptr;
	}
};

template <typename T>
class module : public module_base {
public:
	module() noexcept {
		s_init(static_cast<T*>(this)->entries());
	}
	
	~module() {
		s_deinit(static_cast<T*>(this)->entries());
	}

	virtual class_factory* get_class_object(const uid& clsid) noexcept override {
		return s_get_class_object(static_cast<T*>(this)->entries(), clsid);
	}
	
	virtual any* create_instance(any* outer, const uid& clsid, const uid& iid) noexcept override {
		if (auto cf = get_class_object(clsid))
			return cf->create_instance(outer, iid);
		last_error(errc::no_such_class);
		return nullptr;
	}
	
	template <typename Q>
	ref_ptr<Q> create_instance(any* outer, const uid& clsid) noexcept {
		return static_cast<Q*>(create_instance(outer, clsid, UIDOF(Q)));
	}
	
	template <typename Q>
	ref_ptr<Q> create_instance(const uid& clsid) noexcept {
		return static_cast<Q*>(create_instance(nullptr, clsid, UIDOF(Q)));
	}
};

} // namespace com
} // namespace cobalt

#endif // COBALT_COM_BASE_HPP_INCLUDED
