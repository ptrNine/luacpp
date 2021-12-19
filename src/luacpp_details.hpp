#pragma once

#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <optional>

#include "luacpp_lib.hpp"
#include "luacpp_usertype_registry.hpp"
#include "luacpp_utils.hpp"

namespace luacpp
{

template <typename T>
concept LuaNumber = (std::is_integral_v<T> || std::is_floating_point_v<T>)&&!std::same_as<T, bool>;

template <typename T>
concept LuaNumberOrRef = LuaNumber<std::decay_t<T>>;

template <typename T>
concept LuaStringLike = std::same_as<T, std::string> || std::same_as<T, std::string_view>;

template <typename T>
concept LuaStringLikeOrRef = LuaStringLike<std::decay_t<T>>;

template <typename T>
concept LuaTupleLike = requires {
    std::tuple_size<T>::value;
};

template <typename T>
concept LuaTupleLikeOrRef = LuaTupleLike<std::decay_t<T>>;

template <typename T>
concept LuaListLike = !LuaTupleLike<T> && !LuaStringLike<T> && requires(const T& v) {
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
concept LuaPushBackable = !LuaStringLike<T> && requires(T & v) {
    {v.push_back(std::declval<decltype(*v.begin())>())};
};

template <typename T>
concept LuaPushBackableOrRef = LuaPushBackable<std::decay_t<T>>;

template <typename T>
concept LuaStaticSettable = !LuaTupleLike<T> && !LuaStringLike<T> && !LuaPushBackable<T> && requires(T & v) {
    {v[0] = v[0]};
    { size(v) } -> std::convertible_to<size_t>;
};

template <typename T>
concept LuaStaticSettableOrRef = LuaStaticSettable<std::decay_t<T>>;

/* Forward declarations */
void luapush(lua_State* l, const LuaListLike auto& value);

void luapush(lua_State* l, const LuaTupleLike auto& value);

template <LuaPushBackableOrRef T>
auto luaget(lua_State* l, int idx);

template <LuaStaticSettableOrRef T>
auto luaget(lua_State* l, int idx);

template <LuaTupleLikeOrRef T>
auto luaget(lua_State* l, int idx);

template <typename T>
    requires LuaPushBackableOrRef<T> || LuaStaticSettableOrRef<T>
bool luacheck(lua_State* l, int idx);

template <LuaTupleLikeOrRef T>
bool luacheck(lua_State* l, int idx);

template <typename T>
    requires std::same_as<T, bool>
inline void luapush(lua_State* l, T value) {
    lua_pushboolean(l, value);
}

void luapush(lua_State* l, LuaNumber auto value) {
    lua_pushnumber(l, lua_Number(value));
}

template <size_t S>
void luapush(lua_State* l, const char (&value)[S]) {
    lua_pushlstring(l, value, S - 1);
}

inline void luapush(lua_State* l, const char* value) {
    if (!value)
        lua_pushnil(l);
    else
        lua_pushstring(l, value);
}

struct placeholder {};

inline void luapush(lua_State*, placeholder) {}

inline void luapush(lua_State* l, std::nullptr_t) {
    lua_pushnil(l);
}

void luapush(lua_State* l, const LuaStringLike auto& value) {
    lua_pushlstring(l, value.data(), value.size());
}

void luapush(lua_State* l, const auto* pointer_value) {
    if (!pointer_value)
        lua_pushnil(l);
    else
        luapush(l, *pointer_value);
}

void luapush(lua_State* l, const LuaListLike auto& value) {
    auto b = begin(value);
    auto e = end(value);

    if constexpr (LuaArrayLike<std::decay_t<decltype(value)>>)
        lua_createtable(l, int(e - b), 0);
    else
        lua_createtable(l, 0, 0);

    for (uint32_t i = 1; b != e; ++i, ++b) {
        lua_pushnumber(l, i);
        luapush(l, *b);
        lua_settable(l, -3);
    }
}

void luapush(lua_State* l, const LuaTupleLike auto& value) {
    lua_createtable(l, int(std::tuple_size_v<std::decay_t<decltype(value)>>), 0);
    []<size_t... Idxs>([[maybe_unused]] lua_State * l, decltype(value) value, std::index_sequence<Idxs...>) {
        ((lua_pushnumber(l, int(Idxs) + 1), luapush(l, std::get<Idxs>(value)), lua_settable(l, -3)), ...);
    }
    (l, value, std::make_index_sequence<std::tuple_size_v<std::decay_t<decltype(value)>>>());
}

template <typename T> requires LuaRegisteredType<std::decay_t<T>>
std::decay_t<T>& luapush(lua_State* l, T&& value) {
    void*    p          = lua_newuserdata(l, sizeof(uint64_t) + sizeof(value));
    uint64_t type_index = type_registry::get_index<std::decay_t<T>>();
    std::memcpy(p, &type_index, sizeof(type_index));
    void* data = reinterpret_cast<char*>(p) + sizeof(uint64_t);      // NOLINT
    auto  ptr  = new (data) std::decay_t<T>(std::forward<T>(value)); // NOLINT
    lua_getglobal(l, type_registry::get_typespec<std::decay_t<T>>().lua_name().data());
    lua_setmetatable(l, -2);

    return *ptr;
}

template <typename T>
    requires std::same_as<T, lua_CFunction>
inline void luapush(lua_State* l, T value) {
    lua_pushcfunction(l, value);
}

namespace errors
{
    class cast_error : public std::runtime_error {
    public:
        cast_error(lua_State* l, int stackidx, const std::string& msg, const std::string& pf):
            std::runtime_error(std::string("luacpp: cast from ") + lua_typename(l, lua_type(l, stackidx)) +
                               " failed (" + msg + "). PRETTY_FUNCTION:\n" + pf) {}
    };

    class call_cpp_error : public std::runtime_error {
    public:
        call_cpp_error(const std::string& msg): std::runtime_error("luacpp: call c++ function failed: " + msg) {}
    };
} // namespace errors

struct anycasted {
public:
    template <typename T>
    operator T() const {
        return T{};
    }
};

template <typename T>
    requires std::same_as<std::decay_t<T>, placeholder>
auto luaget(lua_State*, int) {
    return placeholder{};
}

template <typename T>
    requires std::same_as<std::decay_t<T>, placeholder>
bool luacheck(lua_State*, int) {
    return true;
}

template <typename T>
    requires std::same_as<std::decay_t<T>, bool>
auto luaget(lua_State* l, int idx) {
    if (lua_isboolean(l, idx))
        return lua_toboolean(l, idx);
    else
        throw errors::cast_error(l, idx, "this type can't be casted to C++ bool", __PRETTY_FUNCTION__);
}

template <typename T>
    requires std::same_as<std::decay_t<T>, bool>
bool luacheck(lua_State* l, int idx) {
    return lua_isboolean(l, idx);
}

template <LuaNumberOrRef T>
auto luaget(lua_State* l, int idx) {
    if (lua_isnumber(l, idx))
        return std::decay_t<T>(lua_tonumber(l, idx));
    else
        throw errors::cast_error(l, idx, "this type can't be casted to C++ number", __PRETTY_FUNCTION__);
}

template <LuaNumberOrRef T>
bool luacheck(lua_State* l, int idx) {
    return lua_isnumber(l, idx);
}

template <typename T>
    requires std::same_as<std::decay_t<T>, std::string>
auto luaget(lua_State* l, int idx) {
    if (lua_isstring(l, idx)) {
        size_t len;
        auto   str = lua_tolstring(l, idx, &len);
        return std::string(str, len);
    }
    else
        throw errors::cast_error(l, idx, "this type can't be casted to C++ std::string", __PRETTY_FUNCTION__);
}

template <typename T>
    requires std::same_as<std::decay_t<T>, std::string>
bool luacheck(lua_State* l, int idx) {
    /* Disable implicit number-to-string casting for proper overload resolution */
    return lua_isstring(l, idx) && !lua_isnumber(l, idx);
}

namespace details
{
    template <typename T>
    auto _array_getnext(lua_State* l, int index_check) {
        if (!lua_isnumber(l, -2))
            throw errors::cast_error(l, -3, "some key of lua table is not a number", __PRETTY_FUNCTION__);

        auto idx = int(lua_tonumber(l, -2));
        if (idx != index_check)
            throw errors::cast_error(
                l, idx, "some index key of lua table violates continuous order", __PRETTY_FUNCTION__);

        return luaget<T>(l, -1);
    }

    template <typename T>
    bool _array_check(lua_State* l, int index_check) {
        return lua_isnumber(l, -2) && int(lua_tonumber(l, -2)) == index_check && luacheck<T>(l, -1);
    }
} // namespace details

template <LuaPushBackableOrRef T>
auto luaget(lua_State* l, int idx) {
    if (!lua_istable(l, idx))
        throw errors::cast_error(l, idx, "this type can't be casted to C++ array-like container", __PRETTY_FUNCTION__);
    std::decay_t<T> result;
    using value_t = std::decay_t<decltype(*result.begin())>;

    /* For using relative stack pos */
    lua_pushvalue(l, idx);
    lua_pushnil(l);

    auto guard = exception_guard{[l] {
        lua_pop(l, 3);
    }};

    int index_check = 1;
    while (lua_next(l, -2)) {
        result.push_back(details::_array_getnext<value_t>(l, index_check));
        ++index_check;
        lua_pop(l, 1);
    }

    lua_pop(l, 1);

    return result;
}

template <typename T>
    requires LuaPushBackableOrRef<T> || LuaStaticSettableOrRef<T>
bool luacheck(lua_State* l, int idx) {
    if (!lua_istable(l, idx))
        return false;

    if constexpr (LuaStaticSettableOrRef<T>) {
        if (lua_objlen(l, idx) != size(T()))
            return false;
    }

    using value_t = std::decay_t<decltype(*std::declval<T>().begin())>;

    lua_pushvalue(l, idx);
    lua_pushnil(l);

    bool result      = true;
    int  index_check = 1;
    while (result && lua_next(l, -2)) {
        if (!details::_array_check<value_t>(l, index_check))
            result = false;
        ++index_check;
        lua_pop(l, 1);
    }
    lua_pop(l, 1);

    return result;
}

template <LuaStaticSettableOrRef T>
auto luaget(lua_State* l, int idx) {
    if (!lua_istable(l, idx))
        throw errors::cast_error(l, idx, "this type can't be casted to C++ array-like container", __PRETTY_FUNCTION__);
    std::decay_t<T> result;
    if (lua_objlen(l, idx) != size(result))
        throw errors::cast_error(l, idx, "array lengths do not match", __PRETTY_FUNCTION__);

    using value_t = std::decay_t<decltype(result[0])>;

    /* For using relative stack pos */
    lua_pushvalue(l, idx);
    lua_pushnil(l);

    auto guard = exception_guard{[l] {
        lua_pop(l, 3);
    }};

    int index_check = 1;
    while (lua_next(l, -2)) {
        result[size_t(index_check - 1)] = details::_array_getnext<value_t>(l, index_check);
        ++index_check;
        lua_pop(l, 1);
    }

    lua_pop(l, 1);

    return result;
}

namespace luacppdetails
{
    template <size_t N>
    struct nttp_tag {};
} // namespace luacppdetails

template <LuaTupleLikeOrRef T>
auto luaget(lua_State* l, int idx) {
    using type = std::decay_t<T>;

    if (!lua_istable(l, idx))
        throw errors::cast_error(l, idx, "this type can't be casted to C++ tuple-like type", __PRETTY_FUNCTION__);
    type result;
    if (lua_objlen(l, idx) != std::tuple_size_v<type>)
        throw errors::cast_error(l, idx, "lua table and tuple lengths do not match", __PRETTY_FUNCTION__);

    /* For using relative stack pos */
    lua_pushvalue(l, idx);
    lua_pushnil(l);

    auto guard = exception_guard{[l] {
        lua_pop(l, 3);
    }};

    static constexpr auto getall =
        []<size_t... Idxs>(type & v, [[maybe_unused]] lua_State * l, std::index_sequence<Idxs...>) {
        [[maybe_unused]] static constexpr auto op =
            []<size_t idx>(type& v, lua_State* l, luacppdetails::nttp_tag<idx>) {
                lua_next(l, -2);
                std::get<idx>(v) = details::_array_getnext<std::tuple_element_t<idx, type>>(l, int(idx + 1));
                lua_pop(l, 1);
            };
        ((op(v, l, luacppdetails::nttp_tag<Idxs>{})), ...);
    };

    getall(result, l, std::make_index_sequence<std::tuple_size_v<type>>());
    lua_pop(l, 2);

    return result;
}

template <LuaTupleLikeOrRef T>
bool luacheck(lua_State* l, int idx) {
    using type = std::decay_t<T>;

    if (!lua_istable(l, idx))
        return false;

    if (lua_objlen(l, idx) != std::tuple_size_v<type>)
        return false;

    lua_pushvalue(l, idx);
    lua_pushnil(l);

    auto guard = exception_guard{[l] {
        lua_pop(l, 3);
    }};

    bool result = true;

    static constexpr auto checkall =
        []<size_t... Idxs>([[maybe_unused]] lua_State * l, bool& result, std::index_sequence<Idxs...>) {
        [[maybe_unused]] static constexpr auto op =
            []<size_t idx>(lua_State* l, bool& result, luacppdetails::nttp_tag<idx>) {
                lua_next(l, -2);
                if (result)
                    if (!details::_array_check<std::tuple_element_t<idx, type>>(l, int(idx + 1)))
                        result = false;
                lua_pop(l, 1);
            };
        ((op(l, result, luacppdetails::nttp_tag<Idxs>{})), ...);
    };

    checkall(l, result, std::make_index_sequence<std::tuple_size_v<type>>());
    lua_pop(l, 2);

    return result;
}

/* This is the only thing that can return references */
template <LuaRegisteredTypeRefOrPtr T>
T luaget(lua_State* l, int idx) {
    if (!lua_isuserdata(l, idx))
        throw errors::cast_error(l, idx, "this type can't be casted to C++ userdata type", __PRETTY_FUNCTION__);

    size_t           objlen  = lua_objlen(l, idx);
    constexpr size_t reallen = sizeof(uint64_t) + sizeof(std::remove_pointer_t<T>);
    if (objlen != reallen)
        throw errors::cast_error(l,
                                 idx,
                                 "userdata has invalid length (" + std::to_string(objlen) + " but should be " +
                                     std::to_string(reallen) + ") ",
                                 __PRETTY_FUNCTION__);

    /* TODO: check address alignment for this */
    void*    ptr = lua_touserdata(l, idx);
    uint64_t type_index;
    std::memcpy(&type_index, ptr, sizeof(type_index));

    using pointer_t = std::conditional_t<std::is_pointer_v<T>, T, std::decay_t<T>*>;

    if (type_registry::get_index<std::remove_pointer_t<pointer_t>>() != type_index)
        throw errors::cast_error(
            l,
            idx,
            "userdata is not a " +
                std::string(type_registry::get_typespec<std::remove_pointer_t<pointer_t>>().lua_name().data()),
            __PRETTY_FUNCTION__);

    auto data = reinterpret_cast<pointer_t>(reinterpret_cast<char*>(ptr) + sizeof(uint64_t)); // NOLINT

    if constexpr (std::is_pointer_v<T>)
        return data;
    else
        return *data;
}

template <LuaRegisteredTypeRefOrPtr T>
bool luacheck(lua_State* l, int idx) {
    if (!lua_isuserdata(l, idx) || lua_objlen(l, idx) != sizeof(uint64_t) + sizeof(std::remove_pointer_t<T>))
        return false;

    void*    ptr = lua_touserdata(l, idx);
    uint64_t type_index;
    std::memcpy(&type_index, ptr, sizeof(type_index));

    return type_registry::get_index<std::decay_t<std::remove_pointer_t<T>>>() == type_index;
}

template <typename F, typename RF, uint64_t UniqId>
struct func_storage {
    static func_storage& instance() {
        static func_storage inst;
        return inst;
    }

    int call(lua_State* state) const {
        return f(state, *rf);
    }

    F                 f;
    std::optional<RF> rf;
};

template <typename F, uint64_t UniqId, typename... RFs>
struct overloaded_func_storage {
    static overloaded_func_storage& instance() {
        static overloaded_func_storage inst;
        return inst;
    }

    int call(lua_State* state) const {
        return std::apply(f, std::tuple_cat(std::tuple{state}, *rfs));
    }

    F                                 f;
    std::optional<std::tuple<RFs...>> rfs;
};

template <typename F, typename RF, uint64_t UniqId>
struct wrapped_function {
    static int call(lua_State* state) {
        return func_storage<F, RF, UniqId>::instance().call(state);
    }
};

template <typename F, uint64_t UniqId, typename... RFs>
struct wrapped_overloaded_function {
    static int call(lua_State* state) {
        return overloaded_func_storage<F, UniqId, RFs...>::instance().call(state);
    }
};

template <typename FuncT, typename ReturnT, typename... ArgsT>
struct native_function {
    FuncT function;
};

namespace details
{
    template <typename ReturnT, typename... ArgsT>
    int _function_call(lua_State* l, auto&& function) {
        auto lua_args_count = lua_gettop(l);

        if (size_t(lua_args_count) != sizeof...(ArgsT))
            throw errors::call_cpp_error("arguments count mismatch (lua called with " + std::to_string(lua_args_count) +
                                         ", but C++ function defined with " + std::to_string(sizeof...(ArgsT)) +
                                         " arguments)");

        if constexpr (std::is_same_v<ReturnT, void>) {
            []<size_t... Idxs>([[maybe_unused]] lua_State * l, auto&& function, std::index_sequence<Idxs...>) {
                return function(luaget<ArgsT>(l, int(Idxs + 1))...);
            }
            (l, function, std::make_index_sequence<sizeof...(ArgsT)>());
            return 0;
        }
        else {
            luapush(
                l, []<size_t... Idxs>([[maybe_unused]] lua_State * l, auto&& function, std::index_sequence<Idxs...>) {
                    return function(luaget<ArgsT>(l, int(Idxs + 1))...);
                }(l, function, std::make_index_sequence<sizeof...(ArgsT)>()));
            return 1;
        }
    }

    template <typename...>
    struct _typelist {};

    template <typename ReturnT, typename... ArgsT>
    int _function_call_typelist(lua_State* l, auto&& function, _typelist<ArgsT...>) {
        return _function_call<ReturnT, ArgsT...>(l, function);
    }

    template <uint64_t UniqId, typename F, typename ReturnT, typename... ArgsT>
    lua_CFunction _wrap_function(native_function<F, ReturnT, ArgsT...>&& function) {
        auto func = [](lua_State* l, auto&& function) {
            return _function_call<ReturnT, ArgsT...>(l, std::forward<decltype(function)>(function));
        };

        auto& fstorage = func_storage<decltype(func), decltype(function.function), UniqId>::instance();
        fstorage.f     = std::move(func);
        fstorage.rf.emplace(std::move(function.function));

        return &wrapped_function<decltype(func), decltype(function.function), UniqId>{}.call;
    }

    template <uint64_t UniqId, typename F, typename ClassT, typename ReturnT, typename... ArgsT>
    auto _wrap_functional(ReturnT (ClassT::*)(ArgsT...) const, F&& function) {
        return _wrap_function<UniqId>(native_function<std::decay_t<decltype(function)>, ReturnT, ArgsT...>{function});
    }

    template <typename T>
    concept LuaFunctional = requires {
        &T::operator();
    };
} // namespace details

template <typename T>
concept LuaFunctionLike = details::LuaFunctional<T> || std::is_function_v<T>;

template <uint64_t UniqId, typename ReturnT, typename... ArgsT>
auto wrap_function(ReturnT (*function)(ArgsT...)) {
    return details::_wrap_function<UniqId>(native_function<decltype(function), ReturnT, ArgsT...>{function});
}

template <uint64_t UniqId, details::LuaFunctional F>
auto wrap_function(F&& function) {
    return details::_wrap_functional<UniqId>(decltype(&F::operator()){}, function);
}

namespace details
{
    template <typename T>
    struct lua_member_function : std::false_type {};

    template <typename ReturnT, typename ClassT, typename... ArgsT>
    struct lua_member_function<ReturnT (ClassT::*)(ArgsT...)> : std::true_type {
        static constexpr bool is_noexcept = false;
        static constexpr bool is_const    = false;
        using class_t                     = ClassT;
    };

    template <typename ReturnT, typename ClassT, typename... ArgsT>
    struct lua_member_function<ReturnT (ClassT::*)(ArgsT...) const> : std::true_type {
        static constexpr bool is_noexcept = false;
        static constexpr bool is_const    = true;
        using class_t                     = ClassT;
    };

    template <typename ReturnT, typename ClassT, typename... ArgsT>
    struct lua_member_function<ReturnT (ClassT::*)(ArgsT...) noexcept> : std::true_type {
        static constexpr bool is_noexcept = true;
        static constexpr bool is_const    = false;
        using class_t                     = ClassT;
    };

    template <typename ReturnT, typename ClassT, typename... ArgsT>
    struct lua_member_function<ReturnT (ClassT::*)(ArgsT...) const noexcept> : std::true_type {
        static constexpr bool is_noexcept = true;
        static constexpr bool is_const    = true;
        using class_t                     = ClassT;
    };

    template <typename F>
    struct lua_function_traits;

    template <typename T>
    struct lua_ttype {
        T type() const;
    };

    struct lua_noarg {};

    template <typename ReturnT, typename... ArgsT>
    struct lua_function_traits<ReturnT (*)(ArgsT...)> {
        static constexpr size_t arity = sizeof...(ArgsT);
        using return_t                = ReturnT;

        template <size_t N>
        static constexpr auto arg_type() {
            if constexpr (N >= sizeof...(ArgsT))
                return lua_ttype<lua_noarg>{};
            else
                return lua_ttype<std::tuple_element_t<N, std::tuple<ArgsT...>>>{};
        }

        using arg_types = _typelist<ArgsT...>;
    };

    template <typename ReturnT, typename ClassT, typename... ArgsT>
    struct lua_function_traits<ReturnT (ClassT::*)(ArgsT...) const> : lua_function_traits<ReturnT (*)(ArgsT...)> {};

    template <typename F>
        requires details::LuaFunctional<F>
    struct lua_function_traits<F> : lua_function_traits<decltype(&F::operator())> {
    };

    template <typename T>
    struct lua_member_function_traits;

    template <typename ReturnT, typename ClassT, typename... ArgsT>
    struct lua_member_function_traits<ReturnT (ClassT::*)(ArgsT...)> : lua_function_traits<ReturnT (*)(ArgsT...)> {
        using class_t                     = ClassT;
        static constexpr bool is_const    = false;
        static constexpr bool is_noexcept = false;
    };
    template <typename ReturnT, typename ClassT, typename... ArgsT>
    struct lua_member_function_traits<ReturnT (ClassT::*)(ArgsT...) const>
        : lua_function_traits<ReturnT (*)(ArgsT...)> {
        using class_t                     = ClassT;
        static constexpr bool is_const    = true;
        static constexpr bool is_noexcept = false;
    };
    template <typename ReturnT, typename ClassT, typename... ArgsT>
    struct lua_member_function_traits<ReturnT (ClassT::*)(ArgsT...) noexcept>
        : lua_function_traits<ReturnT (*)(ArgsT...)> {
        using class_t                     = ClassT;
        static constexpr bool is_const    = false;
        static constexpr bool is_noexcept = true;
    };
    template <typename ReturnT, typename ClassT, typename... ArgsT>
    struct lua_member_function_traits<ReturnT (ClassT::*)(ArgsT...) const noexcept>
        : lua_function_traits<ReturnT (*)(ArgsT...)> {
        using class_t                     = ClassT;
        static constexpr bool is_const    = true;
        static constexpr bool is_noexcept = true;
    };

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

    template <typename T, typename... Ts>
    struct lua_first_type {
        using type = T;
    };

    template <typename T, typename... Ts>
    static constexpr bool lua_same_types = (std::same_as<T, Ts> && ... && true);

    template <typename... CheckT>
    struct lua_type_list {};

    template <size_t arg_number, bool enable_call, typename F, typename... Fs>
        requires(lua_function_traits<F>::arity == 0)
    int lua_overloaded_dispatch_by_arg_types(lua_State* l, F&& function, Fs&&...) {
        return _function_call_typelist<typename lua_function_traits<F>::return_t>(
            l, std::forward<F>(function), typename lua_function_traits<F>::arg_types{});
    }

    template <size_t arg_number, bool enable_call, typename F, typename... Fs>
    int lua_overloaded_dispatch_by_arg_types(lua_State* l, F&& function, Fs&&... functions) {
        using arg_type = decltype(lua_function_traits<F>::template arg_type<arg_number>().type());
        constexpr auto same_arg_type =
            lua_same_types<arg_type, decltype(lua_function_traits<Fs>::template arg_type<arg_number>().type())...>;

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
            if (luacheck<arg_type>(l, int(arg_number + 1))) {
                if constexpr (arg_number + 1 < max_args_count) {
                    int rc = lua_overloaded_dispatch_by_arg_types<arg_number + 1, false>(
                        l, std::forward<F>(function), std::forward<Fs>(functions)...);
                    if (rc != -1)
                        return rc;
                }
                return _function_call_typelist<typename lua_function_traits<F>::return_t>(
                    l, std::forward<F>(function), typename lua_function_traits<F>::arg_types{});
            }

            /* Take next function if it exists */
            if constexpr (sizeof...(functions) > 0) {
                return lua_overloaded_dispatch_by_arg_types<arg_number, enable_call>(l, std::forward<Fs>(functions)...);
            }
            else {
                return -1;
            }
        }
    }

    template <size_t CurrentArity = 0, size_t MaxArity, typename A, typename... Fs>
    int lua_args_count_recursive_dispatch(size_t args_count, A&& acceptor, Fs&&... functions) {
        if (args_count == CurrentArity) {
            return acceptor(
                (lua_arity_fold<Fs, CurrentArity>(std::forward<Fs>(functions)) + ... + lua_fold_element<>{}).functions);
        }
        else {
            if constexpr (CurrentArity + 1 <= MaxArity)
                return lua_args_count_recursive_dispatch<CurrentArity + 1, MaxArity, A, Fs...>(
                    args_count, std::forward<A>(acceptor), std::forward<Fs>(functions)...);
        }
        return -1;
    }

    template <typename... Fs>
    int lua_overloaded_call_dispatch(size_t args_count, lua_State* l, Fs&&... functions) {
        /* For the first try dispatch by arguments count.
         * Do this if arity differs only.
         */
        if constexpr (!details::lua_same_arity<details::lua_function_traits<Fs>::arity...>) {
            return details::lua_args_count_recursive_dispatch<0, details::lua_max_arity<Fs...>()>(
                args_count,
                [ args_count, l ]<typename... Fs2>(std::tuple<Fs2...> func_tuple) {
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
             * We should use the luacheck for every argument with different type
             */
            return lua_overloaded_dispatch_by_arg_types<0, true>(l, std::forward<Fs>(functions)...);
        }
    }

    template <typename... Fs>
    int lua_overloaded_call_dispatch_entry(lua_State* l, Fs&&... functions) {
        auto args_count = size_t(lua_gettop(l));
        if (((size_t(args_count) == details::lua_function_traits<Fs>::arity) || ... || false)) {
            int rc = lua_overloaded_call_dispatch(args_count, l, std::forward<Fs>(functions)...);
            if (rc == -1)
                throw errors::call_cpp_error("no matched overloaded function");
            return rc;
        }
        else
            throw errors::call_cpp_error("no matched overloaded function (cannot call with " +
                                         std::to_string(args_count) + " arguments)");
    }
} // namespace details

template <typename T>
concept LuaMemberFunction = details::lua_member_function<T>::value;

template <uint64_t UniqId, typename... Fs>
auto wrap_overloaded_functions(Fs&&... functions) {
    auto  func         = &details::lua_overloaded_call_dispatch_entry<Fs...>;
    auto& func_storage = overloaded_func_storage<decltype(func), UniqId, std::decay_t<Fs>...>::instance();
    func_storage.f     = std::move(func);
    func_storage.rfs.emplace(std::forward_as_tuple(std::forward<Fs>(functions)...));

    return &wrapped_overloaded_function<decltype(func), UniqId, std::decay_t<Fs>...>{}.call;
}

namespace details
{
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
            auto pop_on_scope_exit = finalizer{[&] {
                lua_pop(l, 1);
            }};
            return _lua_recursive_provide_namespaced_value(dotsplit.right(), l, value);
        }
        else {
            static_assert(name.size() > 0, "Attempt to declare lua function with empty name");
            lua_pushstring(l, name.data());

            auto rawset_on_scope_exit = finalizer{[&] {
                lua_rawset(l, -3);
            }};

            return luapush(l, value);
        }
    }
} // namespace details

template <typename T, typename TName>
decltype(auto) luaprovide(TName name, lua_State* l, T&& value) {
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

        auto pop_on_scope_exit = finalizer{[&] {
            lua_pop(l, 1);
        }};

        return details::_lua_recursive_provide_namespaced_value(dotsplit.right(), l, value);
    }
    else {
        static_assert(name.size() > 0, "Attempt to declare lua function with empty name");
        auto setglobal = finalizer{[&] {
            lua_setglobal(l, name.data());
        }};
        return luapush(l, value);
    }
}

