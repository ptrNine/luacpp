#pragma once

#include <cstdint>
#include <cstring>
#include <memory>
#include <tuple>

#include "lua_usertype.hpp"

inline constexpr size_t luacpp_userdata_maxsize = 24;

template <typename T>
struct lua_typetag {
    using value_t = T;
};

struct luacpp_userdata {
    uint64_t index;
    uint8_t  raw[luacpp_userdata_maxsize];
};

template <typename T>
struct luacpp_boxedtype_rawpointer {
    using type = T;
    T* ptr;
};

template <typename T>
struct is_boxedtype_rawpointer : std::false_type{};
template <typename T>
struct is_boxedtype_rawpointer<luacpp_boxedtype_rawpointer<T>> : std::true_type{};

template <typename T>
concept BoxedTypeRawPointer = is_boxedtype_rawpointer<T>::value;

template <int Tag>
struct lua_usertype_registry_s {
    template <typename T>
    [[nodiscard]] static constexpr size_t to_index() {
        constexpr auto toidx = []<size_t... Idxs>(std::index_sequence<Idxs...>) {
            return ((std::is_same_v<T, decltype(std::tuple_element_t<Idxs, typename luacpp_usertype_list<Tag>::type>().type())>
                             ? Idxs + 1
                             : 0) +
                        ...);
        };

        constexpr auto idx = toidx(std::make_index_sequence<std::tuple_size_v<typename luacpp_usertype_list<Tag>::type>>());
        //static_assert(idx == 0, "The type not registered: ");

        return idx - 1;
    }

    template <BoxedTypeRawPointer T>
    static constexpr size_t to_index() {
        return to_index<typename T::type>();
    }

    template <typename T>
    static constexpr luacpp_memclass memclass() {
        constexpr auto idx = to_index<T>();
        return std::tuple_element_t<idx, typename luacpp_usertype_list<Tag>::type>().memclass();
    }

    template <typename T>
    static constexpr auto lua_name() {
        constexpr auto idx = to_index<T>();
        return std::tuple_element_t<idx, typename luacpp_usertype_list<Tag>::type>().lua_name();
    }

    template <typename T> requires luacpp_boxedtype_rawpointer<T>::value
    static constexpr luacpp_memclass memclass() {
        return memclass<T::type>();
    }

    struct dispatch_op {
        template <size_t Idx, typename T>
        static constexpr bool f(const luacpp_userdata& data, auto&& f) {
            static_assert(std::tuple_element_t<Idx, typename luacpp_usertype_list<Tag>::type>().memclass() !=
                                  luacpp_memclass::flat ||
                              (sizeof(T) <= luacpp_userdata_maxsize && std::is_trivial_v<T>),
                          "Flat lua usertype's must be trivial and has sizeof <= 24");
            if (Idx == data.index) {
                if constexpr (std::is_invocable_v<decltype(f), T>) {
                    T real_value;
                    std::memcpy(&real_value, data.raw, sizeof(real_value));
                    f(real_value);
                }
                else if constexpr (std::is_invocable_v<decltype(f), lua_typetag<T>>) {
                    f(lua_typetag<T>());
                }
                return true;
            }
            return false;
        }
    };

    static auto dispatch_by_index(const luacpp_userdata& data, auto&& f) {
        constexpr auto dispatch_seq =
            []<size_t... Idxs>(const luacpp_userdata& data, auto&& f, std::index_sequence<Idxs...>) {
            return (
                dispatch_op()
                    .template f<Idxs, decltype(std::tuple_element_t<Idxs, typename luacpp_usertype_list<Tag>::type>().type())>(
                        data, f) ||
                ... || false);
        };
        return dispatch_seq(
            data, f, std::make_index_sequence<std::tuple_size_v<typename luacpp_usertype_list<Tag>::type>>());
    }
};

template <typename T>
concept LuaRegisteredType = lua_usertype_registry_s<0>::to_index<T>() != size_t(-1);
