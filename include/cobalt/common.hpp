#pragma once

#include "cool/platform.hpp"

#define BOOST_ENABLE_ASSERT_HANDLER
#include <boost/assert.hpp>

#include <boost/smart_ptr/intrusive_ref_counter.hpp>
#include <boost/smart_ptr/intrusive_ptr.hpp>
#include <boost/thread/thread.hpp>

template <typename T>
using ref_counter = boost::intrusive_ref_counter<T, boost::thread_unsafe_counter>;

template <typename T>
using thread_safe_ref_counter = boost::intrusive_ref_counter<T, boost::thread_safe_counter>;

bool almost_equal_floats(float A, float B, int maxUlps = 16);

#define DISALLOW_COPY_AND_ASSIGN(Class) \
	BOOST_DELETED_FUNCTION(Class(const Class&)) \
	BOOST_DELETED_FUNCTION(Class& operator=(const Class&))

#if defined(TARGET_PLATFORM_WINDOWS)

#pragma warning(disable:4127) // warning C4127: conditional expression is constant
#pragma warning(disable:4201) // warning C4201: nonstandard extension used : nameless struct/union

#endif
