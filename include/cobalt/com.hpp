#ifndef COBALT_COM_HPP_INCLUDED
#define COBALT_COM_HPP_INCLUDED

#pragma once

// Interfaces in this file:
//     unknown
//     class_factory
//
// Classes in this file:
//     object_base
//     tear_off_object_base
//     stack_object
//     object
//     contained_object
//     aggregatable_object
//     tear_off_object
//     cached_tear_off_object
//     coclass
//     module
//
// Functions in this file:
//     same_objects

#include <cobalt/utility/type_index.hpp>
#include <cobalt/utility/intrusive.hpp>

#include <boost/assert.hpp>

namespace cobalt {
namespace com {

using iid = type_info;
using clsid = iid;

#define IIDOF(x) type_id<x>().type_info()

class unknown {
public:
	virtual ~unknown() = default;
	virtual size_t retain() noexcept = 0;
	virtual size_t release() noexcept = 0;
	virtual unknown* cast(const iid& iid) noexcept = 0;
	
	template <typename Q> ref_ptr<Q> cast() noexcept { return static_cast<Q*>(cast(IIDOF(Q))); }
	
	friend void intrusive_ptr_add_ref(unknown* p) noexcept { p->retain(); }
	friend void intrusive_ptr_release(unknown* p) noexcept { p->release(); }
};

class class_factory : public unknown {
public:
	virtual unknown* create_instance(unknown* outer, const iid& iid) noexcept = 0;
	
	template <typename Q> ref_ptr<Q> create_instance(unknown* outer = nullptr) noexcept
		{ return static_cast<Q*>(create_instance(outer, IIDOF(Q))); }
};

bool same_objects(unknown* obj1, unknown* obj2) noexcept {
	if (!obj1 || !obj2)
		return obj1 == obj2;		
	return obj1->cast<unknown>() == obj2->cast<unknown>();
}

#define OFFSETOFCLASS(base, derived) \
	(reinterpret_cast<size_t>(static_cast<base*>((derived*)8)) - 8)
	
#define OFFSETOFCLASS2(base, base2, derived) \
	(reinterpret_cast<size_t>(static_cast<base*>(static_cast<base2*>((derived*)8))) - 8)
	
#define SIMPLE_CAST_ENTRY (::cobalt::com::cast_fn)1

using creator_fn = unknown* (*)(unknown* outer, const iid& iid);
using cast_fn = unknown* (*)(void* pv, const iid& iid, size_t data);

struct cast_entry {
	const iid* iid;
	size_t data;
	cast_fn func;
};

struct creator_thunk {
	creator_fn func;
};

template <class Creator>
struct creator_thunk_data {
	static creator_thunk data;
};

template <class Creator>
creator_thunk creator_thunk_data<Creator>::data = { Creator::create_instance };

struct cache_thunk {
	size_t offset;
	creator_fn func;
};

template <typename Creator, size_t Offset>
struct cache_thunk_data {
	static cache_thunk data;
};

template <typename Creator, size_t Offset>
cache_thunk cache_thunk_data<Creator, Offset>::data = { Offset, Creator::create_instance };

struct chain_thunk {
	size_t offset;
	const cast_entry* (*func)() noexcept;
};

template <typename Base, typename Derived>
struct chain_thunk_data {
	static chain_thunk data;
};

template <typename Base, typename Derived>
chain_thunk chain_thunk_data<Base, Derived>::data = { OFFSETOFCLASS(Base, Derived), Base::entries };

class object_base {
public:
	explicit object_base(unknown* outer = nullptr) noexcept : _outer(outer) {}
	
	void set_void(void* p) noexcept {}
	
	bool initialize() noexcept { return true; }
	void uninitialize() noexcept {}

protected:
	size_t internal_retain() noexcept { return ++_ref_count; }
	size_t internal_release() noexcept { return --_ref_count; }
	
	size_t outer_retain() noexcept { return _outer->retain(); }
	size_t outer_release() noexcept { return _outer->release(); }
	unknown* outer_cast(const iid& iid) noexcept { return _outer->cast(iid); }
	