template <LuaFunctionLike F, typename TName>
lua_CFunction luaprovide(TName name, lua_State* l, F&& function) {
    constexpr auto hash     = name.hash();
    auto           lua_func = wrap_function<hash>(std::forward<F>(function));
    luaprovide(name, l, lua_func);
    return lua_func;
}

template <typename TName, LuaFunctionLike... Fs>
lua_CFunction luaprovide_overloaded(TName name, lua_State* l, Fs&&... functions) {
    constexpr auto hash     = name.hash();
    auto           lua_func = wrap_overloaded_functions<hash>(std::forward<Fs>(functions)...);
    luaprovide(name, l, lua_func);
    return lua_func;
}

namespace details
{
    template <bool Noexcept, bool Const, typename ReturnT, typename ClassT, typename... ArgsT>
    struct member_wrapper {
        member_wrapper() = default;

        template <typename T>
        member_wrapper(T ifunc): f(ifunc) {}

        ReturnT operator()(ClassT& it, ArgsT... args) const {
            return (it.*f)(args...);
        }
        std::conditional_t<
            Const,
            std::conditional_t<Noexcept,
                               ReturnT (ClassT::*)(ArgsT...) const noexcept,
                               ReturnT (ClassT::*)(ArgsT...) const>,
            std::conditional_t<Noexcept, ReturnT (ClassT::*)(ArgsT...) noexcept, ReturnT (ClassT::*)(ArgsT...)>>
            f;
    };

