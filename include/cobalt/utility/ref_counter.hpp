#ifndef COBALT_UTILITY_REF_COUNTER_HPP_INCLUDED
#define COBALT_UTILITY_REF_COUNTER_HPP_INCLUDED

#pragma once

#include <boost/smart_ptr/intrusive_ref_counter.hpp>
#include <boost/smart_ptr/intrusive_ptr.hpp>

namespace cobalt {

template <typename T>
using ref_counter = boost::intrusive_ref_counter<T>;
	
} // namespace cobalt

#endif // COBALT_UTILITY_REF_COUNTER_HPP_INCLUDED
