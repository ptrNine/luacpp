#pragma once

#include <tuple>
#include <string>
#include <cstdint>

#include "luacpp_integral_constant.hpp"

namespace luacpp {

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

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

template <typename T>
struct lua_name_divide_result_t {
    bool _divide_result;
    T    _left;
    T    _right;

    const T& left() const {
        return _left;
    }
    const T& right() const {
        return _right;
    }
    constexpr bool success() const {
        return _divide_result;
    }
    constexpr operator bool() const {
        return success();
    }
};

template <size_t I, bool Success, typename T1, typename T2>
constexpr auto get(const lua_tname_divide_result_t<Success, T1, T2>& r) {
    if constexpr (I == 0)
        return r.left();
    if constexpr (I == 1)
        return r.right();
}

template <bool Success, typename T1, typename T2>
constexpr auto lua_tname_divide_result(T1, T2) {
    return lua_tname_divide_result_t<Success, T1, T2>{};
}

struct lua_name {
    lua_name(std::string str): _storage(std::move(str)) {}

    std::string _storage;

    const char* data() const {
        return _storage.data();
    }

    size_t size() const {
        return _storage.size();
    }

    uint64_t hash() const {
        uint64_t hsh = 14695981039346656037ULL;
        for (auto c : _storage) {
            hsh ^= uint8_t(c);
            hsh *= 1099511628211ULL;
        }
        return hsh;
    }

    template <typename StartT, typename SizeT>
    auto substr(StartT start, SizeT sz) const {
        return lua_name{_storage.substr(start, sz)};
    }

    template <typename CharT>
    auto divide_by(CharT separator) const {
        auto pos = _storage.find(separator);

        if (pos == std::string::npos)
            return lua_name_divide_result_t<lua_name>{false, *this, *this};
        else
            return lua_name_divide_result_t<lua_name>{true, substr(0U, pos), substr(pos + 1, size() - (pos + 1))};
    }

    auto operator+(const lua_name& name) const {
        return lua_name{_storage + name._storage};
    }

    auto dot(const lua_name& name) const {
        return lua_name{_storage + '.' + name._storage};
    }

    operator const std::string&() const {
        return _storage;
    }

    operator std::string_view() const {
        return _storage;
    }

    bool operator==(const lua_name& name) const {
        return _storage == name._storage;
    }
    bool operator!=(const lua_name& name) const {
        return _storage != name._storage;
    }
};

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

    template <typename StartT, typename SizeT = int_const<sizeof...(Cs) - StartT{}>>
    constexpr auto substr(StartT, SizeT) const {
        return []<size_t... Idxs>(std::index_sequence<Idxs...>) {
            return lua_tname<_storage[StartT{} + Idxs]...>{};
        }
        (std::make_index_sequence<SizeT{}>());
    }

    template <typename CharT>
    constexpr auto divide_by(CharT) const {
        constexpr auto pos = []<size_t... Idxs>(std::index_sequence<Idxs...>) {
            size_t p = 0;
            ((_storage[sizeof...(Idxs) - Idxs - 1] == char(CharT{}) ? (p = (sizeof...(Idxs) - Idxs)) : (p)), ...);
            return p - 1;
            // return ((Cs == separator ? Idxs + 1 : 0) + ... + 1) - 2;
        }
        (std::make_index_sequence<sizeof...(Cs)>());

        if constexpr (pos == size_t(-1))
            return lua_tname_divide_result<false>(lua_tname<Cs...>{}, lua_tname<Cs...>{});
        else
            return lua_tname_divide_result<true>(substr(int_const<size_t(0)>{}, int_const<pos>{}),
                                                 substr(int_const<pos + 1>{}, int_const<sizeof...(Cs) - (pos + 1)>{}));
    }

    template <char... Cs2>
    constexpr auto operator+(lua_tname<Cs2...>) const {
        return lua_tname<Cs..., Cs2...>{};
    }

    template <char... Cs2>
    constexpr auto dot(lua_tname<Cs2...>) const {
        return lua_tname<Cs..., '.', Cs2...>{};
    }

    constexpr auto dot(std::string_view name) const {
        return lua_name{std::string(*this) + '.' + std::string(name)};
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
    bool operator==(const lua_name& name) const {
        return std::string_view(*this) == name;
    }
    bool operator!=(const lua_name& name) const {
        return std::string_view(*this) != name;
    }
    friend bool operator==(const lua_name& lhs, lua_tname<Cs...> rhs) {
        return std::string_view(lhs) == rhs;
    }
    friend bool operator!=(const lua_name& lhs, lua_tname<Cs...> rhs) {
        return std::string_view(lhs) != rhs;
    }
};

#define LUA_TNAME(STR)                                                                                                 \
    []<size_t... Idxs>(std::index_sequence<Idxs...>) constexpr {                                                       \
        return luacpp::lua_tname<STR[Idxs]...>();                                                                      \
    }                                                                                                                  \
    (std::make_index_sequence<sizeof(STR) - 1>())

// template <typename T, T... Cs>
// constexpr lua_tname<Cs...> operator""_tname() { return {}; }

template <typename Type, auto LuaName>
struct typespec {
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
constexpr size_t tuniqfind(auto&& f) {
    return []<size_t... Idxs>(auto&& f, std::index_sequence<Idxs...>) {
        return ((f(details::telement<Idxs>(T{})) ? Idxs + size_t(1) : size_t(0)) + ... + size_t(0)) - size_t(1);
    }
    (f, std::make_index_sequence<std::tuple_size_v<T>>());
}

template <typename T>
constexpr void tforeach(auto&& f) {
    []<typename... ArgsT>(auto&& f, std::tuple<ArgsT...>) {
        (f(ArgsT()), ...);
    }(f, T{});
}

template <size_t>
struct typespec_list_s;

template <size_t tag>
constexpr auto typespec_list_get_type() {
    if constexpr (requires { typename typespec_list_s<tag>::type; })
        return typename typespec_list_s<tag>::type{};
    else
        return std::tuple<>{};
}

template <size_t tag>
using typespec_list = decltype(typespec_list_get_type<tag>());

template <typename T>
struct usertype_method_loader;

} // namespace luacpp

namespace std
{
template <bool Success, typename T1, typename T2>
struct tuple_element<0, luacpp::lua_tname_divide_result_t<Success, T1, T2>> {
    using type = T1;
};
template <bool Success, typename T1, typename T2>
struct tuple_element<1, luacpp::lua_tname_divide_result_t<Success, T1, T2>> {
    using type = T2;
};
template <bool Success, typename T1, typename T2>
struct tuple_size<luacpp::lua_tname_divide_result_t<Success, T1, T2>> {
    static constexpr size_t value = 2;
};
} // namespace std
