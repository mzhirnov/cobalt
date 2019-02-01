#ifndef COBALT_COM_BASE_HPP_INCLUDED
#define COBALT_COM_BASE_HPP_INCLUDED

#pragma once

// Classes in this file:
//     object_base
//     tear_off_object_base
//     stack_object
//     object
//     contained_object
//     aggregated_object
//     maybe_aggregated_object
//     tear_off_object
//     cached_tear_off_object
//     coclass
//     module_base
//     module

#include <cobalt/com/core.hpp>
#include <cobalt/com/utility.hpp>

#include <boost/assert.hpp>

#include <mutex>

namespace cobalt {
namespace com {
namespace detail {

template <typename Base, typename Derived, typename = std::enable_if_t<std::is_base_of_v<Base, Derived>>>
constexpr size_t offset_from_class() noexcept {
	constexpr Derived* const derived = nullptr;
	return
		static_cast<const char*>(static_cast<const void*>(derived)) -
		static_cast<const char*>(static_cast<const void*>(static_cast<Base*>(derived)));
}

template <typename Base, typename Base2, typename Derived, typename = std::enable_if_t<std::is_base_of_v<Base, Base2> && std::is_base_of_v<Base2, Derived>>>
constexpr size_t offset_from_class() noexcept {
	constexpr Derived* const derived = nullptr;
	return
		static_cast<const char*>(static_cast<const void*>(derived)) -
		static_cast<const char*>(static_cast<const void*>(static_cast<Base*>(static_cast<Base2*>(derived))));
}

template <typename Parent, typename Member>
constexpr size_t offset_from_member_ptr(const Member Parent::*member) noexcept {
	constexpr Parent* const parent = nullptr;
	return
		static_cast<const char*>(static_cast<const void*>(&(parent->*member))) -
		static_cast<const char*>(static_cast<const void*>(parent));
}

} // namespace detail

using create_fn = ref<any> (*)(void* pv, const uid& iid, std::error_code& ec);
using cast_fn = ref<any> (*)(void* pv, const uid& iid, size_t data, std::error_code& ec);

const cast_fn kSimpleCastEntry = reinterpret_cast<cast_fn>(1);

struct cast_entry {
	const uid* iid;
	size_t data;
	cast_fn func;
};

struct creator_data {
	create_fn func;
};

template <typename Creator>
struct creator_thunk {
	inline static constexpr creator_data data{Creator::create_instance};
};

struct cache_data {
	size_t offset;
	create_fn func;
};

template <typename Creator, size_t Offset>
struct cache_thunk {
	inline static constexpr cache_data data{Offset, Creator::create_instance};
};

struct chain_data {
	size_t offset;
	const cast_entry* (*get_cast_entries)() noexcept;
};

template <typename Base, typename Derived>
struct chain_thunk {
	inline static const chain_data data{detail::offset_from_class<Base, Derived>(), Base::get_cast_entries};
};

/// Base class for regular objects.
class object_base {
public:
	object_base() noexcept = default;
	
	static void class_initialize() noexcept {}
	static void class_shutdown() noexcept {}
	
	void initialize(std::error_code& ec) noexcept { ec = make_error_code(errc::success); }
	void shutdown() noexcept {}
	
protected:
	void internal_initialize_retain() noexcept {}
	void internal_initialize_release() noexcept { BOOST_ASSERT(_ref_count == 0); }
	
private:
	template <typename T> friend struct creator;
	template <typename T> friend struct internal_creator;
	
	void set_param(void* pv) noexcept {}

protected:
	uint32_t internal_retain() noexcept { BOOST_ASSERT(_ref_count != -1); return ++_ref_count; }
	uint32_t internal_release() noexcept { BOOST_ASSERT(_ref_count != 0); return --_ref_count; }
	
