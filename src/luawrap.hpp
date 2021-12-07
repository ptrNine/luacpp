#pragma once

#include <concepts>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>

#include <luajit-2.1/lua.hpp>

#include "lua_usertype_registry.hpp"
#include "lua_utils.hpp"

template <typename T>
concept LuaNumber = (std::is_integral_v<T> || std::is_floating_point_v<T>) && !std::same_as<T, bool>;

template <typename T>
concept LuaNumberOrRef = LuaNumber<std::decay_t<T>>;

template <typename T>
concept LuaStringLike = std::same_as<T, std::string> || std::same_as<T, std::string_view>;

template <typename T>
concept LuaStringLikeOrRef = LuaStringLike<std::decay_t<T>>;

template <typename T>
concept LuaListLike = !LuaStringLike<T> && requires(const T& v) {
    {begin(v)};
    {end(v)};
};

template <typename T>
concept LuaListLikeOrRef = LuaListLike<std::decay_t<T>>;

template <typename T>
concept LuaArrayLike = LuaListLike<T> && requires(const T& v) {
    { end(v) - begin(v) } -> std::convertible_to<size_t>;
};

template <typename T>
concept LuaArrayLikeOrRef = LuaArrayLike<std::decay_t<T>>;

template <typename T>
concept LuaPushBackable = !LuaStringLike<T> && requires(T& v) {
    {v.push_back(std::declval<decltype(*v.begin())>())};
};

template <typename T>
concept LuaPushBackableOrRef = LuaPushBackable<std::decay_t<T>>;

template <typename T>
concept LuaStaticSettable = !LuaStringLike<T> && !LuaPushBackable<T> && requires(T & v) {
    {v[0] = v[0]};
    { size(v) } -> std::convertible_to<size_t>;
};

template <typename T>
concept LuaStaticSettableOrRef = LuaStaticSettable<std::decay_t<T>>;

template <typename T>
concept LuaTupleLike = requires {
    std::tuple_size<T>::value;
};

template <typename T>
concept LuaTupleLikeOrRef = LuaTupleLike<std::decay_t<T>>;

template <typename T> requires std::same_as<T, bool>
inline void luacpp_push(lua_State* l, T value) {
    lua_pushboolean(l, value);
}

void luacpp_push(lua_State* l, LuaNumber auto value) {
    lua_pushnumber(l, lua_Number(value));
}

template <size_t S>
void luacpp_push(lua_State* l, const char (&value)[S]) {
    lua_pushlstring(l, value, S - 1);
}

inline void luacpp_push(lua_State* l, const char* value) {
    if (!value)
        lua_pushnil(l);
    else
        lua_pushstring(l, value);
}

inline void luacpp_push(lua_State* l, std::nullptr_t) {
    lua_pushnil(l);
}

void luacpp_push(lua_State* l, const LuaStringLike auto& value) {
    lua_pushlstring(l, value.data(), value.size());
}

void luacpp_push(lua_State* l, const auto* pointer_value) {
    if (!pointer_value)
        lua_pushnil(l);
    else
        luacpp_push(l, *pointer_value);
}

void luacpp_push(lua_State* l, const LuaListLike auto& value) {
    auto b = begin(value);
    auto e = end(value);

    if constexpr (LuaArrayLike<std::decay_t<decltype(value)>>)
        lua_createtable(l, int(e - b), 0);
    else
        lua_createtable(l, 0, 0);

    for (uint32_t i = 1; b != e; ++i, ++b) {
        lua_pushnumber(l, i);
        luacpp_push(l, *b);
        lua_settable(l, -3);
    }
}

void luacpp_push(lua_State* l, const LuaTupleLike auto& value) {
    lua_createtable(l, int(std::tuple_size_v<std::decay_t<decltype(value)>>), 0);
    []<size_t... Idxs>(lua_State * l, decltype(value) value, std::index_sequence<Idxs...>) {
        ((lua_pushnumber(l, int(Idxs) + 1), luacpp_push(l, std::get<Idxs>(value)), lua_settable(l, -3)), ...);
    }
    (l, value, std::make_index_sequence<std::tuple_size_v<std::decay_t<decltype(value)>>>());
}

template <LuaRegisteredType T>
T& luacpp_push(lua_State* l, const T& value) {
    void*    p          = lua_newuserdata(l, sizeof(uint64_t) + sizeof(value));
    uint64_t type_index = luacpp_type_registry::get_index<T>();
    std::memcpy(p, &type_index, sizeof(type_index));
    void* data = reinterpret_cast<char*>(p) + sizeof(uint64_t); // NOLINT
    auto  ptr  = new (data) T(value);                           // NOLINT
    lua_getglobal(l, luacpp_type_registry::get_typespec<T>().lua_name().data());
    lua_setmetatable(l, -2);

    return *ptr;
}

