#ifndef COBALT_UTILITY_IDENTIFIER_HPP_INCLUDED
#define COBALT_UTILITY_IDENTIFIER_HPP_INCLUDED

#pragma once

#include <cobalt/utility/hash.hpp>

#include <boost/flyweight.hpp>

#include <string>

namespace cobalt {

struct identifier_tag {};

struct identifier_hash {
	std::size_t operator()(const std::string& str) const noexcept {
		return static_cast<std::size_t>(murmur3(str.c_str(), str.size(), 0));
	}
};

using identifier = boost::flyweight<
	std::string,
	boost::flyweights::tag<identifier_tag>,
	boost::flyweights::hashed_factory<
		identifier_hash,
		std::equal_to<std::string>,
		std::allocator<boost::mpl::_1>
	>
>;

} // namespace cobalt

#endif // COBALT_UTILITY_IDENTIFIER_HPP_INCLUDED