	static ref<any> s_internal_cast(void* pv, const cast_entry* entries, const uid& iid, std::error_code& ec) noexcept {
		BOOST_ASSERT(pv);
		BOOST_ASSERT(entries);
		if (!pv || !entries) {
			ec = make_error_code(std::errc::invalid_argument);
			return nullptr;
		}
		
		BOOST_ASSERT(entries[0].func == kSimpleCastEntry);
		if (entries[0].func != kSimpleCastEntry) {
			ec = make_error_code(errc::failure);
			return nullptr;
		}
		
		if (iid == UIDOF(any)) {
			ec = make_error_code(errc::success);
			return reinterpret_cast<any*>(reinterpret_cast<size_t>(pv) + entries[0].data);
		}
		
		for (auto entry = entries; entry->func; ++entry) {
			bool blind = !entry->iid;
			if (blind || *entry->iid == iid) {
				if (entry->func == kSimpleCastEntry) {
					BOOST_ASSERT(!blind);
					ec = make_error_code(errc::success);
					return reinterpret_cast<any*>(reinterpret_cast<size_t>(pv) + entry->data);
				} else {
					auto id = entry->func(pv, iid, entry->data, ec);
					if (ec) return nullptr;
					if (id || (!blind && !id)) {
						ec = make_error_code(errc::success);
						return id;
					}
				}
			}
		}
		
		ec = make_error_code(errc::no_such_interface);
		return nullptr;
	}
	
	static ref<any> s_break(void* pv, const uid& iid, size_t data, std::error_code& ec) noexcept {
		(void)pv;
		(void)iid;
		(void)data;
		// TODO: Break into debugger
		BOOST_ASSERT(false);
		ec = make_error_code(std::errc::operation_canceled);
		return nullptr;
	}
	
	static ref<any> s_nointerface(void* pv, const uid& iid, size_t data, std::error_code& ec) noexcept {
		ec = make_error_code(errc::no_such_interface);
		return nullptr;
	}
	
	static ref<any> s_create(void* pv, const uid& iid, size_t data, std::error_code& ec) noexcept {
		auto cd = reinterpret_cast<creator_data*>(data);
		return cd->func(reinterpret_cast<any*>(pv), iid, ec);
	}
	
	static ref<any> s_cache(void* pv, const uid& iid, size_t data, std::error_code& ec) noexcept {
		auto cd = reinterpret_cast<cache_data*>(data);
		auto id = reinterpret_cast<any**>((reinterpret_cast<size_t>(pv) + cd->offset));
		if (!*id) {
			if (auto rid = cd->func(reinterpret_cast<any*>(pv), UIDOF(any), ec)) {
				if (!ec) {
					*id = rid.detach();
				}
			}
		}
		return *id ? (*id)->cast(iid, ec) : nullptr;
	}
	
	static ref<any> s_delegate(void* pv, const uid& iid, size_t data, std::error_code& ec) noexcept {
		if (auto id = *reinterpret_cast<any**>(reinterpret_cast<size_t>(pv) + data))
			return id->cast(iid, ec);
		ec = make_error_code(errc::failure);
		return nullptr;
	}
	
	static ref<any> s_chain(void* pv, const uid& iid, size_t data, std::error_code& ec) noexcept {
		auto cd = reinterpret_cast<chain_data*>(data);
		auto p = reinterpret_cast<void*>((reinterpret_cast<size_t>(pv) + cd->offset));
		return s_internal_cast(p, cd->get_cast_entries(), iid, ec);
	}
	
protected:
	union {
		// Object reference counter if regular object
		int_fast32_t _ref_count;
		// Outer owning object if aggregated object
		any* _outer = nullptr;
	};
};

/// Base class for tear-off implementations.
template <typename Owner>
class tear_off_object_base : public object_base {
public:
	using owner_type = Owner;
	