template <typename T> requires std::same_as<T, lua_CFunction>
inline void luacpp_push(lua_State* l, T value) {
    lua_pushcfunction(l, value);
}

class luacpp_cast_error : public std::runtime_error {
public:
    luacpp_cast_error(lua_State* l, int stackidx, const std::string& msg, const std::string& pf):
        std::runtime_error(std::string("luacpp: cast from ") + lua_typename(l, lua_type(l, stackidx)) + " failed (" +
                           msg + "). PRETTY_FUNCTION:\n" + pf) {}
};

class luacpp_call_cpp_error : public std::runtime_error {
public:
    luacpp_call_cpp_error(const std::string& msg): std::runtime_error("luacpp: call c++ function failed: " + msg) {}
};

struct luacpp_anycasted {
public:
    template <typename T>
    operator T() const {
        return T{};
    }
};

/*
template <typename T>
requires std::is_pointer_v<T> T luacpp_get(lua_State* l, int idx) {
    int type = lua_type(l, idx);
    switch (type) {
    case LUA_TNIL: return nullptr;
    case LUA_TLIGHTUSERDATA: return static_cast<T>(lua_touserdata(l, idx));
    default: throw luacpp_cast_error(l, idx, "this type can't be casted to C++ pointer", __PRETTY_FUNCTION__);
    }
}
*/

template <typename T>
    requires std::same_as<std::decay_t<T>, bool>
auto luacpp_get(lua_State* l, int idx) {
    if (lua_isboolean(l, idx))
        return lua_toboolean(l, idx);
    else
        throw luacpp_cast_error(l, idx, "this type can't be casted to C++ bool", __PRETTY_FUNCTION__);
}

template <typename T>
    requires std::same_as<std::decay_t<T>, bool>
bool luacpp_check(lua_State* l, int idx) {
    return lua_isboolean(l, idx);
}

template <LuaNumberOrRef T>
auto luacpp_get(lua_State* l, int idx) {
    if (lua_isnumber(l, idx))
        return std::decay_t<T>(lua_tonumber(l, idx));
    else
        throw luacpp_cast_error(l, idx, "this type can't be casted to C++ number", __PRETTY_FUNCTION__);
}

template <LuaNumberOrRef T>
bool luacpp_check(lua_State* l, int idx) {
    return lua_isnumber(l, idx);
}

template <typename T>
    requires std::same_as<std::decay_t<T>, std::string>
auto luacpp_get(lua_State* l, int idx) {
    if (lua_isstring(l, idx)) {
        size_t len;
        auto   str = lua_tolstring(l, idx, &len);
        return std::string(str, len);
    }
    else
        throw luacpp_cast_error(l, idx, "this type can't be casted to C++ std::string", __PRETTY_FUNCTION__);
}

template <typename T>
    requires std::same_as<std::decay_t<T>, std::string>
bool luacpp_check(lua_State* l, int idx) {
    return lua_isstring(l, idx);
}

namespace details {
    template <typename T>
    auto _luacpp_array_getnext(lua_State * l, int index_check) {
        if (!lua_isnumber(l, -2))
            throw luacpp_cast_error(l, -3, "some key of lua table is not a number", __PRETTY_FUNCTION__);

        auto idx = int(lua_tonumber(l, -2));
        if (idx != index_check)
            throw luacpp_cast_error(
                l, idx, "some index key of lua table violates continuous order", __PRETTY_FUNCTION__);

        return luacpp_get<T>(l, -1);
    }

    template <typename T>
    bool _luacpp_array_check(lua_State* l, int index_check) {
        return lua_isnumber(l, -2) && int(lua_tonumber(l, -2)) == index_check && luacpp_check<T>(l, -1);
    }
} // namespace details

template <LuaPushBackableOrRef T>
auto luacpp_get(lua_State* l, int idx) {
    if (!lua_istable(l, idx))
        throw luacpp_cast_error(l, idx, "this type can't be casted to C++ array-like container", __PRETTY_FUNCTION__);
    std::decay_t<T> result;
    using value_t = std::decay_t<decltype(*result.begin())>;

    /* For using relative stack pos */
    lua_pushvalue(l, idx);
    lua_pushnil(l);

    auto guard = luacpp_exception_guard{[l] {
        lua_pop(l, 3);
    }};

    int index_check = 1;
    while (lua_next(l, -2)) {
        result.push_back(details::_luacpp_array_getnext<value_t>(l, index_check));
        ++index_check;
        lua_pop(l, 1);
    }

    lua_pop(l, 1);

    return result;
}

