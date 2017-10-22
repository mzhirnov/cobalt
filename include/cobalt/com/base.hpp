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

using create_fn = unknown* (*)(void* pv, const iid& iid, std::error_code& ec);
using cast_fn = unknown* (*)(void* pv, const iid& iid, size_t data, std::error_code& ec);

struct cast_entry {
	const iid* iid;
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
	
	bool initialize() noexcept { return true; }
	void shutdown() noexcept {}
	
	static void s_initialize() noexcept {}
	static void s_shutdown() noexcept {}

protected:
	size_t internal_retain() noexcept { return ++_ref_count; }
	size_t internal_release() noexcept { return --_ref_count; }
	
	size_t outer_retain() noexcept { return _outer->retain(); }
	size_t outer_release() noexcept { return _outer->release(); }
	unknown* outer_cast(const iid& iid, std::error_code& ec) noexcept { return _outer->cast(iid, ec); }
	
	static unknown* s_internal_cast(void* pv, const cast_entry* entries, const iid& iid, std::error_code& ec) noexcept {
		BOOST_ASSERT(pv);
		BOOST_ASSERT(entries);
		if (!pv || !entries) {
			ec = make_error_code(std::errc::invalid_argument);
			return nullptr;
		}
		
		BOOST_ASSERT(entries[0].func == SIMPLE_CAST_ENTRY);
		if (entries[0].func != SIMPLE_CAST_ENTRY) {
			ec = errc::failure;
			return nullptr;
		}
		
		if (iid == IIDOF(unknown)) {
			ec = errc::success;
			return reinterpret_cast<unknown*>(reinterpret_cast<size_t>(pv) + entries[0].data);
		}
		
		ec.clear();
		
		for (auto entry = entries; entry->func; ++entry) {
			bool blind = !entry->iid;
			if (blind || entry->iid == &iid) {
				if (entry->func == SIMPLE_CAST_ENTRY) {
					BOOST_ASSERT(!blind);
					ec = errc::success;
					return reinterpret_cast<unknown*>(reinterpret_cast<size_t>(pv) + entry->data);
				} else {
					auto unk = entry->func(pv, iid, entry->data, ec);
					if (ec) return nullptr;
					if (unk || (!blind && !unk)) {
						ec = errc::success;
						return unk;
					}
				}
			}
		}
		
		ec = errc::no_such_interface;
		return nullptr;
	}
	
	static unknown* s_break(void* pv, const iid& iid, size_t data, std::error_code& ec) noexcept {
		(void)pv;
		(void)iid;
		(void)data;
		// TODO: break into debugger
		BOOST_ASSERT(false);
		ec = make_error_code(std::errc::operation_canceled);
		return nullptr;
	}
	
	static unknown* s_nointerface(void* pv, const iid& iid, size_t data, std::error_code& ec) noexcept {
		ec = errc::no_such_interface;
		return nullptr;
	}
	
	static unknown* s_create(void* pv, const iid& iid, size_t data, std::error_code& ec) noexcept {
		ec.clear();
		creator_data* cd = reinterpret_cast<creator_data*>(data);
		return cd->func(reinterpret_cast<unknown*>(pv), iid, ec);
	}
	
	static unknown* s_cache(void* pv, const iid& iid, size_t data, std::error_code& ec) noexcept {
		ec.clear();
		cache_data* cd = reinterpret_cast<cache_data*>(data);
		unknown** unk = reinterpret_cast<unknown**>((reinterpret_cast<size_t>(pv) + cd->offset));
		if (!*unk && (*unk = cd->func(reinterpret_cast<unknown*>(pv), IIDOF(unknown), ec))) {
			(*unk)->retain();
			if (ec) {
				(*unk)->release();
				*unk = nullptr;
			}
		}
		return *unk ? (*unk)->cast(iid, ec) : nullptr;
	}
	
	static unknown* s_delegate(void* pv, const iid& iid, size_t data, std::error_code& ec) noexcept {
		ec.clear();
		unknown* unk = *reinterpret_cast<unknown**>(reinterpret_cast<size_t>(pv) + data);
		if (unk) return unk->cast(iid, ec);
		ec = make_error_code(std::errc::bad_address);
		return nullptr;
	}
	
