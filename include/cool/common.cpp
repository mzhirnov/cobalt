#include "pch.h"
#include "cool/common.hpp"

#ifdef BOOST_ENABLE_ASSERT_HANDLER

#if defined(TARGET_PLATFORM_ANDROID)

#include <android/log.h>

namespace boost
{
	void assertion_failed(char const * expr, char const * function, char const * file, long line) {
		__android_log_assert(expr, "cool", "%s(%ld)", file, line);
	}

	void assertion_failed_msg(char const * expr, char const * msg, char const * function, char const * file, long line) {
		__android_log_assert(expr, "cool", "%s(%ld): %s", file, line, msg);
	}
} // namespace boost

#elif defined(TARGET_PLATFORM_WINDOWS)

namespace boost
{
	void assertion_failed(char const * expr, char const * function, char const * file, long line) {
		expr;
		function;
		file;
		line;

		_CrtDbgBreak();
	}

	void assertion_failed_msg(char const * expr, char const * msg, char const * function, char const * file, long line) {
		expr;
		msg;
		function;
		file;
		line;

		_CrtDbgBreak();
	}
} // namespace boost

#endif // TARGET_PLATFORM_*

#endif // BOOST_ENABLE_ASSERT_HANDLER

// http://www.cygnus-software.com/papers/comparingfloats/comparingfloats.htm
bool almost_equal_floats(float A, float B, int maxUlps) {
	// Make sure maxUlps is non-negative and small enough that the
	// default NAN won't compare as equal to anything.
	BOOST_ASSERT(maxUlps > 0 && maxUlps < 4 * 1024 * 1024);
	int aInt = *(int*)&A;
	// Make aInt lexicographically ordered as a twos-complement int
	if (aInt < 0) {
		aInt = 0x80000000 - aInt;
	}
	// Make bInt lexicographically ordered as a twos-complement int
	int bInt = *(int*)&B;
	if (bInt < 0) {
		bInt = 0x80000000 - bInt;
	}
	int intDiff = abs(aInt - bInt);
	if (intDiff <= maxUlps) {
		return true;
	}
	return false;
}