template <typename T> requires LuaPushBackableOrRef<T> || LuaStaticSettableOrRef<T>
bool luacpp_check(lua_State* l, int idx) {
    if (!lua_istable(l, idx))
        return false;

    if constexpr (LuaStaticSettableOrRef<T>) {
        if (lua_objlen(l, idx) != size(T()))
            return false;
    }

    using value_t = std::decay_t<decltype(*std::declval<T>().begin())>;

    lua_pushvalue(l, idx);
    lua_pushnil(l);

    bool result     = true;
    int index_check = 1;
    while (result && lua_next(l, -2)) {
        if (details::_luacpp_array_check<value_t>(l, index_check))
            result = false;
        ++index_check;
        lua_pop(l, 1);
    }
    lua_pop(l, 1);

    return result;
}

template <LuaStaticSettableOrRef T>
auto luacpp_get(lua_State* l, int idx) {
    if (!lua_istable(l, idx))
        throw luacpp_cast_error(l, idx, "this type can't be casted to C++ array-like container", __PRETTY_FUNCTION__);
    std::decay_t<T> result;
    if (lua_objlen(l, idx) != size(result))
        throw luacpp_cast_error(l, idx, "array lengths do not match", __PRETTY_FUNCTION__);

    using value_t = std::decay_t<decltype(result[0])>;

    /* For using relative stack pos */
    lua_pushvalue(l, idx);
    lua_pushnil(l);

    auto guard = luacpp_exception_guard{[l] {
        lua_pop(l, 3);
    }};

    int index_check = 1;
    while (lua_next(l, -2)) {
        result[size_t(index_check - 1)] = details::_luacpp_array_getnext<value_t>(l, index_check);
        ++index_check;
        lua_pop(l, 1);
    }

    lua_pop(l, 1);

    return result;
}

template <LuaTupleLikeOrRef T>
auto luacpp_get(lua_State* l, int idx) {
    if (!lua_istable(l, idx))
        throw luacpp_cast_error(l, idx, "this type can't be casted to C++ tuple-like type", __PRETTY_FUNCTION__);
    std::decay_t<T> result;
    if (lua_objlen(l, idx) != std::tuple_size_v<T>)
        throw luacpp_cast_error(l, idx, "lua table and tuple lengths do not match", __PRETTY_FUNCTION__);

    using value_t = std::decay_t<decltype(result[0])>;

    /* TODO: fix this */

    /* For using relative stack pos */
    lua_pushvalue(l, idx);
    lua_pushnil(l);

    auto guard = luacpp_exception_guard{[l] {
        lua_pop(l, 3);
    }};

    static constexpr auto getall = []<size_t... Idxs>(T & v, lua_State * l, std::index_sequence<Idxs...>) {
        static constexpr auto op = []<size_t idx>(T& v, lua_State* l) {
            lua_next(l, -2);
            std::get<idx>(v) = details::_luacpp_array_getnext<value_t>(l, int(idx + 1));
            lua_pop(l, 1);
        };
        ((op<Idxs>(v, l)), ...);
    };

    getall(result, l, std::make_index_sequence<std::tuple_size_v<T>>());
    lua_pop(l, 1);

    return result;
}

template <LuaTupleLikeOrRef T>
bool luacpp_check(lua_State* l, int idx) {
    if (!lua_istable(l, idx))
        return false;

    if (lua_objlen(l, idx) != std::tuple_size_v<T>)
        return false;

    lua_pushvalue(l, idx);
    lua_pushnil(l);

    auto guard = luacpp_exception_guard{[l] {
        lua_pop(l, 3);
    }};

    /* TODO: fix this */

    return false;
}