	static unknown* s_internal_cast(void* pv, const cast_entry* entries, const iid& iid) noexcept {
		BOOST_ASSERT(pv);
		BOOST_ASSERT(entries);
		if (!pv || !entries)
			return nullptr;
		
		BOOST_ASSERT(entries[0].func == SIMPLE_CAST_ENTRY);
		if (iid == IIDOF(unknown))
			return reinterpret_cast<unknown*>(reinterpret_cast<size_t>(pv) + entries[0].data);
		
		for (auto entry = entries; entry->func; ++entry) {
			bool blind = !entry->iid;
			if (blind || entry->iid == &iid) {
				if (entry->func == SIMPLE_CAST_ENTRY) {
					BOOST_ASSERT(!blind);
					return reinterpret_cast<unknown*>(reinterpret_cast<size_t>(pv) + entry->data);
				} else {
					auto unknown_ = entry->func(pv, iid, entry->data);
					if (unknown_ || (!blind && !unknown_))
						return unknown_;
				}
			}
		}
		
		return nullptr;
	}
	
	static unknown* s_break(void* pv, const iid& iid, size_t data) noexcept {
		(void)pv;
		(void)iid;
		(void)data;
		// TODO: DebugBreak
		return nullptr;
	}
	
	static unknown* s_nointerface(void* pv, const iid& iid, size_t data) noexcept {
		return nullptr;
	}
	
	static unknown* s_creator(void* pv, const iid& iid, size_t data) noexcept {
		creator_thunk* thunk = reinterpret_cast<creator_thunk*>(data);
		return thunk->func(reinterpret_cast<unknown*>(pv), iid);
	}
	
	static unknown* s_cache(void* pv, const iid& iid, size_t data) noexcept {
		cache_thunk* thunk = reinterpret_cast<cache_thunk*>(data);
		unknown** unk = reinterpret_cast<unknown**>((reinterpret_cast<size_t>(pv) + thunk->offset));
		if (*unk || (*unk = thunk->func(reinterpret_cast<unknown*>(pv), IIDOF(unknown))))
			return (*unk)->cast(iid);
		return nullptr;
	}
	
	static unknown* s_delegate(void* pv, const iid& iid, size_t data) noexcept {
		unknown* unk = *reinterpret_cast<unknown**>(reinterpret_cast<size_t>(pv) + data);
		return unk ? unk->cast(iid) : nullptr;
	}
	