	void owner(owner_type* owner) noexcept { _owner = owner; }
	owner_type* owner() noexcept { return _owner; }

private:
	// Owner object which "implements" this object's interfaces
	owner_type* _owner = nullptr;
};

#define DECLARE_PROTECT_INITIALIZE() \
public: \
	void internal_initialize_retain() noexcept { this->internal_retain(); } \
	void internal_initialize_release() noexcept { this->internal_release(); }
	
#define DECLARE_GET_OUTER_OBJECT() \
public: \
	virtual ::cobalt::com::any* get_outer_object() const noexcept { \
		return identity(); \
	}

#define BEGIN_CAST_MAP(x) \
public: \
	::cobalt::com::any* identity() const noexcept { \
		BOOST_ASSERT(get_cast_entries()->func == ::cobalt::com::kSimpleCastEntry); \
		return reinterpret_cast<::cobalt::com::any*>(reinterpret_cast<size_t>(this) + get_cast_entries()->data); \
	} \
	::cobalt::com::ref<::cobalt::com::any> internal_cast(const ::cobalt::uid& iid, std::error_code& ec) noexcept { \
		return s_internal_cast(this, get_cast_entries(), iid, ec); \
	} \
	static const ::cobalt::com::cast_entry* get_cast_entries() noexcept { \
		using namespace ::cobalt::com; \
		using self = x; \
		static const cast_entry entries[] = {

#define CAST_ENTRY_BREAK(x) \
			{ &UIDOF(x), 0, s_break },
	
#define CAST_ENTRY_NOINTERFACE(x) \
			{ &UIDOF(x), 0, s_nointerface },
	
#define CAST_ENTRY_FUNC(iid, data, func) \
			{ &iid, data, func },

#define CAST_ENTRY_FUNC_BLIND(data, func) \
			{ nullptr, data, func }

#define CAST_ENTRY(x) \
			{ &UIDOF(x), detail::offset_from_class<x, self>(), kSimpleCastEntry },
	
#define CAST_ENTRY_IID(iid, x) \
			{ &iid, detail::offset_from_class<x, self>(), kSimpleCastEntry },

#define CAST_ENTRY2(x, x2) \
			{ &UIDOF(x), detail::offset_from_class<x, x2, self>(), kSimpleCastEntry },

#define CAST_ENTRY2_IID(iid, x, x2) \
			{ &iid, detail::offset_from_class<x, x2, self>(), kSimpleCastEntry },
	
#define CAST_ENTRY_TEAR_OFF(iid, x) \
			{ &iid, reinterpret_cast<size_t>(&creator_thunk<internal_creator<tear_off_object<x>>>::data), s_create },
	
#define CAST_ENTRY_CACHED_TEAR_OFF(iid, x, pid) \
			{ &iid, reinterpret_cast<size_t>(&cache_thunk<creator<cached_tear_off_object<x>>, offsetof(self, pid)>::data), s_cache },
	
#define CAST_ENTRY_AGGREGATE(iid, pid) \
			{ &iid, detail::offset_from_member_ptr<self>(&self::pid), s_delegate },
	
#define CAST_ENTRY_AGGREGATE_BLIND(pid) \
			{ nullptr, detail::offset_from_member_ptr<self>(&self::pid), s_delegate },
	
#define CAST_ENTRY_AUTOAGGREGATE(iid, pid, clsid) \
			{ &iid, reinterpret_cast<size_t>(&cache_thunk<aggregate_creator<self, clsid>, offsetof(self, pid)>::data), s_cache },
	
#define CAST_ENTRY_AUTOAGGREGATE_BLIND(pid, clsid) \
			{ nullptr, reinterpret_cast<size_t>(&cache_thunk<aggregate_creator<self, clsid>, offsetof(self, pid)>::data), s_cache },
	
#define CAST_ENTRY_AUTOAGGREGATE_CLASS(iid, pid, x) \
			{ &iid, reinterpret_cast<size_t>(&cache_thunk<creator<aggregated_object<x>>, offsetof(self, pid)>::data), s_cache },
	
#define CAST_ENTRY_AUTOAGGREGATE_CLASS_BLIND(pid, x) \
			{ nullptr, reinterpret_cast<size_t>(&cache_thunk<creator<aggregated_object<x>>, offsetof(self, pid)>::data), s_cache },
	
#define CAST_ENTRY_CHAIN(class) \
			{ nullptr, reinterpret_cast<size_t>(&chain_thunk<class, self>::data), s_chain },

#define END_CAST_MAP() \
			{ nullptr, 0, nullptr } \
		}; \
		return entries; \
	} \
	virtual uint32_t retain() noexcept override = 0; \
	virtual uint32_t release() noexcept override = 0; \
	virtual ::cobalt::com::ref<::cobalt::com::any> cast(const uid& iid, std::error_code& ec) noexcept override = 0;

/// Object instance on stack.
template <typename Base>
class stack_object : public Base {
public:
	stack_object() noexcept {
		this->initialize(_ec);
		BOOST_ASSERT(!_ec);
	}
	~stack_object() {
		this->shutdown();
		BOOST_ASSERT(this->_ref_count == 0);
	}
	