	static unknown* s_chain(void* pv, const iid& iid, size_t data, std::error_code& ec) noexcept {
		ec.clear();
		chain_data* cd = reinterpret_cast<chain_data*>(data);
		void* p = reinterpret_cast<void*>((reinterpret_cast<size_t>(pv) + cd->offset));
		return s_internal_cast(p, cd->func(), iid, ec);
	}
	
protected:
	union {
		size_t _ref_count = 0;
		unknown* _outer;
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
			{ &IIDOF(x), 0, s_break },
	
#define CAST_ENTRY_NOINTERFACE(x) \
			{ &IIDOF(x), 0, s_nointerface },
	
#define CAST_ENTRY_FUNC(iid, data, func) \
			{ &iid, data, func },

#define CAST_ENTRY_FUNC_BLIND(data, func) \
			{ nullptr, data, func }

#define CAST_ENTRY(x) \
			{ &IIDOF(x), OFFSETOFCLASS(x, cast_map_class), SIMPLE_CAST_ENTRY },
	
#define CAST_ENTRY_IID(iid, x) \
			{ &iid, OFFSETOFCLASS(x, cast_map_class), SIMPLE_CAST_ENTRY },

#define CAST_ENTRY2(x, x2) \
			{ &IIDOF(x), OFFSETOFCLASS2(x, x2, cast_map_class), SIMPLE_CAST_ENTRY },

#define CAST_ENTRY2_IID(iid, x, x2) \
			{ &iid, OFFSETOFCLASS2(x, x2, cast_map_class), SIMPLE_CAST_ENTRY },
	
#define CAST_ENTRY_TEAR_OFF(iid, x) \
			{ &iid, reinterpret_cast<size_t>(&creator_thunk<internal_creator<tear_off_object<x>>>::data), s_create },
	
#define CAST_ENTRY_CACHED_TEAR_OFF(iid, x, punk) \
			{ &iid, reinterpret_cast<size_t>(&cache_thunk<creator<cached_tear_off_object<x>>, offsetof(cast_map_class, punk)>::data), s_cache },
	
#define CAST_ENTRY_AGGREGATE(iid, punk) \
			{ &iid, OFFSETOFMEMBER(cast_map_class, punk), s_delegate },
	
#define CAST_ENTRY_AGGREGATE_BLIND(punk) \
			{ nullptr, OFFSETOFMEMBER(cast_map_class, punk), s_delegate },
	
#define CAST_ENTRY_AUTOAGGREGATE(iid, x, punk) \
			{ &iid, reinterpret_cast<size_t>(&cache_thunk<creator<agg_object<x>>, offsetof(cast_map_class, punk)>::data), s_cache },
	
#define CAST_ENTRY_AUTOAGGREGATE_BLIND(x, punk) \
			{ nullptr, reinterpret_cast<size_t>(&cache_thunk<creator<agg_object<x>>, offsetof(cast_map_class, punk)>::data), s_cache },
	
#define CAST_ENTRY_CHAIN(class) \
			{ nullptr, reinterpret_cast<size_t>(&chain_thunk<class, cast_map_class>::data), s_chain },

#define END_CAST_MAP() \
			{ nullptr, 0, nullptr } \
		}; \
		return entries; \
	} \
	::cobalt::com::unknown* internal_cast(const ::cobalt::com::iid& iid, std::error_code& ec) noexcept { \
		return s_internal_cast(this, cast_entries(), iid, ec); \
	} \
	::cobalt::com::unknown* get_unknown() const noexcept { \
		BOOST_ASSERT(cast_entries()->func == SIMPLE_CAST_ENTRY); \
		return reinterpret_cast<::cobalt::com::unknown*>(reinterpret_cast<size_t>(this) + cast_entries()->data); \
	} \
	virtual ::cobalt::com::unknown* controlling_unknown() const noexcept { \
		return get_unknown(); \
	}

template <typename Base>
class stack_object : public Base {
public:
	using base_type = Base;
	
	explicit stack_object(unknown* outer = nullptr) noexcept {
		BOOST_ASSERT(!outer);
		this->set_void(nullptr);
		BOOST_VERIFY(_init_result = this->initialize());
	}
	
	~stack_object() {
		this->shutdown();
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
	
	virtual unknown* cast(const iid& iid, std::error_code& ec) noexcept override { return this->internal_cast(iid, ec); }
	
private:
	bool _init_result;
};

template <typename Base>
class object : public Base {
public:
	using base_type = Base;
	
	explicit object(void* pv = nullptr) noexcept {}
	~object() { this->shutdown(); }
	
	virtual size_t retain() noexcept override { return this->internal_retain(); }
	virtual size_t release() noexcept override {
		auto ref_count = this->internal_release();
		if (!ref_count)
			delete this;
		return ref_count;
	}
	virtual unknown* cast(const iid& iid, std::error_code& ec) noexcept override { return this->internal_cast(iid, ec); }
	