/* This is the only thing that can return references */
template <LuaRegisteredTypeRefOrPtr T>
T luacpp_get(lua_State* l, int idx) {
    if (!lua_isuserdata(l, idx))
        throw luacpp_cast_error(l, idx, "this type can't be casted to C++ userdata type", __PRETTY_FUNCTION__);

    size_t objlen = lua_objlen(l, idx);
    constexpr size_t reallen = sizeof(uint64_t) + sizeof(std::remove_pointer_t<T>);
    if (objlen != reallen)
        throw luacpp_cast_error(l,
                                idx,
                                "userdata has invalid length (" + std::to_string(objlen) + " but should be " +
                                    std::to_string(reallen) + ") ",
                                __PRETTY_FUNCTION__);

    /* TODO: check address alignment for this */
    void* ptr = lua_touserdata(l, idx);
    uint64_t type_index;
    std::memcpy(&type_index, ptr, sizeof(type_index));

    using pointer_t = std::conditional_t<std::is_pointer_v<T>, T, std::decay_t<T>*>;

    if (luacpp_type_registry::get_index<std::remove_pointer_t<pointer_t>>() != type_index)
        throw luacpp_cast_error(
            l,
            idx,
            "userdata is not a " +
                std::string(
                    luacpp_type_registry::get_typespec<std::remove_pointer_t<pointer_t>>().lua_name().data()),
            __PRETTY_FUNCTION__);

    auto data = reinterpret_cast<pointer_t>(reinterpret_cast<char*>(ptr) + sizeof(uint64_t)); // NOLINT

    if constexpr (std::is_pointer_v<T>)
        return data;
    else
        return *data;
}

template <LuaRegisteredTypeRefOrPtr T>
bool luacpp_check(lua_State* l, int idx) {
    if (!lua_isuserdata(l, idx) || lua_objlen(l, idx) != sizeof(uint64_t) + sizeof(std::remove_pointer_t<T>))
        return false;

    void* ptr = lua_touserdata(l, idx);
    uint64_t type_index;
    std::memcpy(&type_index, ptr, sizeof(type_index));

    return luacpp_type_registry::get_index<std::decay_t<std::remove_pointer_t<T>>>() == type_index;
}

template <typename F, typename RF, uint64_t UniqId>
struct luacpp_func_storage {
    static luacpp_func_storage& instance() {
        static luacpp_func_storage inst;
        return inst;
    }

    int call(lua_State* state) const {
        return f(state, rf);
    }

    F f;
    RF rf;
};

template <typename F, uint64_t UniqId, typename... RFs>
struct luacpp_overloaded_func_storage {
    static luacpp_overloaded_func_storage& instance() {
        static luacpp_overloaded_func_storage inst;
        return inst;
    }

    int call(lua_State* state) const {
        return std::apply(f, std::tuple_cat(std::tuple{state}, rfs));
    }

    F f;
    std::tuple<RFs...> rfs;
};

template <typename F, typename RF, uint64_t UniqId>
struct luacpp_wrapped_function {
    static int call(lua_State* state) {
        return luacpp_func_storage<F, RF, UniqId>::instance().call(state);
    }
};

template <typename F, uint64_t UniqId, typename... RFs>
struct luacpp_wrapped_overloaded_function {
    static int call(lua_State* state) {
        return luacpp_overloaded_func_storage<F, UniqId, RFs...>::instance().call(state);
    }
};

template <typename FuncT, typename ReturnT, typename... ArgsT>
struct luacpp_native_function {
    FuncT function;
};

namespace details
{

template <typename ReturnT, typename... ArgsT>
int _luacpp_function_call(lua_State* l, auto&& function) {
    auto lua_args_count = lua_gettop(l);

    if (size_t(lua_args_count) != sizeof...(ArgsT))
        throw luacpp_call_cpp_error("arguments count mismatch (lua called with " + std::to_string(lua_args_count) +
                                    ", but C++ function defined with " + std::to_string(sizeof...(ArgsT)) +
                                    " arguments)");

    if constexpr (std::is_same_v<ReturnT, void>) {
        []<size_t... Idxs>([[maybe_unused]] lua_State * l, auto&& function, std::index_sequence<Idxs...>) {
            return function(luacpp_get<ArgsT>(l, int(Idxs + 1))...);
        }
        (l, function, std::make_index_sequence<sizeof...(ArgsT)>());
        return 0;
    }
    else {
        auto result = []<size_t... Idxs>(lua_State * l, auto&& function, std::index_sequence<Idxs...>) {
            return function(luacpp_get<ArgsT>(l, int(Idxs + 1))...);
        }
        (l, function, std::make_index_sequence<sizeof...(ArgsT)>());

        luacpp_push(l, result);
        return 1;
    }
}

template <typename...>
struct _luacpp_typelist {};

template <typename ReturnT, typename... ArgsT>
int _luacpp_function_call_typelist(lua_State* l, auto&& function, _luacpp_typelist<ArgsT...>) {
    return _luacpp_function_call<ReturnT, ArgsT...>(l, function);
}

template <uint64_t UniqId, typename F, typename ReturnT, typename... ArgsT>
lua_CFunction _luacpp_wrap_function(luacpp_native_function<F, ReturnT, ArgsT...>&& function) {
    auto func = [](lua_State* l, auto&& function) {
        return _luacpp_function_call<ReturnT, ArgsT...>(l, std::forward<decltype(function)>(function));
    };

    auto& func_storage = luacpp_func_storage<decltype(func), decltype(function.function), UniqId>::instance();
    func_storage.f = std::move(func);
    func_storage.rf = std::move(function.function);

    return &luacpp_wrapped_function<decltype(func), decltype(function.function), UniqId>{}.call;
}

template <uint64_t UniqId, typename F, typename ClassT, typename ReturnT, typename... ArgsT>
auto _luacpp_wrap_functional(ReturnT (ClassT::*)(ArgsT...) const, F&& function) {
    return _luacpp_wrap_function<UniqId>(
        luacpp_native_function<std::decay_t<decltype(function)>, ReturnT, ArgsT...>{function});
}

template <typename T>
concept LuaFunctional = requires {
    &T::operator();
};
} // namespace details