	const std::error_code& error() const noexcept { return _ec; }
	
	virtual uint32_t retain() noexcept override {
#ifdef BOOST_ASSERT_IS_VOID
		return 0;
#else
		return this->internal_retain();
#endif
	}
	virtual uint32_t release() noexcept override {
#ifdef BOOST_ASSERT_IS_VOID
		return 0;
#else
		return this->internal_release();
#endif
	}
	
	virtual ref<any> cast(const uid& iid, std::error_code& ec) noexcept override
		{ return this->internal_cast(iid, ec); }
	
private:
	std::error_code _ec;
};

/// Object instance on heap.
template <typename Base>
class object : public Base {
public:
	explicit object(void* pv) noexcept {}
	~object() { this->shutdown(); }
	
	virtual uint32_t retain() noexcept override { return this->internal_retain(); }
	virtual uint32_t release() noexcept override {
		auto ref_count = this->internal_release();
		if (!ref_count)
			delete this;
		return ref_count;
	}
	virtual ref<any> cast(const uid& iid, std::error_code& ec) noexcept override
		{ return this->internal_cast(iid, ec); }
	
	static object* create_instance(any* outer, std::error_code& ec) noexcept {
		BOOST_ASSERT(!outer);
		if (outer) {
			ec = make_error_code(errc::aggregation_not_supported);
			return nullptr;
		}
		auto p = new(std::nothrow) object(nullptr);
		if (!p) {
			ec = make_error_code(std::errc::not_enough_memory);
			return nullptr;
		}
		p->internal_initialize_retain();
		p->initialize(ec);
		p->internal_initialize_release();
		if (ec) {
			delete p;
			return nullptr;
		}
		ec = make_error_code(errc::success);
		return p;
	}
};

/// Object instance as part of aggregated object.
template <typename Base>
class contained_object : public Base {
public:
	explicit contained_object(void* pv) noexcept {
		BOOST_ASSERT(!this->_outer);
		BOOST_ASSERT(pv);
		this->_outer = reinterpret_cast<any*>(pv);
	}
	virtual uint32_t retain() noexcept override { return static_cast<_any*>(this->_outer)->retain(); }
	virtual uint32_t release() noexcept override { return static_cast<_any*>(this->_outer)->release(); }
	virtual ref<any> cast(const uid& iid, std::error_code& ec) noexcept override { return this->_outer->cast(iid, ec); }
	virtual any* get_outer_object() const noexcept override { return this->_outer; }

private:
	struct _any : any {
		// Bring retain/release to public
		using any::retain;
		using any::release;
	};
};

/// Object instance on heap aggregated inside another object.
template <typename Contained>
class aggregated_object : public any, public object_base {
public:
	explicit aggregated_object(void* pv) noexcept : _contained(pv) {}
	aggregated_object() { this->shutdown(); }
	
	void initialize(std::error_code& ec) noexcept { object_base::initialize(ec); if (!ec) _contained.initialize(ec); }
	void shutdown() noexcept { object_base::shutdown(); _contained.shutdown(); }
	
