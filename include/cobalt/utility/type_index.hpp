#ifndef COBALT_UTILITY_TYPE_INDEX_HPP_INCLUDED
#define COBALT_UTILITY_TYPE_INDEX_HPP_INCLUDED

#pragma once

#include <boost/type_index.hpp>
#include <boost/functional/hash.hpp>

namespace cobalt {

using boost::typeindex::type_index;
using boost::typeindex::type_id;
using boost::typeindex::type_id_runtime;

} // namespace cobalt

namespace std {

template <>
struct hash<cobalt::type_index> : boost::hash<cobalt::type_index> {
};

} // namespace std

#endif // COBALT_UTILITY_TYPE_INDEX_HPP_INCLUDED
