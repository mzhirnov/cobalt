#ifndef COBALT_UTILITY_IDENTIFIER_HPP_INCLUDED
#define COBALT_UTILITY_IDENTIFIER_HPP_INCLUDED

#pragma once

#include <boost/flyweight.hpp>

#include <string>

namespace cobalt {

using identifier = boost::flyweight<std::string>;

} // namespace cobalt

#endif // COBALT_UTILITY_IDENTIFIER_HPP_INCLUDED
