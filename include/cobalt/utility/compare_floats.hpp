#ifndef COBALT_UTILITY_COMPARE_FLOATS_HPP_INCLUDED
#define COBALT_UTILITY_COMPARE_FLOATS_HPP_INCLUDED

#pragma once

namespace cobalt {
	
// http://www.cygnus-software.com/papers/comparingfloats/comparingfloats.htm
inline bool almost_equal_floats(float A, float B, int maxUlps) {
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
	
} // namespace cobalt

#endif // COBALT_UTILITY_COMPARE_FLOATS_HPP_INCLUDED