	static unknown* s_chain(void* pv, const iid& iid, size_t data) noexcept {
		chain_thunk* thunk = reinterpret_cast<chain_thunk*>(data);
		void* p = reinterpret_cast<void*>((reinterpret_cast<size_t>(pv) + thunk->offset));
		return s_internal_cast(p, thunk->func(),iid);
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
	static const ::cobalt::com::cast_entry* entries() noexcept { \
		using cast_map_class = x; \
		static const ::cobalt::com::cast_entry entries[] = {

#define CAST_ENTRY_BREAK(x) \
			{ &IIDOF(x), nullptr, s_break },
	
#define CAST_ENTRY_NOINTERFACE(x) \
			{ &IIDOF(x), nullptr, s_nointerface },

#define CAST_ENTRY(x) \
			{ &IIDOF(x), OFFSETOFCLASS(x, cast_map_class), SIMPLE_CAST_ENTRY },
	
#define CAST_ENTRY_IID(iid, x) \
			{ &iid, OFFSETOFCLASS(x, cast_map_class), SIMPLE_CAST_ENTRY },

#define CAST_ENTRY2(x, x2) \
			{ &IIDOF(x), OFFSETOFCLASS2(x, x2, cast_map_class), SIMPLE_CAST_ENTRY },

#define CAST_ENTRY2_IID(iid, x, x2) \
			{ &iid, OFFSETOFCLASS2(x, x2, cast_map_class), SIMPLE_CAST_ENTRY },
	
#define CAST_ENTRY_FUNC(iid, data, func) \
			{ &iid, data, func },

#define CAST_ENTRY_FUNC_BLIND(data, func) \
			{ nullptr, data, func }
	
#define CAST_ENTRY_TEAR_OFF(iid, x) \
			{ &iid, reinterpret_cast<size_t>(&creator_thunk_data<internal_creator<tear_off_object<x>>>::data), s_creator },
	
#define CAST_ENTRY_CACHED_TEAR_OFF(iid, x, unk) \
			{ &iid, reinterpret_cast<size_t>(&cache_thunk_data<creator<cached_tear_off_object<x>>, offsetof(cast_map_class, unk)>::data), s_cache },
	
#define CAST_ENTRY_AGGREGATE(iid, punk) \
			{ &iid, offsetof(cast_map_class, punk), s_delegate },
	
#define CAST_ENTRY_AGGREGATE_BLIND(punk) \
			{ nullptr, offsetof(cast_map_class, punk), s_delegate },
	
#define CAST_ENTRY_CHAIN(class) \
			{ nullptr, reinterpret_cast<size_t>(&::cobalt::com::chain_thunk_data<class, cast_map_class>::data), s_chain },

#define END_CAST_MAP() \
			{ nullptr, 0, nullptr } \
		}; \
		return entries; \
	} \
	::cobalt::com::unknown* internal_cast(const ::cobalt::com::iid& iid) noexcept { \
		return s_internal_cast(this, entries(), iid); \
	} \
	::cobalt::com::unknown* get_unknown() noexcept { \
		BOOST_ASSERT(entries()->func == SIMPLE_CAST_ENTRY); \
		return reinterpret_cast<::cobalt::com::unknown*>(reinterpret_cast<size_t>(this) + entries()->data); \
	} \
	virtual ::cobalt::com::unknown* outer_object() noexcept { \
		return get_unknown(); \
	}

template <typename T>
class stack_object : public T {
public:
	stack_object() { BOOST_VERIFY(this->initialize()); }
	~stack_object() { this->uninitialize(); BOOST_ASSERT(this->_ref_count == 0); }
	virtual size_t retain() noexcept override { return this->internal_retain(); }
	virtual size_t release() noexcept override { return this->internal_release(); }
	virtual unknown* cast(const iid& iid) noexcept override { return this->internal_cast(iid); }
};

template <typename T>
class object : public T {
public:
	explicit object(unknown* outer) noexcept {}
	~object() { this->uninitialize(); }
	
	virtual size_t retain() noexcept override { return this->internal_retain(); }
	virtual size_t release() noexcept override {
		auto ref_count = this->internal_release();
		if (!ref_count)
			delete this;
		return ref_count;
	}
	virtual unknown* cast(const iid& iid) noexcept override { return this->internal_cast(iid); }
	
	static object<T>* create_instance(unknown* outer = nullptr) noexcept {
		BOOST_ASSERT(outer == nullptr);
		if (outer)
			return nullptr;
		auto obj = new(std::nothrow) object<T>(nullptr);
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

template <typename T>
class contained_object : public T {
public:
	contained_object(unknown* outer) noexcept { BOOST_ASSERT(!this->_outer); this->_outer = outer; }
	virtual size_t retain() noexcept override { return this->outer_retain(); }
	virtual size_t release() noexcept override { return this->outer_release(); }
	virtual unknown* cast(const iid& iid) noexcept override { return this->outer_cast(iid); }
	virtual unknown* outer_object() noexcept override { return this->_outer; }
};

template <class T>
class aggregatable_object : public unknown, public object_base {
public:
	explicit aggregatable_object(unknown* outer) noexcept : _contained(outer ? outer : this) {}
	~aggregatable_object() {
		this->uninitialize();
	}
	
	bool initialize() noexcept {
		object_base::initialize();
		return _contained.initialize();
	}
	
	void uninitialize() noexcept {
		object_base::uninitialize();
		_contained.uninitialize();
	}
	
	virtual size_t retain() noexcept override { return this->internal_retain(); }
	virtual size_t release() noexcept override {
		auto ref_count = this->internal_release();
		if (!ref_count)
			delete this;
		return ref_count;
	}
	virtual unknown* cast(const iid& iid) noexcept override {
		if (iid == IIDOF(unknown))
			return this;
		return _contained.internal_cast(iid);
	}
	
	static aggregatable_object<T>* create_instance(unknown* outer) noexcept {
		auto obj = new(std::nothrow) aggregatable_object<T>(outer);
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
	contained_object<T> _contained;
};

template <typename T>
class tear_off_object : public T {
public:
	explicit tear_off_object(unknown* outer) noexcept {
		BOOST_ASSERT(!this->_owner);
		BOOST_ASSERT(!!outer);
		this->owner(reinterpret_cast<typename T::owner_type*>(outer));
		this->_owner->retain();
	}
	~tear_off_object() { this->uninitialize(); this->_owner->release(); }
	
	virtual size_t retain() noexcept override { return this->internal_retain(); }
	virtual size_t release() noexcept override {
		auto ref_count = this->internal_release();
		if (!ref_count)
			delete this;
		return ref_count;
	}
	virtual unknown* cast(const iid& iid) noexcept override {
		return this->_owner->cast(iid);
	}
};

template <class T>
class cached_tear_off_object : public unknown, public object_base {
public:
	explicit cached_tear_off_object(unknown* outer) noexcept
		: _contained(outer ? outer : this)
	{
		BOOST_ASSERT(!this->_owner);
		BOOST_ASSERT(!!outer);
		this->owner(reinterpret_cast<typename T::owner_type*>(outer));
		this->_owner->retain();
	}
	~cached_tear_off_object() { this->uninitialize(); }
	
	bool initialize() noexcept {
		object_base::initialize();
		return _contained.initialize();
	}
	
	void uninitialize() noexcept {
		object_base::uninitialize();
		_contained.uninitialize();
	}
	
	virtual size_t retain() noexcept override { return this->internal_retain(); }
	virtual size_t release() noexcept override {
		auto ref_count = this->internal_release();
		if (!ref_count)
			delete this;
		return ref_count;
	}
	virtual unknown* cast(const iid& iid) noexcept override {
		if (iid == IIDOF(unknown))
			return this;
		return _contained.internal_cast(iid);
	}

private:
	contained_object<T> _contained;
};

template <typename T>
class creator {
public:
	static unknown* create_instance(unknown* outer, const iid& iid) noexcept {
		auto p = new(std::nothrow) T(outer);
		if (!p) return nullptr;
		p->set_void(outer);
		if (!p->initialize()) {
			delete p;
			return nullptr;
		}
		unknown* unk = p->cast(iid);
		if (!unk)
			delete p;
		return unk;
	}
};

template <typename T>
class internal_creator {
public:
	static unknown* create_instance(unknown* outer, const iid& iid) noexcept {
		auto p = new(std::nothrow) T(outer);
		if (!p) return nullptr;
		p->set_void(outer);
		if (!p->initialize()) {
			delete p;
			return nullptr;
		}
		unknown* unk = p->internal_cast(iid);
		if (!unk)
			delete p;
		return unk;
	}
};

template <typename T1, typename T2>
class creator2 {
public:
	static unknown* create_instance(unknown* outer, const iid& iid) noexcept {
		return !outer ? T1::create_instance(nullptr, iid) :
		                T2::create_instance(outer, iid);
	}
};

class fail_creator {
public:
	static unknown* create_instance(unknown* outer, const iid& iid) noexcept {
		return nullptr;
	}
};

#define DECLARE_AGGREGATABLE(x) public: \
	using creator_type = ::cobalt::com::creator2< \
	                         ::cobalt::com::creator<::cobalt::com::object<x>>, \
	                         ::cobalt::com::creator<::cobalt::com::aggregatable_object<x>>>;
	
#define DECLARE_NOT_AGGREGATABLE(x) public: \
	using creator_type = ::cobalt::com::creator2< \
	                         ::cobalt::com::creator<::cobalt::com::object<x>>, \
	                         ::cobalt::com::fail_creator>;
	
#define DECLARE_ONLY_AGGREGATABLE(x) public: \
	using creator_type = ::cobalt::com::creator2< \
	                         ::cobalt::com::fail_creator, \
	                         ::cobalt::com::creator<::cobalt::com::aggregatable_object<x>>>;

#define DECLARE_POLY_AGGREGATABLE(x) public: \
	using creator_type = ::cobalt::com::creator<::cobalt::com::aggregatable_object<x>>;

class class_factory_impl : public object_base, public class_factory {
public:
	explicit class_factory_impl(unknown* outer = nullptr) noexcept {}
	
	BEGIN_CAST_MAP(class_factory_impl)
		CAST_ENTRY(class_factory)
	END_CAST_MAP()
	
	void set_void(void* p) noexcept { _creator_fn = reinterpret_cast<creator_fn>(p); }
	
	virtual unknown* create_instance(unknown* outer, const iid& iid) noexcept override {
		BOOST_ASSERT(_creator_fn);
		BOOST_ASSERT(!outer || (outer && &iid == &IIDOF(unknown)));
		if (outer && &iid != &IIDOF(unknown)) return nullptr;
		return _creator_fn(outer, iid);
	}
	
private:
	creator_fn _creator_fn;
};

class class_factory_singleton_impl : public object_base, public class_factory {
public:
	explicit class_factory_singleton_impl(unknown* outer = nullptr) noexcept {}
	
	BEGIN_CAST_MAP(class_factory_singleton_impl)
		CAST_ENTRY(class_factory)
	END_CAST_MAP()
	
	void set_void(void* p) noexcept { _creator_fn = reinterpret_cast<creator_fn>(p); }
	
	virtual unknown* create_instance(unknown* outer, const iid& iid) noexcept override {
		BOOST_ASSERT(!outer);
		if (outer) return nullptr;
		if (!_object) {
			BOOST_ASSERT(_creator_fn);
			_object = _creator_fn(outer, iid);
			BOOST_ASSERT(_object);
		}
		return _object->cast(iid);
	}
	
private:
	creator_fn _creator_fn;
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
	
	static const clsid& class_id() noexcept { return IIDOF(T); }
	static void object_main(bool starting) noexcept {}
	
	template <typename Q>
	static ref_ptr<Q> create_instance(unknown* outer = nullptr) noexcept {
		return static_cast<Q*>(T::creator_type::create_instance(outer, IIDOF(Q)));
	}
};

#define BEGIN_OBJECT_MAP(x) public: \
	static const object_entry* entries() noexcept { \
		using base_class = x; \
		static const object_entry entries[] = {
	
#define OBJECT_ENTRY(class) {\
			&class::class_id(), \
			&class::class_factory_creator_type::create_instance, \
			&class::creator_type::create_instance, \
			nullptr, \
			&class::object_main },
	
#define OBJECT_ENTRY_NON_CREATEABLE(class) \
			{ &class::class_id(), nullptr, nullptr, nullptr, &class::object_init },

#define END_OBJECT_MAP() \
			{ nullptr, nullptr, nullptr, nullptr, nullptr } \
		}; \
		return entries; \
	}
	
class module_base {
protected:
	struct object_entry {
		const clsid* clsid;
		creator_fn get_class_object;
		creator_fn create_instance;
		mutable unknown* factory;
		void (*object_main)(bool);
	};
	
	static void s_object_main(const object_entry* entries, bool starting) noexcept {
		for (auto entry = entries; entry->clsid; ++entry) {
			entry->object_main(starting);
			// Release factory
			if (!starting && entry->factory) {
				entry->factory->release();
				entry->factory = nullptr;
			}
		}
	}
	
	static unknown* s_get_class_object(const object_entry* entries, const clsid& clsid) noexcept {
		for (auto entry = entries; entry->clsid; ++entry) {
			if (entry->get_class_object && entry->clsid == &clsid) {
				if (!entry->factory) {
					entry->factory = entry->get_class_object(
						reinterpret_cast<unknown*>(entry->create_instance), IIDOF(unknown));
					BOOST_ASSERT(!!entry->factory);
					entry->factory->retain();
				}
				return entry->factory;
			}
		}
		
		return nullptr;
	}
};

template <typename T>
class module : public module_base {
public:
	module() noexcept {
		s_object_main(static_cast<T*>(this)->entries(), true);
	}
	
	virtual ~module() {
		s_object_main(static_cast<T*>(this)->entries(), false);
	}
	
	unknown* get_class_object(const clsid& clsid) noexcept {
		return s_get_class_object(static_cast<T*>(this)->entries(), clsid);
	}
	
	unknown* create_instance(unknown* outer, const clsid& clsid, const iid& iid) noexcept {
		if (auto unk = get_class_object(clsid)) {
			if (auto cf = unk->template cast<class_factory>())
				return cf->create_instance(outer, iid);
		}
		return nullptr;
	}
	
	template <typename Q>
	ref_ptr<Q> create_instance(unknown* outer, const clsid& clsid) noexcept {
		return static_cast<Q*>(create_instance(outer, clsid, IIDOF(Q)));
	}
	
	template <typename Q>
	ref_ptr<Q> create_instance(const clsid& clsid) noexcept {
		return static_cast<Q*>(create_instance(nullptr, clsid, IIDOF(Q)));
	}
};

} // namespace com
} // namespace cobalt

#endif // COBALT_COM_HPP_INCLUDED