	static object<Base>* create_instance(unknown* outer = nullptr) noexcept {
		BOOST_ASSERT(!outer);
		if (outer)
			return nullptr;
		auto obj = new(std::nothrow) object<Base>(nullptr);
		if (obj) {
			obj->set_void(nullptr);
			if (!obj->initialize()) {
				delete obj;
				obj = nullptr;
			}
		}
		return obj;
	}
};

template <typename Base>
class contained_object : public Base {
public:
	using base_type = Base;
	contained_object(void* pv) noexcept {
		BOOST_ASSERT(!this->_outer);
		BOOST_ASSERT(!!pv);
		this->_outer = reinterpret_cast<unknown*>(pv);
	}
	virtual size_t retain() noexcept override { return this->outer_retain(); }
	virtual size_t release() noexcept override { return this->outer_release(); }
	virtual unknown* cast(const iid& iid, std::error_code& ec) noexcept override { return this->outer_cast(iid, ec); }
	virtual unknown* controlling_unknown() const noexcept override { return this->_outer; }
};

template <class Contained>
class agg_object : public unknown, public object_base {
public:
	using base_type = Contained;
	
	explicit agg_object(void* pv) noexcept : _contained(pv) {}
	~agg_object() { this->shutdown(); }
	
	bool initialize() noexcept { return object_base::initialize()&& _contained.initialize(); }
	void shutdown() noexcept { object_base::shutdown(); _contained.shutdown(); }
	
	virtual size_t retain() noexcept override { return this->internal_retain(); }
	virtual size_t release() noexcept override {
		auto ref_count = this->internal_release();
		if (!ref_count)
			delete this;
		return ref_count;
	}
	virtual unknown* cast(const iid& iid, std::error_code& ec) noexcept override {
		if (iid == IIDOF(unknown)) {
			ec = errc::success;
			return this;
		}
		return _contained.internal_cast(iid, ec);
	}
	
	static agg_object<Contained>* create_instance(unknown* outer) noexcept {
		auto obj = new(std::nothrow) agg_object<Contained>(outer);
		if (obj) {
			obj->set_void(nullptr);
			if (!obj->initialize()) {
				delete obj;
				obj = nullptr;
			}
		}
		return obj;
	}

private:
	contained_object<Contained> _contained;
};

template <class Contained>
class poly_object : public unknown, public object_base {
public:
	using base_type = Contained;
	
	explicit poly_object(void* pv) noexcept : _contained(pv ? pv : this) {}
	~poly_object() { this->shutdown(); }
	
	bool initialize() noexcept { return object_base::initialize() && _contained.initialize(); }
	void shutdown() noexcept { object_base::shutdown(); _contained.shutdown(); }
	
	virtual size_t retain() noexcept override { return this->internal_retain(); }
	virtual size_t release() noexcept override {
		auto ref_count = this->internal_release();
		if (!ref_count)
			delete this;
		return ref_count;
	}
	virtual unknown* cast(const iid& iid, std::error_code& ec) noexcept override {
		if (iid == IIDOF(unknown)) {
			ec = errc::success;
			return this;
		}
		return _contained.internal_cast(iid, ec);
	}
	
	static poly_object<Contained>* create_instance(unknown* outer) noexcept {
		auto obj = new(std::nothrow) poly_object<Contained>(outer);
		if (obj) {
			obj->set_void(nullptr);
			if (!obj->initialize()) {
				delete obj;
				obj = nullptr;
			}
		}
		return obj;
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
		this->shutdown();
		this->owner()->release();
	}
	
	virtual size_t retain() noexcept override { return this->internal_retain(); }
	virtual size_t release() noexcept override {
		auto ref_count = this->internal_release();
		if (!ref_count)
			delete this;
		return ref_count;
	}
	virtual unknown* cast(const iid& iid, std::error_code& ec) noexcept override { return this->owner()->cast(iid, ec); }
};

template <class Contained>
class cached_tear_off_object : public unknown, public object_base {
public:
	using base_type = Contained;
	
	explicit cached_tear_off_object(void* pv) noexcept
		: _contained(static_cast<typename Contained::owner_type*>(pv)->controlling_unknown())
	{
		BOOST_ASSERT(!_contained.owner());
		BOOST_ASSERT(!!pv);
		_contained.owner(static_cast<typename Contained::owner_type*>(pv));
	}
	~cached_tear_off_object() { this->shutdown(); }
	
	bool initialize() noexcept { return object_base::initialize() && _contained.initialize(); }
	void shutdown() noexcept { object_base::shutdown(); _contained.shutdown(); }
	
