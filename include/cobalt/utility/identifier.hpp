#ifndef COBALT_UTILITY_IDENTIFIER_HPP_INCLUDED
#define COBALT_UTILITY_IDENTIFIER_HPP_INCLUDED

#pragma once

#include <boost/flyweight.hpp>

#include <string>

namespace cobalt {

template <typename Tag>
using basic_identifier = boost::flyweight<
	std::string,
	boost::flyweights::tag<Tag>,
	boost::flyweights::hashed_factory<
		std::hash<std::string>,
		std::equal_to<std::string>,
		std::allocator<boost::mpl::_1>
	>,
	boost::flyweights::tracking<boost::flyweights::refcounted>
>;

struct generic_identifier_tag {};

using identifier = basic_identifier<generic_identifier_tag>;

} // namespace cobalt

#endif // COBALT_UTILITY_IDENTIFIER_HPP_INCLUDED
