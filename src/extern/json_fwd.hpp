//     __ _____ _____ _____
//  __|  |   __|     |   | |  JSON for Modern C++
// |  |  |__   |  |  | | | |  version 3.11.3
// |_____|_____|_____|_|___|  https://github.com/nlohmann/json
//
// SPDX-FileCopyrightText: 2013-2023 Niels Lohmann <https://nlohmann.me>
// SPDX-License-Identifier: MIT

#ifndef INCLUDE_NLOHMANN_JSON_FWD_HPP_
#define INCLUDE_NLOHMANN_JSON_FWD_HPP_

#include <cstdint> // int64_t, uint64_t
#include <map> // map
#include <memory> // allocator
#include <string> // string
#include <vector> // vector

#ifndef NLOHMANN_JSON_NAMESPACE_BEGIN
#define NLOHMANN_JSON_NAMESPACE_BEGIN namespace nlohmann {
#define NLOHMANN_JSON_NAMESPACE_END   }
#endif

NLOHMANN_JSON_NAMESPACE_BEGIN

template<typename T = void, typename SFINAE = void>
struct adl_serializer;

template<template<typename U, typename V, typename... Args> class ObjectType =
         std::map,
         template<typename U, typename... Args> class ArrayType = std::vector,
         class StringType = std::string, class BooleanType = bool,
         class NumberIntegerType = std::int64_t,
         class NumberUnsignedType = std::uint64_t,
         class NumberFloatType = double,
         template<typename U> class AllocatorType = std::allocator,
         template<typename T, typename SFINAE = void> class JSONSerializer =
         adl_serializer,
         class BinaryType = std::vector<std::uint8_t>,
         class CustomBaseClass = void>
class basic_json;

template<typename RefStringType>
class json_pointer;

using json = basic_json<>;

template<class Key, class T, class IgnoredLess, class Allocator>
struct ordered_map;

using ordered_json = basic_json<nlohmann::ordered_map>;

NLOHMANN_JSON_NAMESPACE_END

#endif  // INCLUDE_NLOHMANN_JSON_FWD_HPP_