	virtual uint32_t retain() noexcept override { return this->internal_retain(); }
	virtual uint32_t release() noexcept override {
		auto ref_count = this->internal_release();
		if (!ref_count)
			delete this;
		return ref_count;
	}
	virtual ref<any> cast(const uid& iid, std::error_code& ec) noexcept override {
		if (iid == UIDOF(any)) {
			ec = make_error_code(errc::success);
			return this;
		}
		return _contained.internal_cast(iid, ec);
	}
	
	static aggregated_object* create_instance(any* outer, std::error_code& ec) noexcept {
		BOOST_ASSERT(outer);
		if (!outer) {
			ec = make_error_code(errc::failure);
			return nullptr;
		}
		auto p = new(std::nothrow) aggregated_object(outer);
		if (!p) {
			ec = make_error_code(std::errc::not_enough_memory);
			return nullptr;
		}
		p->internal_initialize_retain();
		p->initialize(ec);
		p->internal_initialize_release();
		if (ec) {
			delete p;
			ec = make_error_code(errc::failure);
			return nullptr;
		}
		ec = make_error_code(errc::success);
		return p;
	}

private:
	contained_object<Contained> _contained;
};

/// Object instance on heap, either aggregated or not.
template <typename Contained>
class maybe_aggregated_object : public any, public object_base {
public:
	explicit maybe_aggregated_object(void* pv) noexcept : _contained(pv ? pv : this) {}
	maybe_aggregated_object() { this->shutdown(); }
	
	void initialize(std::error_code& ec) noexcept { object_base::initialize(ec); if (!ec) _contained.initialize(ec); }
	void shutdown() noexcept { object_base::shutdown(); _contained.shutdown(); }
	
	virtual uint32_t retain() noexcept override { return this->internal_retain(); }
	virtual uint32_t release() noexcept override {
		auto ref_count = this->internal_release();
		if (!ref_count)
			delete this;
		return ref_count;
	}
	virtual ref<any> cast(const uid& iid, std::error_code& ec) noexcept override {
		if (iid == UIDOF(any)) {
			ec = make_error_code(errc::success);
			return this;
		}
		return _contained.internal_cast(iid, ec);
	}
	
	static maybe_aggregated_object* create_instance(any* outer, std::error_code& ec) noexcept {
		auto p = new(std::nothrow) maybe_aggregated_object(outer);
		if (!p) {
			ec = make_error_code(std::errc::not_enough_memory);
			return nullptr;
		}
		p->internal_initialize_retain();
		p->initialize(ec);
		p->internal_initialize_release();
		if (ec) {
			delete p;
			ec = make_error_code(errc::failure);
			return nullptr;
		}
		ec = make_error_code(errc::success);
		return p;
	}

private:
	contained_object<Contained> _contained;
};

/// Object instance on heap as of tear-off implementation.
template <typename Base>
class tear_off_object : public Base {
public:
	explicit tear_off_object(void* pv) noexcept {
		BOOST_ASSERT(!this->owner());
		BOOST_ASSERT(pv);
		this->owner(static_cast<typename Base::owner_type*>(pv));
		this->owner()->retain();
	}
	~tear_off_object() {
		this->shutdown();
		this->owner()->release();
	}
	
	virtual uint32_t retain() noexcept override { return this->internal_retain(); }
	virtual uint32_t release() noexcept override {
		auto ref_count = this->internal_release();
		if (!ref_count)
			delete this;
		return ref_count;
	}
	virtual ref<any> cast(const uid& iid, std::error_code& ec) noexcept override { return this->owner()->cast(iid, ec); }
};

/// Object instance on heap  as of cached tear-off implementation.
template <typename Contained>
class cached_tear_off_object : public any, public object_base {
public:
	explicit cached_tear_off_object(void* pv) noexcept
		: _contained(static_cast<typename Contained::owner_type*>(pv)->get_outer_object())
	{
		BOOST_ASSERT(!_contained.owner());
		BOOST_ASSERT(pv);
		_contained.owner(static_cast<typename Contained::owner_type*>(pv));
	}
	~cached_tear_off_object() { this->shutdown(); }
	
	void initialize(std::error_code& ec) noexcept { object_base::initialize(ec); if (!ec) _contained.initialize(ec); }
	void shutdown() noexcept { object_base::shutdown(); _contained.shutdown(); }
	
