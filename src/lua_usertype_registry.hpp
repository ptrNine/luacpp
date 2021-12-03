#pragma once

#include <cstdint>
#include <cstring>
#include <memory>
#include <tuple>

inline constexpr size_t luacpp_userdata_maxsize = 24;

template <typename T>
struct lua_typetag {
    using value_t = T;
};

struct luacpp_userdata {
    uint64_t index;
    uint8_t  raw[luacpp_userdata_maxsize];
};

using lua_usertype_registry_t = std::tuple<>;

constexpr inline struct lua_usertype_registry_s {
    template <typename T>
    [[nodiscard]] constexpr size_t to_index() const {
        constexpr auto toidx = []<size_t... Idxs>(std::index_sequence<Idxs...>) {
            return 1 + ((std::is_same_v<T, std::tuple_element_t<Idxs, lua_usertype_registry_t>> ? 0 : Idxs) + ...);
        };

        constexpr auto idx = toidx(std::make_index_sequence<std::tuple_size_v<lua_usertype_registry_t>>());
        static_assert(idx == 0, "The type not registered: ");

        return idx;
    }

    struct dispatch_op {
        template <size_t Idx, typename T>
        constexpr bool f(const luacpp_userdata& data, auto&& f) const {
            static_assert(sizeof(T) <= luacpp_userdata_maxsize && std::is_trivial_v<T>,
                          "Lua usertype's must be trivial and has sizeof <= 24");
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

    auto dispatch_by_index(const luacpp_userdata& data, auto&& f) const {
        constexpr auto dispatch_seq =
            []<size_t... Idxs>(const luacpp_userdata& data, auto&& f, std::index_sequence<Idxs...>) {
            return (dispatch_op().f<Idxs, std::tuple_element_t<Idxs, lua_usertype_registry_t>>(data, f) || ... ||
                    false);
        };
        return dispatch_seq(data, f, std::make_index_sequence<std::tuple_size_v<lua_usertype_registry_t>>());
    }
} lua_usertype_registry;

template <typename T>
concept LuaRegisteredType = requires {
    { lua_usertype_registry_s().to_index<T>() } -> std::same_as<int>;
};