    template <typename ReturnT, typename ClassT, typename... ArgsT>
    member_wrapper(ReturnT (ClassT::*)(ArgsT...)) -> member_wrapper<false, false, ReturnT, ClassT, ArgsT...>;

    template <typename ReturnT, typename ClassT, typename... ArgsT>
    member_wrapper(ReturnT (ClassT::*)(ArgsT...) const) -> member_wrapper<false, true, ReturnT, ClassT, ArgsT...>;

    template <typename ReturnT, typename ClassT, typename... ArgsT>
    member_wrapper(ReturnT (ClassT::*)(ArgsT...) noexcept) -> member_wrapper<true, false, ReturnT, ClassT, ArgsT...>;

    template <typename ReturnT, typename ClassT, typename... ArgsT>
    member_wrapper(ReturnT (ClassT::*)(ArgsT...) const noexcept)
        -> member_wrapper<true, true, ReturnT, ClassT, ArgsT...>;

    template <bool Noexcept,
              bool Const,
              typename F,
              typename ReturnT,
              typename ClassT,
              typename TName,
              typename... ArgsT>
    lua_CFunction _luaprovide_member_function(TName name, lua_State* l, F member_function) {
        constexpr auto typespec = type_registry::get_typespec<ClassT>();
        constexpr auto fullname = typespec.lua_name().dot(name);
        constexpr auto hash     = fullname.hash();
        auto           lua_func = wrap_function<hash>(member_wrapper{member_function});
        luaprovide(fullname, l, lua_func);
        return lua_func;
    }