	virtual uint32_t retain() noexcept override { return this->internal_retain(); }
	virtual uint32_t release() noexcept override {
		auto ref_count = this->internal_release();
		if (!ref_count)
			delete this;
		return ref_count;
	}
	virtual ref<any> cast(const uid& iid, std::error_code& ec) noexcept override {
		if (iid == UIDOF(any)) {
			ec = make_error_code(errc::success);
			return this;
		}
		return _contained.internal_cast(iid, ec);
	}

private:
	contained_object<Contained> _contained;
};

template <typename T>
struct creator {
	static ref<any> create_instance(void* pv, const uid& iid, std::error_code& ec) noexcept {
		auto p = new(std::nothrow) T(pv);
		if (!p) {
			ec = make_error_code(std::errc::not_enough_memory);
			return nullptr;
		};
		p->set_param(pv);
		p->internal_initialize_retain();
		p->initialize(ec);
		p->internal_initialize_release();
		if (ec) {
			delete p;
			ec = make_error_code(errc::failure);
			return nullptr;
		}
		auto rid = p->cast(iid, ec);
		if (!ec) return rid;
		if (!rid) delete p;
		return nullptr;
	}
};

template <typename T>
struct internal_creator {
	static ref<any> create_instance(void* pv, const uid& iid, std::error_code& ec) noexcept {
		auto p = new(std::nothrow) T(pv);
		if (!p) {
			ec = make_error_code(std::errc::not_enough_memory);
			return nullptr;
		};
		p->set_param(pv);
		p->internal_initialize_retain();
		p->initialize(ec);
		p->internal_initialize_release();
		if (ec) {
			delete p;
			ec = make_error_code(errc::failure);
			return nullptr;
		}
		auto rid = p->internal_cast(iid, ec);
		if (!ec) return rid;
		if (!rid) delete p;
		return nullptr;
	}
};

template <typename T, const uid& clsid>
struct aggregate_creator {
	static ref<any> create_instance(void* pv, const uid& iid, std::error_code& ec) noexcept {
		if (!pv) {
			ec = make_error_code(std::errc::invalid_argument);
			return nullptr;
		}
		return ::cobalt::com::create_instance(static_cast<T*>(pv)->get_outer_object(), clsid, UIDOF(any), ec);
	}
};

template <typename T1, typename T2>
struct creator2 {
	static ref<any> create_instance(void* pv, const uid& iid, std::error_code& ec) noexcept {
		return !pv ? T1::create_instance(pv, iid, ec) :
		             T2::create_instance(pv, iid, ec);
	}
};

struct fail_creator {
	static ref<any> create_instance(void* pv, const uid& iid, std::error_code& ec) noexcept {
		ec = make_error_code(errc::failure);
		return nullptr;
	}
};

#define DECLARE_AGGREGATABLE(x) \
public: \
	using creator_type = ::cobalt::com::creator2< \
	                         ::cobalt::com::creator<::cobalt::com::object<x>>, \
	                         ::cobalt::com::creator<::cobalt::com::aggregated_object<x>>>;
	
#define DECLARE_NOT_AGGREGATABLE(x) \
public: \
	using creator_type = ::cobalt::com::creator2< \
	                         ::cobalt::com::creator<::cobalt::com::object<x>>, \
	                         ::cobalt::com::fail_creator>;
	
#define DECLARE_ONLY_AGGREGATABLE(x) \
public: \
	using creator_type = ::cobalt::com::creator2< \
	                         ::cobalt::com::fail_creator, \
	                         ::cobalt::com::creator<::cobalt::com::aggregated_object<x>>>;

#define DECLARE_MAYBE_AGGREGATABLE(x) \
public: \
	using creator_type = ::cobalt::com::creator<::cobalt::com::maybe_aggregated_object<x>>;

class class_factory_impl : public object_base, public class_factory {
public:
	BEGIN_CAST_MAP(class_factory_impl)
		CAST_ENTRY(class_factory)
	END_CAST_MAP()
	
