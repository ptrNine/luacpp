#pragma once

#include "luacpp_details.hpp"
#include "luacpp_member_table.hpp"
#include "luacpp_assist_gen.hpp"

namespace luacpp {

namespace errors
{
    class init_error : public std::runtime_error {
    public:
        init_error(const std::string& msg): std::runtime_error("Failed to initialize luajit: " + msg) {}
    };
    class newstate_failed : public std::runtime_error {
    public:
        newstate_failed(): std::runtime_error("Failed to create lua state") {}
    };

    class cannot_open_file : public std::runtime_error {
    public:
        cannot_open_file(const std::string& filename): std::runtime_error("Cannot open lua script file: " + filename) {}
    };

    class syntax_error : public std::runtime_error {
    public:
        syntax_error(const std::string& msg): std::runtime_error(msg) {}
    };

    class memory_error : public std::runtime_error {
    public:
        memory_error(): std::runtime_error("lua memory error") {}
    };
} // namespace errors

struct lua_code {
    std::string code;
};

class luactx {
public:
    static auto luacpp_close(luactx* l) {
        return [l] {
            if (l->l) {
                lua_close(l->l);
                l->l = nullptr;
            }
        };
    }

    luactx(bool generate_assist = false): l(luaL_newstate()), generate_assist_file(generate_assist) {
        if (!l)
            throw errors::newstate_failed();

        auto guard = exception_guard{luacpp_close(this)};

        luaL_openlibs(l);

#ifdef WITH_LUAJIT
        if (!luaJIT_setmode(l, 0, LUAJIT_MODE_ENGINE | LUAJIT_MODE_ON))
            throw errors::init_error("enabling jit failed");
#endif

        register_usertypes();
    }

    luactx(lua_State* state, bool generate_assist = false): l(state), generate_assist_file(generate_assist) {
        register_usertypes();
    }

    luactx(const char* entry_file, bool generate_assist = false): luactx(generate_assist) {
        auto guard = exception_guard{luacpp_close(this)};
        load_and_call(entry_file);
    }

    luactx(const lua_code& code, bool generate_assist = false): luactx(generate_assist) {
        auto guard = exception_guard{luacpp_close(this)};
        load_and_call(code);
    }

    ~luactx() {
        //lua_gc(l, LUA_GCCOLLECT, 0);
        if (l)
            lua_close(l);
    }

    void load_and_call(const char* entry_file) {
        load(entry_file);
        call();
    }

    void load_and_call(const lua_code& code) {
        load(code);
        call();
    }

    void load(const char* entry_file) {
        switch (luaL_loadfile(l, entry_file)) {
        case LUA_ERRSYNTAX:
            throw errors::syntax_error(lua_tostring(l, -1));
        case LUA_ERRMEM:
            throw errors::memory_error();
        case LUA_ERRFILE:
            throw errors::cannot_open_file(entry_file);
        }
    }

    void load(const lua_code& code) {
        switch (luaL_loadstring(l, code.code.data())) {
        case LUA_ERRSYNTAX:
            throw errors::syntax_error(lua_tostring(l, -1));
        case LUA_ERRMEM:
            throw errors::memory_error();
        }
    }

    void call() {
        lua_call(l, 0, 0);
    }

    [[nodiscard]]
    lua_State* state() const {
        return l;
    }

    template <typename NameT, typename T>
    decltype(auto) provide(NameT, T&& value) {
        if (generate_assist_file)
            provide_assist<NameT, T>();

        return luaprovide(NameT{}, l, std::forward<T>(value));
    }