template <typename T>
concept LuaFunctionLike = details::LuaFunctional<T> || std::is_function_v<T>;

template <uint64_t UniqId, typename ReturnT, typename... ArgsT>
auto luacpp_wrap_function(ReturnT (*function)(ArgsT...)) {
    return details::_luacpp_wrap_function<UniqId>(
        luacpp_native_function<decltype(function), ReturnT, ArgsT...>{function});
}

template <uint64_t UniqId, details::LuaFunctional F>
auto luacpp_wrap_function(F&& function) {
    return details::_luacpp_wrap_functional<UniqId>(decltype(&F::operator()){}, function);
}

namespace details {
template <typename F>
struct lua_function_traits;

template <typename T>
struct lua_ttype {
    T type() const;
};

struct lua_noarg {};

template <typename ReturnT, typename... ArgsT>
struct lua_function_traits<ReturnT(*)(ArgsT...)> {
    static constexpr size_t arity = sizeof...(ArgsT);
    using return_t = ReturnT;


    template <size_t N>
    static constexpr auto arg_type() {
        if constexpr (N >= sizeof...(ArgsT))
            return lua_ttype<lua_noarg>{};
        else
            return lua_ttype<std::tuple_element_t<N, std::tuple<ArgsT...>>>{};
    }

    using arg_types = _luacpp_typelist<ArgsT...>;
};

template <typename ReturnT, typename ClassT, typename... ArgsT>
struct lua_function_traits<ReturnT(ClassT::*)(ArgsT...) const> : lua_function_traits<ReturnT(*)(ArgsT...)> {};

template <typename F> requires details::LuaFunctional<F>
struct lua_function_traits<F> : lua_function_traits<decltype(&F::operator())> {};

template <size_t>
struct lua_nttp_tag {};

template <size_t... Sz>
static constexpr bool lua_same_arity = (((Sz + ...) / sizeof...(Sz) == Sz) && ... && true);

template <typename... Fs>
constexpr size_t lua_max_arity() {
    size_t arity = 0;
    ((arity = arity < lua_function_traits<Fs>::arity ? lua_function_traits<Fs>::arity : arity), ...);
    return arity;
}

template <typename... F>
struct lua_fold_element {
    template <typename... F2>
    auto operator+(const lua_fold_element<F2...>& v) const {
        return lua_fold_element<F..., F2...>{std::tuple_cat(functions, v.functions)};
    }
    std::tuple<F...> functions;
};

template <typename F, size_t Arity>
constexpr auto lua_arity_fold(F&& function) {
    if constexpr (lua_function_traits<F>::arity == Arity)
        return lua_fold_element<F>{std::tuple<F>{std::forward<F>(function)}};
    else
        return lua_fold_element<>{};
}

template <typename>
struct lua_type_tag {
    constexpr lua_type_tag operator&&(lua_type_tag) const {
        return *this;
    }
};

template <typename... Ts>
concept LuaSameArgType = requires { (lua_type_tag<Ts>{} + ...); };

template <typename... CheckT>
struct lua_type_list {};

template <size_t arg_number, bool enable_call, typename F, typename... Fs> requires (lua_function_traits<F>::arity == 0)
int lua_overloaded_dispatch_by_arg_types(lua_State* l, F&& function, Fs&&...) {
    return _luacpp_function_call_typelist<typename lua_function_traits<F>::return_t>(
        l, std::forward<F>(function), typename lua_function_traits<F>::arg_types{});
}

template <size_t arg_number, bool enable_call, typename F, typename... Fs>
int lua_overloaded_dispatch_by_arg_types(lua_State* l, F&& function, Fs&&... functions) {
    using arg_type = decltype(lua_function_traits<F>::template arg_type<arg_number>().type());
    constexpr auto same_arg_type =
        LuaSameArgType<arg_type, decltype(lua_function_traits<Fs>::template arg_type<arg_number>().type())...>;

    constexpr size_t max_args_count = lua_function_traits<F>::arity;

    if constexpr (sizeof...(Fs) > 0 && same_arg_type) {
        if constexpr (arg_number + 1 < max_args_count) {
            return lua_overloaded_dispatch_by_arg_types<arg_number + 1, enable_call>(
                l, std::forward<F>(function), std::forward<Fs>(functions)...);
        }
        else {
            /* TODO: fail to select resolution */
            return -1;
        }
    }
    else {
        if (luacpp_check<arg_type>(l, int(arg_number + 1))) {
            if constexpr (arg_number + 1 < max_args_count) {
                int rc = lua_overloaded_dispatch_by_arg_types<arg_number + 1, false>(
                    l, std::forward<F>(function), std::forward<Fs>(functions)...);
                if (rc != -1)
                    return rc;
            }
            return _luacpp_function_call_typelist<typename lua_function_traits<F>::return_t>(
                l, std::forward<F>(function), typename lua_function_traits<F>::arg_types{});
        }

        /* Take next function if it exists */
        if constexpr (sizeof...(functions) > 0) {
            return lua_overloaded_dispatch_by_arg_types<arg_number, enable_call>(
                l, std::forward<Fs>(functions)...);
        }
        else {
            return -1;
        }
    }
}

template <size_t CurrentArity = 0, size_t MaxArity, typename A, typename... Fs>
int lua_args_count_recursive_dispatch(size_t args_count, A&& acceptor, Fs&&... functions) {
    if (args_count == CurrentArity) {
        return acceptor((lua_arity_fold<Fs, CurrentArity>(std::forward<Fs>(functions)) + ... + lua_fold_element<>{})
                     .functions);
    }
    else {
        if constexpr (CurrentArity + 1 <= MaxArity)
            return lua_args_count_recursive_dispatch<CurrentArity + 1, MaxArity, A, Fs...>(
                args_count, std::forward<A>(acceptor), std::forward<Fs>(functions)...);
    }
    return -1;
}

template <typename... Fs>
int lua_overloaded_call_dispatch(int args_count, lua_State* l, Fs&&... functions) {
    /* For the first try dispatch by arguments count.
     * Do this if arity differs only.
     */
    if constexpr (!details::lua_same_arity<details::lua_function_traits<Fs>::arity...>) {
        return details::lua_args_count_recursive_dispatch<0, details::lua_max_arity<Fs...>()>(
            args_count,
            [args_count, l]<typename... Fs2>(std::tuple<Fs2...> func_tuple) {
                /* Call itself with functions with matched arity only */
                if constexpr (sizeof...(Fs2) != 0)
                    return std::apply(lua_overloaded_call_dispatch<Fs2...>,
                               std::tuple_cat(std::tuple{args_count, l}, std::move(func_tuple)));
                return -1;
            },
            std::forward<Fs>(functions)...);
    }
    /* Second - dispatch by argument types */
    else {
        /* Things become more complex.
         * We should use the luacpp_check for every argument with different type
         */
        return lua_overloaded_dispatch_by_arg_types<0, true>(l, std::forward<Fs>(functions)...);
    }
}

template <typename... Fs>
int lua_overloaded_call_dispatch_entry(lua_State* l, Fs&&... functions) {
    int args_count = lua_gettop(l);
    if (((size_t(args_count) == details::lua_function_traits<Fs>::arity) || ... || false)) {
        int rc = lua_overloaded_call_dispatch(args_count, l, std::forward<Fs>(functions)...);
        if (rc == -1)
            throw luacpp_call_cpp_error("no matched overloaded function");
        return rc;
    }
    else
        throw luacpp_call_cpp_error("no matched overloaded function (cannot call with " + std::to_string(args_count) +
                                    " arguments)");
}
}