	void set_param(void* pv) noexcept { _creator = reinterpret_cast<create_fn>(pv); }
	
	virtual ref<any> create_instance(any* outer, const uid& iid, std::error_code& ec) noexcept override {
		BOOST_ASSERT(!outer || iid == UIDOF(any));
		if (outer && iid != UIDOF(any)) {
			ec = make_error_code(errc::aggregation_not_supported);
			return nullptr;
		}
		BOOST_ASSERT(_creator);
		if (!_creator) {
			ec = make_error_code(errc::failure);
			return nullptr;
		}
		return _creator(outer, iid, ec);
	}
	
private:
	create_fn _creator = nullptr;
};

template <typename T>
class class_factory_singleton_impl : public object_base, public class_factory {
public:
	BEGIN_CAST_MAP(class_factory_singleton_impl)
		CAST_ENTRY(class_factory)
	END_CAST_MAP()
	
	virtual ref<any> create_instance(any* outer, const uid& iid, std::error_code& ec) noexcept override {
		BOOST_ASSERT(!outer);
		if (outer) {
			ec = make_error_code(errc::aggregation_not_supported);
			return nullptr;
		}
		if (!_rid && !_ec) {
			std::lock_guard<std::mutex> lock(_mutex);
			if (!_rid) {
				if (auto p = object<T>::create_instance(_ec)) {
					_rid = p->identity();
				}
			}
		}
		if (_ec) {
			ec = _ec;
			return nullptr;
		}
		return _rid->cast(iid, ec);
	}
	
private:
	std::mutex _mutex;
	ref<any> _rid;
	std::error_code _ec;
};

#define DECLARE_CLASSFACTORY(cf) \
	using class_factory_creator_type = ::cobalt::com::creator<::cobalt::com::object<cf>>;

#define DECLARE_DEFAULT_CLASSFACTORY() \
	DECLARE_CLASSFACTORY(::cobalt::com::class_factory_impl)
	
#define DECLARE_SINGLETON_CLASSFACTORY(x) \
	DECLARE_CLASSFACTORY(::cobalt::com::class_factory_singleton_impl<x>>>)

template <typename T, const uid& clsid = uid::null()>
class coclass {
protected:
	DECLARE_AGGREGATABLE(T)
	DECLARE_DEFAULT_CLASSFACTORY()
	
	static const uid& s_class_id() noexcept { return clsid; }
	
	template <typename Q>
	static ref<Q> create_instance(std::error_code& ec) noexcept {
		return boost::static_pointer_cast<Q>(T::creator_type::create_instance(nullptr, UIDOF(Q), ec));
	}
	
	template <typename Q>
	static ref<Q> create_instance(any* outer, std::error_code& ec) noexcept {
		return boost::static_pointer_cast<Q>(T::creator_type::create_instance(outer, UIDOF(Q), ec));
	}
};

struct object_entry {
	const uid* clsid;
	create_fn get_class_object;
	create_fn create_instance;
	mutable class_factory* factory;
	void (*class_initialize)();
	void (*class_shutdown)();
};

#define BEGIN_OBJECT_MAP(x) \
public: \
	static const ::cobalt::com::object_entry* get_entries() noexcept { \
		static const ::cobalt::com::object_entry entries[] = {
	
#define OBJECT_ENTRY(class) { \
			&class::s_class_id(), \
			&class::class_factory_creator_type::create_instance, \
			&class::creator_type::create_instance, \
			nullptr, \
			&class::class_initialize, \
			&class::class_shutdown },
	
#define OBJECT_ENTRY_NON_CREATEABLE(class) { \
			&class::s_class_id(), \
			nullptr, \
			nullptr, \
			nullptr, \
			&class::class_initialize, \
			&class::class_shutdown },

#define END_OBJECT_MAP() \
			{ nullptr, nullptr, nullptr, nullptr, nullptr, nullptr } \
		}; \
		return entries; \
	}