    template <typename TName, typename... Fs>
        requires lua_same_types<typename lua_member_function<Fs>::class_t...>
    lua_CFunction _luaprovide_overloaded_member_function(TName name, lua_State* l, Fs... functions) {
        constexpr auto typespec = type_registry::get_typespec<
            typename lua_first_type<typename lua_member_function<Fs>::class_t...>::type>();
        constexpr auto fullname = typespec.lua_name().dot(name);
        constexpr auto hash     = fullname.hash();
        auto           lua_func = wrap_overloaded_functions<hash>(member_wrapper{functions}...);
        luaprovide(fullname, l, lua_func);
        return lua_func;
    }

} // namespace details

/* Handle const*noexcept
 * Is there a better way?..
 */

template <typename ReturnT, typename ClassT, typename TName, typename... ArgsT>
lua_CFunction luaprovide(TName name, lua_State* l, ReturnT (ClassT::*member_function)(ArgsT...)) {
    return details::
        _luaprovide_member_function<false, false, decltype(member_function), ReturnT, ClassT, TName, ArgsT...>(
            name, l, member_function);
}

template <typename ReturnT, typename ClassT, typename TName, typename... ArgsT>
lua_CFunction luaprovide(TName name, lua_State* l, ReturnT (ClassT::*member_function)(ArgsT...) const) {
    return details::
        _luaprovide_member_function<false, true, decltype(member_function), ReturnT, ClassT, TName, ArgsT...>(
            name, l, member_function);
}

