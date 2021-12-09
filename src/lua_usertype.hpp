#pragma once

#include <tuple>
#include <string>
#include <cstdint>

template <char C>
concept LuaValidNameChar = (C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z') ||
                           (C >= '0' && C <= '9') || C == '_' || C == '.';

template <bool Success, typename T1, typename T2>
struct lua_tname_divide_result_t {
    constexpr T1 left() const {
        return T1{};
    }
    constexpr T2 right() const {
        return T2{};
    }
    constexpr bool success() const {
        return Success;
    }
    constexpr operator bool() const {
        return success();
    }
};

template <size_t I, bool Success, typename T1, typename T2>
constexpr auto get(const lua_tname_divide_result_t<Success, T1, T2>&) {
    if constexpr (I == 0)
        return T1{};
    if constexpr (I == 1)
        return T2{};
}

namespace std
{
template <bool Success, typename T1, typename T2>
struct tuple_element<0, lua_tname_divide_result_t<Success, T1, T2>> {
    using type = T1;
};
template <bool Success, typename T1, typename T2>
struct tuple_element<1, lua_tname_divide_result_t<Success, T1, T2>> {
    using type = T2;
};
template <bool Success, typename T1, typename T2>
struct tuple_size<lua_tname_divide_result_t<Success, T1, T2>> {
    static constexpr size_t value = 2;
};
} // namespace std

template <bool Success, typename T1, typename T2>
constexpr auto lua_tname_divide_result(T1, T2) {
    return lua_tname_divide_result_t<Success, T1, T2>();
}

template <char... Cs>
    requires(LuaValidNameChar<Cs>&&...)
struct lua_tname {
    static constexpr const char _storage[] = {Cs..., '\0'};

    constexpr const char* data() const {
        return _storage;
    }

    constexpr size_t size() const {
        return sizeof...(Cs);
    }

    constexpr uint64_t hash() const {
        uint64_t hsh = 14695981039346656037ULL;
        (((hsh ^= Cs) *= 1099511628211ULL), ...);
        return hsh;
    }

    template <size_t start, size_t size = sizeof...(Cs) - start>
    constexpr auto substr() const {
        return []<size_t... Idxs>(std::index_sequence<Idxs...>) {
            return lua_tname<_storage[start + Idxs]...>{};
        }
        (std::make_index_sequence<size>());
    }

    template <char separator>
    constexpr auto divide_by() const {
        constexpr auto pos = []<size_t... Idxs>(std::index_sequence<Idxs...>) {
            size_t p = 0;
            ((_storage[sizeof...(Idxs) - Idxs - 1] == separator ? (p = (sizeof...(Idxs) - Idxs)) : (p)), ...);
            return p - 1;
            // return ((Cs == separator ? Idxs + 1 : 0) + ... + 1) - 2;
        }
        (std::make_index_sequence<sizeof...(Cs)>());

        if constexpr (pos == size_t(-1))
            return lua_tname_divide_result<false>(lua_tname<Cs...>{}, lua_tname<Cs...>{});
        else
            return lua_tname_divide_result<true>(substr<0, pos>(), substr<pos + 1, sizeof...(Cs) - (pos + 1)>());
    }

    template <char... Cs2>
    constexpr auto operator+(lua_tname<Cs2...>) const {
        return lua_tname<Cs..., Cs2...>{};
    }

    template <char... Cs2>
    constexpr auto dot(lua_tname<Cs2...>) const {
        return lua_tname<Cs..., '.', Cs2...>{};
    }

    operator std::string() const {
        return {data(), size()};
    }

    constexpr operator std::string_view() const {
        return {data(), size()};
    }

    template <char... Cs2>
    constexpr bool operator==(const lua_tname<Cs2...>&&) const {
        return std::is_same_v<lua_tname, lua_tname<Cs2...>>;
    }
    template <char... Cs2>
    constexpr bool operator!=(const lua_tname<Cs2...>&&) const {
        return !std::is_same_v<lua_tname, lua_tname<Cs2...>>;
    }
};

#define LUA_TNAME(STR)                                                                                                 \
    []<size_t... Idxs>(std::index_sequence<Idxs...>) constexpr {                                                       \
        return lua_tname<STR[Idxs]...>();                                                                              \
    }                                                                                                                  \
    (std::make_index_sequence<sizeof(STR) - 1>())

//template <typename T, T... Cs>
//constexpr lua_tname<Cs...> operator""_tname() { return {}; }

template <typename Type, auto LuaName>
struct luacpp_typespec {
    constexpr Type type() const;
    constexpr auto lua_name() const {
        return LuaName;
    }
};

namespace details
{
struct telement_transparent_type {
    template <typename T>
    constexpr T operator+(T) const {
        return {};
    }
    template <typename T>
    friend constexpr T operator+(T, telement_transparent_type) {
        return {};
    }
    constexpr telement_transparent_type operator+(telement_transparent_type) {
        return {};
    }
};

template <bool v, typename T>
constexpr auto tget_type_or_transparent() {
    if constexpr (v)
        return T{};
    else
        return telement_transparent_type{};
}

template <size_t Idx, typename... ArgsT>
    requires(Idx < sizeof...(ArgsT))
constexpr auto telement(std::tuple<ArgsT...>) {
    constexpr auto f = []<size_t... Idxs>(std::index_sequence<Idxs...>) {
        return (tget_type_or_transparent<Idx == Idxs, ArgsT>() + ...);
    };
    return decltype(f(std::make_index_sequence<sizeof...(ArgsT)>()))();
}
} // namespace details

template <typename T>
constexpr size_t luacpp_tuniqfind(auto&& f) {
    return []<size_t... Idxs>(auto&& f, std::index_sequence<Idxs...>) {
        return ((f(details::telement<Idxs>(T{})) ? Idxs + 1U : 0U) + ... + 0U) - 1U;
    }
    (f, std::make_index_sequence<std::tuple_size_v<T>>());
}

template <typename T>
constexpr void luacpp_tforeach(auto&& f) {
    []<typename... ArgsT>(auto&& f, std::tuple<ArgsT...>) {
        (f(ArgsT()), ...);
    }(f, T{});
}


#include <string>
#include <iostream>

struct vec_sample {
    float x, y, z;

    vec_sample(float ix, float iy, float iz): x(ix), y(iy), z(iz) {
        std::cout << "New CTOR" << std::endl;
    }

    vec_sample(const vec_sample& v): x(v.x), y(v.y), z(v.z) {
        std::cout << "Copy CTOR" << std::endl;
    }

    vec_sample(vec_sample&& v) noexcept: x(v.x), y(v.y), z(v.z) {
        std::cout << "Move CTOR" << std::endl;
    }

    vec_sample& operator=(const vec_sample& v) {
        std::cout << "Copy asignment" << std::endl;
        x = v.x;
        y = v.y;
        z = v.z;
        return *this;
    }

    vec_sample& operator=(vec_sample&& v) noexcept {
        std::cout << "Move asignment" << std::endl;
        x = v.x;
        y = v.y;
        z = v.z;
        return *this;
    }

    void test(int i) {
        std::cout << "int f: arg = " << i << " vec: " << tostring() << std::endl;
    }
    void test(std::string) {
        std::cout << __PRETTY_FUNCTION__ << std::endl;
    }

    std::string tostring() const noexcept {
        return std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z);
    }

    ~vec_sample() {
        std::cout << "DELETED" << std::endl;
    }
};

using luacpp_typespec_list = std::tuple<luacpp_typespec<std::string_view, LUA_TNAME("strview")>,
                                        luacpp_typespec<std::u16string_view, LUA_TNAME("strview2")>,
                                        luacpp_typespec<vec_sample, LUA_TNAME("vec_sample")>>;

template <typename T>
struct luacpp_usertype_method_loader;