    template <typename NameT, typename F>
    auto provide_commutative_op(NameT, F&& member_function) {
        return overloaded{
            []<typename ReturnT, typename ClassT, typename... ArgsT>(luactx& l, ReturnT(ClassT::*f)(ArgsT...)){
                return l.provide_member<ClassT>(NameT{},
                                                [f](ClassT& it, ArgsT... args) { return (it.*f)(args...); },
                                                [f](ArgsT... args, ClassT& it) { return (it.*f)(args...); });
            },
            []<typename ReturnT, typename ClassT, typename... ArgsT>(
                    luactx& l, ReturnT(ClassT::*f)(ArgsT...) noexcept){
                return l.provide_member<ClassT>(NameT{},
                                                [f](ClassT& it, ArgsT... args) { return (it.*f)(args...); },
                                                [f](ArgsT... args, ClassT& it) { return (it.*f)(args...); });
            },
            []<typename ReturnT, typename ClassT, typename... ArgsT>(luactx& l, ReturnT(ClassT::*f)(ArgsT...) const){
                return l.provide_member<ClassT>(NameT{},
                                                [f](const ClassT& it, ArgsT... args) { return (it.*f)(args...); },
                                                [f](ArgsT... args, const ClassT& it) { return (it.*f)(args...); });
            },
            []<typename ReturnT, typename ClassT, typename... ArgsT>(
                    luactx& l, ReturnT(ClassT::*f)(ArgsT...) const noexcept) {
                return l.provide_member<ClassT>(NameT{},
                                                [f](const ClassT& it, ArgsT... args) { return (it.*f)(args...); },
                                                [f](ArgsT... args, const ClassT& it) { return (it.*f)(args...); });
            }
        }(*this, member_function);
    }

    template <typename NameT, typename F1, typename F2, typename... Fs>
    auto provide(NameT, F1&& function1, F2&& function2, Fs&&... functions) {
        if (generate_assist_file)
            provide_assist<NameT, F1, F2, Fs...>();

        return luaprovide_overloaded(
            NameT{}, l, std::forward<F1>(function1), std::forward<F2>(function2), std::forward<Fs>(functions)...);
    }

    template <typename UserType, typename NameT, typename T>
    decltype(auto) provide_member(NameT, T&& value) {
        if (generate_assist_file) {
            if constexpr (LuaFunctionLike<T>)
                provide_assist_for_member_function<UserType, NameT, T>();
            else
                provide_assist<decltype(type_registry::get_typespec<UserType>().lua_name().dot(NameT{})), T>();
        }

        return luaprovide(
            type_registry::get_typespec<UserType>().lua_name().dot(NameT{}), l, std::forward<T>(value));
    }

    template <typename UserType, typename NameT, typename F1, typename F2, typename... Fs>
    auto provide_member(NameT, F1&& function1, F2&& function2, Fs&&... functions) {
        if (generate_assist_file)
            provide_assist_for_member_function<UserType, NameT, F1, F2, Fs...>();

        return luaprovide_overloaded(type_registry::get_typespec<UserType>().lua_name().dot(NameT{}),
                                      l,
                                      std::forward<F1>(function1),
                                      std::forward<F2>(function2),
                                      std::forward<Fs>(functions)...);
    }

    template <typename UserType>
    void set_member_table(member_table<UserType> table) {
        if (generate_assist_file) {
            auto class_name = type_registry::get_typespec<UserType>().lua_name();
            for (auto& [field_name, _] : table) assist.field(std::string(class_name) + '.' + field_name);
        }

        provide_member<UserType>(LUA_TNAME("__index"), [this, table](const UserType& data, const std::string& field) {
            auto found_field = table.find(field);
            if (found_field != table.end())
                found_field->second.get(data, *this);
            else
                luaL_getmetafield(state(), -2, field.data());
            return placeholder{};
        });

        provide_member<UserType>(
            LUA_TNAME("__newindex"),
            [this, table = std::move(table)](UserType& data, const std::string& field, placeholder) {
                auto found_field = table.find(field);
                if (found_field != table.end()) {
                    if (!found_field->second.set)
                        throw errors::lua_access_error(std::string("the field '") + field + "' of object type " +
                                                       type_registry::get_typespec<UserType>().lua_name().data() +
                                                       " is private");
                    found_field->second.set(data, *this);
                }
                else
                    throw errors::lua_access_error(std::string("object of type '") +
                                                   type_registry::get_typespec<UserType>().lua_name().data() +
                                                   "' has no '" + field + "' field");
            });
    }

    template <typename T, typename NameT>
    decltype(auto) extract(NameT) {
        return luaextract<T>(NameT{}, l);
    }

    template <typename T>
    void push(T&& value) {
        luapush(l, std::forward<T>(value));
    }

    template <typename T>
    decltype(auto) get(int stack_idx) {
        return luaget<T>(l, stack_idx);
    }

    template <typename T>
    T pop() {
        auto res = luaget<T>(l, -1);
        lua_pop(l, 1);
        return res;
    }

    template <typename T>
    decltype(auto) get_new() {
        return get<T>(3);
    }