template <typename ReturnT, typename ClassT, typename TName, typename... ArgsT>
lua_CFunction luaprovide(TName name, lua_State* l, ReturnT (ClassT::*member_function)(ArgsT...) noexcept) {
    return details::
        _luaprovide_member_function<true, false, decltype(member_function), ReturnT, ClassT, TName, ArgsT...>(
            name, l, member_function);
}

template <typename ReturnT, typename ClassT, typename TName, typename... ArgsT>
lua_CFunction luaprovide(TName name, lua_State* l, ReturnT (ClassT::*member_function)(ArgsT...) const noexcept) {
    return details::
        _luaprovide_member_function<true, true, decltype(member_function), ReturnT, ClassT, TName, ArgsT...>(
            name, l, member_function);
}

template <typename TName, LuaMemberFunction... Fs>
lua_CFunction luaprovide_overloaded(TName name, lua_State* l, Fs... functions) {
    return details::_luaprovide_overloaded_member_function(name, l, functions...);
}

namespace errors
{
    class lua_access_error : public std::runtime_error {
    public:
        lua_access_error(const std::string& msg): std::runtime_error("luacpp: " + msg) {}
    };
} // namespace errors

template <typename TName>
int luaaccess(TName name, lua_State* l, int stack_depth = 0) {
    constexpr auto dotsplit = name.template divide_by<'.'>();

    auto guard = exception_guard{[&] {
        lua_pop(l, stack_depth);
    }};

    if constexpr (dotsplit) {
        static_assert(dotsplit.left().size() > 0, "Attempt to access lua variable with empty name");

        ++stack_depth;
        lua_getfield(l, stack_depth == 1 ? LUA_GLOBALSINDEX : -1, dotsplit.left().data());
        guard.dismiss();
        return luaaccess(dotsplit.right(), l, stack_depth);
    }
    else {
        static_assert(name.size() > 0, "Attempt to access lua variable with empty name");
        ++stack_depth;
        lua_getfield(l, stack_depth == 1 ? LUA_GLOBALSINDEX : -1, name.data());
        return stack_depth;
    }
}