template <uint64_t UniqId, typename... Fs>
auto luacpp_wrap_overloaded_functions(Fs&&... functions) {
    auto func = &details::lua_overloaded_call_dispatch_entry<Fs...>;
    auto& func_storage = luacpp_overloaded_func_storage<decltype(func), UniqId, std::decay_t<Fs>...>::instance();
    func_storage.f = std::move(func);
    func_storage.rfs = std::forward_as_tuple(std::forward<Fs>(functions)...);

    return &luacpp_wrapped_overloaded_function<decltype(func), UniqId, std::decay_t<Fs>...>{}.call;

}

namespace details {
    template <typename TName>
    decltype(auto) _lua_recursive_provide_namespaced_value(TName name, lua_State* l, auto&& value) {
        constexpr auto dotsplit = name.template divide_by<'.'>();
        if constexpr (dotsplit) {
            static_assert(dotsplit.left().size() > 0, "Attempt to declare lua table with empty name");
            lua_getfield(l, -1, dotsplit.left().data());
            if (!lua_istable(l, -1)) {
                lua_pop(l, 1);
                lua_newtable(l);
                lua_pushstring(l, dotsplit.left().data());
                lua_pushvalue(l, -2);
                lua_rawset(l, -4);
            }
            auto pop_on_scope_exit = luacpp_finalizer{[&] {
                lua_pop(l, 1);
            }};
            return _lua_recursive_provide_namespaced_value(dotsplit.right(), l, value);
        }
        else {
            static_assert(name.size() > 0, "Attempt to declare lua function with empty name");
            lua_pushstring(l, name.data());

            auto rawset_on_scope_exit = luacpp_finalizer{[&]{
                lua_rawset(l, -3);
            }};

            return luacpp_push(l, value);
        }
    }
}