    template <typename T>
    void get_new(T& value) {
        value = get<T>(3);
    }

    template <typename T>
    void pop_discard(int count = 1) {
        lua_pop(l, count);
    }

    int top() {
        return lua_gettop(l);
    }

    void enable_assist_gen(bool value = true) {
        generate_assist_file = value;
    }

    std::string generate_assist() const {
        return assist.generate();
    }

    template <typename... Ts>
    void annotate_args(Ts&&... argument_names) {
        if (generate_assist_file)
            assist.annotate_args(std::forward<Ts>(argument_names)...);
    }

private:
    void register_usertypes() {
        tforeach<typespec_list<0>>([this](auto typespec) {
            using type = decltype(typespec.type());
            provide_member<type>(LUA_TNAME("__gc"), [](type* userdata) { userdata->~type(); });

            lua_getglobal(l, typespec.lua_name().data());
            lua_pushvalue(l, -1);
            lua_setfield(l, -2, "__index");
            lua_pop(l, 1);

            if (generate_assist_file)
                assist.field(type_registry::get_typespec<type>().lua_name(), true);

            if constexpr (requires { usertype_method_loader<type>(); })
                usertype_method_loader<type>()(*this);
        });
    }

    template <typename NameT, typename... Ts>
    void provide_assist() {
        if constexpr (NameT{} == LUA_TNAME("__gc") || NameT{} == LUA_TNAME("__index") ||
                      NameT{} == LUA_TNAME("__newindex"))
            return;

        auto push_assist = [this]<typename T>(details::lua_ttype<T>) {
            using type = std::decay_t<T>;

            if constexpr (LuaFunctionLike<type>) {
                using return_t       = typename details::lua_function_traits<type>::return_t;
                constexpr auto arity = details::lua_function_traits<type>::arity;
                if constexpr (type_registry::get_index<return_t>() != type_registry::no_index)
                    assist.function(
                        NameT{}, arity, std::string(type_registry::get_typespec<return_t>().lua_name()));
                else
                    assist.function(NameT{}, arity);
                return;
            }
            if constexpr (LuaMemberFunction<type>) {
                using return_t       = typename details::lua_member_function_traits<type>::return_t;
                constexpr auto arity = details::lua_member_function_traits<type>::arity;
                constexpr auto class_name =
                    type_registry::get_typespec<typename details::lua_member_function_traits<type>::class_t>()
                        .lua_name();

                if constexpr (type_registry::get_index<return_t>() != type_registry::no_index)
                    assist.member_function(class_name,
                                           NameT{},
                                           arity,
                                           std::string(type_registry::get_typespec<return_t>().lua_name()));
                else
                    assist.member_function(class_name, NameT{}, arity);
                return;
            }

            assist.field(NameT{});
        };

        (push_assist(details::lua_ttype<Ts>()), ...);
    }

    template <typename ClassT, typename NameT, typename... Ts>
    void provide_assist_for_member_function() {
        if constexpr (NameT{} == LUA_TNAME("__gc") || NameT{} == LUA_TNAME("__index") ||
                      NameT{} == LUA_TNAME("__newindex"))
            return;

        auto push_assist = [this]<typename T>(details::lua_ttype<T>) {
            using type                = std::decay_t<T>;
            using return_t            = typename details::lua_function_traits<type>::return_t;
            constexpr auto arity      = details::lua_function_traits<type>::arity;
            constexpr auto class_name = type_registry::get_typespec<std::decay_t<ClassT>>().lua_name();

            std::optional<std::string> result_metatable;
            if constexpr (type_registry::get_index<return_t>() != type_registry::no_index)
                result_metatable.emplace(type_registry::get_typespec<return_t>().lua_name());

            /* Check if the first argument has ClassT type */
            constexpr bool captured_self = std::is_same_v<
                std::decay_t<std::remove_pointer_t<decltype(details::lua_function_traits<T>::template arg_type<0>().type())>>,
                std::decay_t<ClassT>>;
            assist.member_function(
                class_name, NameT{}, arity - (captured_self ? 1U : 0U), result_metatable, captured_self);
        };
        (push_assist(details::lua_ttype<Ts>()), ...);
    }

private:
    lua_State* l;

    bool generate_assist_file = false;
    assist_gen assist;
};

} // namespace luacpp
