#ifndef COBALT_UTILITY_TYPE_INDEX_HPP_INCLUDED
#define COBALT_UTILITY_TYPE_INDEX_HPP_INCLUDED

#pragma once

#include <string_view>

#include <boost/type_index.hpp>
#include <boost/functional/hash.hpp>

namespace cobalt {

using boost::typeindex::type_info;
using boost::typeindex::type_index;
using boost::typeindex::type_id;
using boost::typeindex::type_id_runtime;

} // namespace cobalt

namespace std {

template <>
struct hash<cobalt::type_index> : boost::hash<cobalt::type_index> {
};

} // namespace std

template <typename T>
constexpr auto type_name() {
    std::string_view name, prefix, suffix;
#ifdef __clang__
    name = __PRETTY_FUNCTION__;
    prefix = "auto type_name() [T = ";
    suffix = "]";
#elif defined(__GNUC__)
    name = __PRETTY_FUNCTION__;
    prefix = "constexpr auto type_name() [with T = ";
    suffix = "]";
#elif defined(_MSC_VER)
    name = __FUNCSIG__;
    prefix = "auto __cdecl type_name<";
    suffix = ">(void)";
#endif
    name.remove_prefix(prefix.size());
    name.remove_suffix(suffix.size());
    return name;
}

#endif // COBALT_UTILITY_TYPE_INDEX_HPP_INCLUDED