template <typename T, typename TName>
decltype(auto) luaextract(TName, lua_State* l) {
    int  stack_depth = luaaccess(TName{}, l);
    auto finalize    = finalizer{[l, stack_depth] {
        lua_pop(l, stack_depth);
    }};

    return luaget<T>(l, -1);
}

template <typename TName, typename T>
class lua_function;

template <typename TName, typename ReturnT, typename... ArgsT>
class lua_function<TName, ReturnT(ArgsT...)> {
public:
    lua_function(lua_State* il, TName, ReturnT (*)(ArgsT...)): l(il) {}

    template <bool IsVoid = std::is_same_v<ReturnT, void>>
    ReturnT operator()(ArgsT&&... args) const {
        auto stack_depth = luaaccess(TName{}, l);
        auto finalize    = finalizer{[l = this->l, &stack_depth] {
            lua_pop(l, stack_depth - 1);
        }};

        ((luapush(l, args)), ...);

        if constexpr (IsVoid) {
            luacall(l, int(sizeof...(ArgsT)), 0);
        }
        else {
            luacall(l, int(sizeof...(ArgsT)), 1);
            ++stack_depth;
            return luaget<ReturnT>(l, -1);
        }
    }

    constexpr TName name() const {
        return TName{};
    }

private:
    lua_State* l;
};

template <typename T, typename TName>
    requires std::is_function_v<T>
auto luaextract(TName, lua_State* l) {
    using ptr = T*;
    return lua_function<TName, T>(l, TName{}, ptr{});
}

} // namespace luacpp