	virtual size_t retain() noexcept override { return this->internal_retain(); }
	virtual size_t release() noexcept override {
		auto ref_count = this->internal_release();
		if (!ref_count)
			delete this;
		return ref_count;
	}
	virtual unknown* cast(const iid& iid, std::error_code& ec) noexcept override {
		if (iid == IIDOF(unknown)) {
			ec = errc::success;
			return this;
		}
		return _contained.internal_cast(iid, ec);
	}

private:
	contained_object<Contained> _contained;
};

template <typename T>
struct creator {
	static unknown* create_instance(void* pv, const iid& iid, std::error_code& ec) noexcept {
		ec.clear();
		auto p = new(std::nothrow) T(pv);
		if (!p) return nullptr;
		p->set_void(pv);
		if (!p->initialize()) {
			delete p;
			ec = errc::failure;
			return nullptr;
		}
		unknown* unk = p->cast(iid, ec);
		if (!unk)
			delete p;
		return unk;
	}
};

template <typename T>
struct internal_creator {
	static unknown* create_instance(void* pv, const iid& iid, std::error_code& ec) noexcept {
		ec.clear();
		auto p = new(std::nothrow) T(pv);
		if (!p) return nullptr;
		p->set_void(pv);
		if (!p->initialize()) {
			delete p;
			ec = errc::failure;
			return nullptr;
		}
		unknown* unk = p->internal_cast(iid, ec);
		if (!unk)
			delete p;
		return unk;
	}
};

template <typename T1, typename T2>
struct creator2 {
	static unknown* create_instance(void* pv, const iid& iid, std::error_code& ec) noexcept {
		return !pv ? T1::create_instance(nullptr, iid, ec) :
		             T2::create_instance(pv, iid, ec);
	}
};

struct fail_creator {
	static unknown* create_instance(void* pv, const iid& iid, std::error_code& ec) noexcept {
		ec = errc::failure;
		return nullptr;
	}
};

#define DECLARE_AGGREGATABLE(x) public: \
	using creator_type = ::cobalt::com::creator2< \
	                         ::cobalt::com::creator<::cobalt::com::object<x>>, \
	                         ::cobalt::com::creator<::cobalt::com::agg_object<x>>>;
	
#define DECLARE_NOT_AGGREGATABLE(x) public: \
	using creator_type = ::cobalt::com::creator2< \
	                         ::cobalt::com::creator<::cobalt::com::object<x>>, \
	                         ::cobalt::com::fail_creator>;
	
#define DECLARE_ONLY_AGGREGATABLE(x) public: \
	using creator_type = ::cobalt::com::creator2< \
	                         ::cobalt::com::fail_creator, \
	                         ::cobalt::com::creator<::cobalt::com::agg_object<x>>>;

#define DECLARE_POLY_AGGREGATABLE(x) public: \
	using creator_type = ::cobalt::com::creator<::cobalt::com::poly_object<x>>;

class class_factory_impl : public object_base, public class_factory {
public:
	BEGIN_CAST_MAP(class_factory_impl)
		CAST_ENTRY(class_factory)
	END_CAST_MAP()
	
	void set_void(void* pv) noexcept { _creator = reinterpret_cast<create_fn>(pv); }
	
	virtual unknown* create_instance(unknown* outer, const iid& iid, std::error_code& ec) noexcept override {
		BOOST_ASSERT(_creator);
		BOOST_ASSERT(!outer || (outer && &iid == &IIDOF(unknown)));
		if (outer && &iid != &IIDOF(unknown)) {
			ec = errc::aggregation_not_supported;
			return nullptr;
		}
		return _creator(outer, iid, ec);
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
	
	virtual unknown* create_instance(unknown* outer, const iid& iid, std::error_code& ec) noexcept override {
		BOOST_ASSERT(!outer);
		if (outer) {
			ec = errc::aggregation_not_supported;
			return nullptr;
		}
		if (!_object) {
			BOOST_ASSERT(_creator);
			_object = _creator(outer, iid, ec);
			BOOST_ASSERT(_object);
		}
		return _object->cast(iid, ec);
	}
	
private:
	create_fn _creator = nullptr;
	ref_ptr<unknown> _object;
};

#define DECLARE_CLASSFACTORY() \
	using class_factory_creator_type = ::cobalt::com::creator<::cobalt::com::object<::cobalt::com::class_factory_impl>>;
	
#define DECLARE_CLASSFACTORY_SINGLETON() \
	using class_factory_creator_type = ::cobalt::com::creator<::cobalt::com::object<::cobalt::com::class_factory_singleton_impl>>;

template <typename T>
class coclass {
public:
	DECLARE_AGGREGATABLE(T)
	DECLARE_CLASSFACTORY()
	