template <typename T, typename TName>
decltype(auto) lua_provide(TName name, lua_State* l, T&& value) {
    constexpr auto dotsplit = name.template divide_by<'.'>();
    if constexpr (dotsplit) {
        static_assert(dotsplit.left().size() > 0, "Attempt to declare lua table with empty name");
        lua_getglobal(l, dotsplit.left().data());
        if (!lua_istable(l, -1)) {
            lua_pop(l, 1);
            lua_newtable(l);
            lua_pushvalue(l, -1);
            lua_setglobal(l, dotsplit.left().data());
        }

        auto pop_on_scope_exit = luacpp_finalizer{[&] {
            lua_pop(l, 1);
        }};

        return details::_lua_recursive_provide_namespaced_value(dotsplit.right(), l, value);
    }
    else {
        static_assert(name.size() > 0, "Attempt to declare lua function with empty name");
        auto setglobal = luacpp_finalizer{[&] {
            lua_setglobal(l, name.data());
        }};
        return luacpp_push(l, value);
    }
}

template <LuaFunctionLike F, typename TName>
lua_CFunction lua_provide(TName name, lua_State* l, F&& function) {
    constexpr auto hash = name.hash();
    auto lua_func = luacpp_wrap_function<hash>(std::forward<F>(function));
    lua_provide(name, l, lua_func);
    return lua_func;
}

template <typename TName, LuaFunctionLike... Fs>
lua_CFunction lua_provide(TName name, lua_State* l, Fs&&... functions) {
    constexpr auto hash = name.hash();
    auto lua_func = luacpp_wrap_overloaded_functions<hash>(std::forward<Fs>(functions)...);
    lua_provide(name, l, lua_func);
    return lua_func;
}

namespace details
{
template <bool Noexcept, bool Const, typename ReturnT, typename ClassT, typename... ArgsT>
struct luacpp_member_wrapper {
    ReturnT operator()(ClassT& it, ArgsT... args) const {
        return (it.*f)(args...);
    }
    std::conditional_t<
        Const,
        std::conditional_t<Noexcept, ReturnT (ClassT::*)(ArgsT...) const noexcept, ReturnT (ClassT::*)(ArgsT...) const>,
        std::conditional_t<Noexcept, ReturnT (ClassT::*)(ArgsT...) noexcept, ReturnT (ClassT::*)(ArgsT...)>>
        f;
};
template <bool Noexcept, bool Const, typename F, typename ReturnT, typename ClassT, typename TName, typename... ArgsT>
lua_CFunction _lua_provide_member_function(TName name, lua_State* l, F member_function) {
    constexpr auto typespec = luacpp_type_registry::get_typespec<ClassT>();
    constexpr auto fullname = typespec.lua_name().dot(name);
    constexpr auto hash     = fullname.hash();
    auto           lua_func = luacpp_wrap_function<hash>(
        details::luacpp_member_wrapper<Noexcept, Const, ReturnT, ClassT, ArgsT...>{member_function});
    lua_provide(fullname, l, lua_func);
    return lua_func;
}
} // namespace details

