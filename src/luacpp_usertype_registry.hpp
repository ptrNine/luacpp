#pragma once

#include <cstdint>
#include <cstring>
#include <memory>
#include <tuple>

#include "luacpp_basic.hpp"

namespace luacpp
{

struct type_registry {
    static constexpr auto no_index = size_t(-1);

    template <typename T>
    static constexpr size_t get_index() {
        return tuniqfind<typespec_list<0>>(
            [](auto&& usertype) { return std::is_same_v<decltype(usertype.type()), T>; });
    }

    template <typename T>
    static constexpr auto get_typespec() {
        constexpr auto idx = get_index<T>();
        return details::telement<idx>(typespec_list<0>{});
    }

    static constexpr void typespec_dispatch(size_t type_index, auto&& function) {
        []<size_t... Idxs>(size_t type_index, auto&& function, std::index_sequence<Idxs...>) {
            ((type_index == Idxs ? (function(details::telement<Idxs, typespec_list<0>>()), 0) : 0) + ...);
        }
        (type_index, function, std::make_index_sequence<std::tuple_size_v<typespec_list<0>>>());
    }
};

template <typename T>
concept LuaRegisteredType = type_registry::get_index<T>() != type_registry::no_index;

template <typename T>
concept LuaRegisteredTypeRefOrPtr = LuaRegisteredType<std::decay_t<std::remove_pointer_t<T>>>;

} // namespace luacpp