	static const clsid& s_object_clsid() noexcept { return IIDOF(T); }
	
	template <typename Q>
	static ref_ptr<Q> create_instance(unknown* outer = nullptr, std::error_code& ec = last_error()) noexcept {
		return static_cast<Q*>(T::creator_type::create_instance(outer, IIDOF(Q), ec));
	}
};

struct object_entry {
	const clsid* clsid;
	create_fn get_class_object;
	create_fn create_instance;
	mutable unknown* factory;
	void (*initialize)();
	void (*shutdown)();
};

#define BEGIN_OBJECT_MAP(x) public: \
	static const ::cobalt::com::object_entry* entries() noexcept { \
		using object_map_class = x; \
		static const ::cobalt::com::object_entry entries[] = {
	
#define OBJECT_ENTRY(class) {\
			&class::s_object_clsid(), \
			&class::class_factory_creator_type::create_instance, \
			&class::creator_type::create_instance, \
			nullptr, \
			&class::s_initialize, \
			&class::s_shutdown },
	
#define OBJECT_ENTRY_NON_CREATEABLE(class) \
			{ &class::s_object_clsid(), nullptr, nullptr, nullptr, &class::s_initialize, &class::s_shutdown },

#define END_OBJECT_MAP() \
			{ nullptr, nullptr, nullptr, nullptr, nullptr, nullptr } \
		}; \
		return entries; \
	}
	
class module_base {
protected:
	static void s_initialize(const object_entry* entries) noexcept {
		for (auto entry = entries; entry->clsid; ++entry) {
			entry->initialize();
		}
	}
	
	static void s_shutdown(const object_entry* entries) noexcept {
		for (auto entry = entries; entry->clsid; ++entry) {
			entry->shutdown();
			// Release factory
			if (entry->factory) {
				entry->factory->release();
				entry->factory = nullptr;
			}
		}
	}
	
	static unknown* s_get_class_object(const object_entry* entries, const clsid& clsid, std::error_code& ec) noexcept {
		ec.clear();
		for (auto entry = entries; entry->clsid; ++entry) {
			if (entry->get_class_object && entry->clsid == &clsid) {
				if (!entry->factory && (entry->factory = entry->get_class_object(reinterpret_cast<unknown*>(entry->create_instance), IIDOF(unknown), ec)))
					entry->factory->retain();
				BOOST_ASSERT(!!entry->factory);
				ec = errc::success;
				return entry->factory;
			}
		}
		
		ec = errc::no_such_class;
		return nullptr;
	}
};

template <typename T>
class module : public module_base {
public:
	module() noexcept {
		BOOST_ASSERT(!_instance);
		_instance = this;
		
		s_initialize(static_cast<T*>(this)->entries());
	}
	
	virtual ~module() {
		s_shutdown(static_cast<T*>(this)->entries());
		
		BOOST_ASSERT(!!_instance);
		_instance = nullptr;
	}
	
	static module* instance() noexcept { BOOST_ASSERT(!!_instance); return _instance; }
	
	unknown* get_class_object(const clsid& clsid, std::error_code& ec = last_error()) noexcept {
		return s_get_class_object(static_cast<T*>(this)->entries(), clsid, ec);
	}
	
	unknown* create_instance(unknown* outer, const clsid& clsid, const iid& iid, std::error_code& ec = last_error()) noexcept {
		ec.clear();
		if (auto unk = get_class_object(clsid, ec)) {
			if (auto cf = (class_factory*)unk->cast(IIDOF(class_factory), ec))
				return cf->create_instance(outer, iid, ec);
		}
		ec = errc::no_such_class;
		return nullptr;
	}
	
	template <typename Q>
	ref_ptr<Q> create_instance(unknown* outer, const clsid& clsid, std::error_code& ec = last_error()) noexcept {
		return static_cast<Q*>(create_instance(outer, clsid, IIDOF(Q), ec));
	}
	
	template <typename Q>
	ref_ptr<Q> create_instance(const clsid& clsid, std::error_code& ec = last_error()) noexcept {
		return static_cast<Q*>(create_instance(nullptr, clsid, IIDOF(Q), ec));
	}
	
private:
	static module* _instance;
};

template <typename T> module<T>* module<T>::_instance = nullptr;

} // namespace com
} // namespace cobalt

#endif // COBALT_COM_BASE_HPP_INCLUDED