class module_base {
public:
	module_base() noexcept { _next = head(); head() = this; }
	virtual ~module_base() { head() = nullptr; }
	
	virtual ref<class_factory> get_class_object(const uid& clsid, std::error_code& ec) noexcept = 0;
	virtual ref<any> create_instance(any* outer, const uid& clsid, const uid& iid, std::error_code& ec) noexcept = 0;

protected:
	static void s_initialize(const object_entry* entries) noexcept {
		for (auto entry = entries; entry->clsid; ++entry) {
			entry->class_initialize();
		}
	}
	
	static void s_shutdown(const object_entry* entries) noexcept {
		for (auto entry = entries; entry->clsid; ++entry) {
			entry->class_shutdown();
			// Release factory
			if (entry->factory) {
				ref<any> adopt(entry->factory, false);
				entry->factory = nullptr;
			}
		}
	}
	
	static class_factory* s_get_class_object(std::mutex& mutex, const object_entry* entries, const uid& clsid, std::error_code& ec) noexcept {
		for (auto entry = entries; entry->clsid; ++entry) {
			if (entry->get_class_object && *entry->clsid == clsid) {
				if (!entry->factory) {
					std::lock_guard<std::mutex> lock(mutex);
				 	if (!entry->factory) {
				 		auto pv = reinterpret_cast<any*>(entry->create_instance);
				 		if (auto rcf = boost::static_pointer_cast<class_factory>(entry->get_class_object(pv, UIDOF(any), ec))) {
				 			entry->factory = rcf.detach();
						}
					}
				}
				BOOST_ASSERT(entry->factory);
				ec = make_error_code(entry->factory ? errc::success : errc::failure);
				return entry->factory;
			}
		}
		
		ec = make_error_code(errc::no_such_class);
		return nullptr;
	}
	
private:
	static module_base*& head() noexcept {
		static module_base* head = nullptr;
		return head;
	}
	
	module_base* _next = nullptr;
	
	// Global functions
	
	friend ref<class_factory> get_class_object(const uid& clsid, std::error_code& ec) noexcept {
		for (auto&& module = head(); module; module = module->_next) {
			if (auto p = module->get_class_object(clsid, ec))
				return p;
		}
		ec = make_error_code(errc::no_such_class);
		return nullptr;
	}

	friend ref<any> create_instance(any* outer, const uid& clsid, const uid& iid, std::error_code& ec) noexcept {
		for (auto&& module = head(); module; module = module->_next) {
			if (auto p = module->create_instance(outer, clsid, iid, ec))
				return p;
		}
		ec = make_error_code(errc::no_such_class);
		return nullptr;
	}
};

template <typename T>
class module : public module_base {
public:
	module() noexcept {
		s_initialize(derived().get_entries());
	}
	
	~module() {
		s_shutdown(derived().get_entries());
	}

	virtual ref<class_factory> get_class_object(const uid& clsid, std::error_code& ec) noexcept override {
		return s_get_class_object(_mutex, derived().get_entries(), clsid, ec);
	}
	
	virtual ref<any> create_instance(any* outer, const uid& clsid, const uid& iid, std::error_code& ec) noexcept override {
		if (auto cf = get_class_object(clsid, ec))
			return cf->create_instance(outer, iid, ec);
		ec = make_error_code(errc::no_such_class);
		return nullptr;
	}
	
	template <typename Q>
	ref<Q> create_instance(any* outer, const uid& clsid, std::error_code& ec) noexcept {
		return boost::static_pointer_cast<Q>(create_instance(outer, clsid, UIDOF(Q), ec));
	}
	
	template <typename Q>
	ref<Q> create_instance(const uid& clsid, std::error_code& ec) noexcept {
		return boost::static_pointer_cast<Q>(create_instance(nullptr, clsid, UIDOF(Q), ec));
	}
	
private:
	T& derived() noexcept { return static_cast<T&>(*this); }
	
private:
	std::mutex _mutex;
};

} // namespace com
} // namespace cobalt

#endif // COBALT_COM_BASE_HPP_INCLUDED