/* Handle const*noexcept
 * Is there a better way?..
 */

template <typename ReturnT, typename ClassT, typename TName, typename... ArgsT>
lua_CFunction lua_provide(TName name, lua_State* l, ReturnT (ClassT::*member_function)(ArgsT...)) {
    return details::
        _lua_provide_member_function<false, false, decltype(member_function), ReturnT, ClassT, TName, ArgsT...>(
            name, l, member_function);
}

template <typename ReturnT, typename ClassT, typename TName, typename... ArgsT>
lua_CFunction lua_provide(TName name, lua_State* l, ReturnT (ClassT::*member_function)(ArgsT...) const) {
    return details::
        _lua_provide_member_function<false, true, decltype(member_function), ReturnT, ClassT, TName, ArgsT...>(
            name, l, member_function);
}

template <typename ReturnT, typename ClassT, typename TName, typename... ArgsT>
lua_CFunction lua_provide(TName name, lua_State* l, ReturnT (ClassT::*member_function)(ArgsT...) noexcept) {
    return details::
        _lua_provide_member_function<true, false, decltype(member_function), ReturnT, ClassT, TName, ArgsT...>(
            name, l, member_function);
}

template <typename ReturnT, typename ClassT, typename TName, typename... ArgsT>
lua_CFunction lua_provide(TName name, lua_State* l, ReturnT (ClassT::*member_function)(ArgsT...) const noexcept) {
    return details::
        _lua_provide_member_function<true, true, decltype(member_function), ReturnT, ClassT, TName, ArgsT...>(
            name, l, member_function);
}

class luacpp_access_error : public std::runtime_error {
public:
    luacpp_access_error(const std::string& msg): std::runtime_error("luacpp: " + msg) {}
};

template <typename TName>
int luacpp_access(TName name, lua_State* l, int stack_depth = 0) {
    constexpr auto dotsplit = name.template divide_by<'.'>();

    auto guard = luacpp_exception_guard{[&] {
        lua_pop(l, stack_depth);
    }};

    if constexpr (dotsplit) {
        static_assert(dotsplit.left().size() > 0, "Attempt to access lua variable with empty name");

        ++stack_depth;
        lua_getfield(l, stack_depth == 1 ? LUA_GLOBALSINDEX : -1, dotsplit.left().data());
        guard.dismiss();
        return luacpp_access(dotsplit.right(), l, stack_depth);
    }
    else {
        static_assert(name.size() > 0, "Attempt to access lua variable with empty name");
        ++stack_depth;
        lua_getfield(l, stack_depth == 1 ? LUA_GLOBALSINDEX : -1, name.data());
        return stack_depth;
    }
}

template <typename T, typename TName>
decltype(auto) luacpp_extract(TName, lua_State* l) {
    int  stack_depth = luacpp_access(TName{}, l);
    auto finalize    = luacpp_finalizer{[l, stack_depth] {
        lua_pop(l, stack_depth);
    }};

    return luacpp_get<T>(l, -1);
}

template <typename TName, typename T>
class lua_function;

template <typename TName, typename ReturnT, typename... ArgsT>
class lua_function<TName, ReturnT(ArgsT...)> {
public:
    lua_function(lua_State* il, TName, ReturnT(*)(ArgsT...)): l(il) {}

    template <bool IsVoid = std::is_same_v<ReturnT, void>>
    ReturnT operator()(ArgsT&&... args) const {
        auto stack_depth = luacpp_access(TName{}, l);
        auto finalize    = luacpp_finalizer{[l = this->l, &stack_depth] {
            lua_pop(l, stack_depth - 1);
        }};

        ((luacpp_push(l, args)), ...);

        if constexpr (IsVoid) {
            lua_call(l, int(sizeof...(ArgsT)), 0);
        }
        else {
            lua_call(l, int(sizeof...(ArgsT)), 1);
            ++stack_depth;
            return luacpp_get<ReturnT>(l, -1);
        }
    }

    constexpr TName name() const {
        return TName{};
    }

private:
    lua_State* l;
};

template <typename T, typename TName> requires std::is_function_v<T>
auto luacpp_extract(TName, lua_State* l) {
    using ptr = T*;
    return lua_function<TName, T>(l, TName{}, ptr{});
}

