#pragma once

#include "luawrap.hpp"
#include "lua_member_table.hpp"
#include "lua_assist_gen.hpp"

class luactx_newstate_failed : public std::runtime_error {
public:
    luactx_newstate_failed(): std::runtime_error("Failed to create lua state") {}
};

class luactx_cannot_open_file : public std::runtime_error {
public:
    luactx_cannot_open_file(const std::string& filename):
        std::runtime_error("Cannot open lua script file: " + filename) {}
};

class luactx_syntax_error : public std::runtime_error {
public:
    luactx_syntax_error(const std::string& msg): std::runtime_error(msg) {}
};

class luactx_memory_error : public std::runtime_error {
public:
    luactx_memory_error(): std::runtime_error("lua memory error") {}
};

struct lua_code {
    std::string code;
};

class luactx {
public:
    luactx(bool generate_assist = false): l(luaL_newstate()), generate_assist_file(generate_assist) {
        if (!l)
            throw luactx_newstate_failed();

        luaL_openlibs(l);

        //lua_atpanic(l, &panic_wrapper);

        register_usertypes();
    }

    //static int panic_wrapper(lua_State* l) {
    //    std::cerr << lua_tostring(l, -1) << std::endl;
    //    std::abort();
    //}

    luactx(const char* entry_file, bool generate_assist = false): luactx(generate_assist) {
        switch (luaL_loadfile(l, entry_file)) {
        case LUA_ERRSYNTAX:
            throw luactx_syntax_error(lua_tostring(l, -1));
        case LUA_ERRMEM:
            throw luactx_memory_error();
        case LUA_ERRFILE:
            throw luactx_cannot_open_file(entry_file);
        }

        auto guard = luacpp_exception_guard{[this] {
            lua_close(l);
        }};

        lua_call(l, 0, 0);
    }

    luactx(const lua_code& code, bool generate_assist = false): luactx(generate_assist) {
        switch (luaL_loadstring(l, code.code.data())) {
        case LUA_ERRSYNTAX:
            throw luactx_syntax_error(lua_tostring(l, -1));
        case LUA_ERRMEM:
            throw luactx_memory_error();
        }

        auto guard = luacpp_exception_guard{[this] {
            lua_close(l);
        }};

        luacpp_call(l, 0, 0);
    }

    ~luactx() {
        //lua_gc(l, LUA_GCCOLLECT, 0);
        lua_close(l);
    }

    [[nodiscard]]
    lua_State* ctx() const {
        return l;
    }

    template <typename NameT, typename T>
    decltype(auto) provide(NameT, T&& value) {
        if (generate_assist_file)
            provide_assist<NameT, T>();

        return lua_provide(NameT{}, l, std::forward<T>(value));
    }

    template <typename NameT, typename F1, typename F2, typename... Fs>
    auto provide(NameT, F1&& function1, F2&& function2, Fs&&... functions) {
        if (generate_assist_file)
            provide_assist<NameT, F1, F2, Fs...>();

        return lua_provide_overloaded(
            NameT{}, l, std::forward<F1>(function1), std::forward<F2>(function2), std::forward<Fs>(functions)...);
    }

    template <typename UserType, typename NameT, typename T>
    decltype(auto) provide_member(NameT, T&& value) {
        if (generate_assist_file) {
            if constexpr (LuaFunctionLike<T>)
                provide_assist_for_member_function<UserType, NameT, T>();
            else
                provide_assist<decltype(luacpp_type_registry::get_typespec<UserType>().lua_name().dot(NameT{})), T>();
        }

        return lua_provide(
            luacpp_type_registry::get_typespec<UserType>().lua_name().dot(NameT{}), l, std::forward<T>(value));
    }

    template <typename UserType, typename NameT, typename F1, typename F2, typename... Fs>
    auto provide_member(NameT, F1&& function1, F2&& function2, Fs&&... functions) {
        if (generate_assist_file)
            provide_assist_for_member_function<UserType, NameT, F1, F2, Fs...>();

        return lua_provide_overloaded(luacpp_type_registry::get_typespec<UserType>().lua_name().dot(NameT{}),
                                      l,
                                      std::forward<F1>(function1),
                                      std::forward<F2>(function2),
                                      std::forward<Fs>(functions)...);
    }

    template <typename UserType>
    void set_member_table(luacpp_member_table<UserType> table) {
        if (generate_assist_file) {
            auto class_name = luacpp_type_registry::get_typespec<UserType>().lua_name();
            for (auto& [field_name, _] : table) assist.field(std::string(class_name) + '.' + field_name);
        }

        provide_member<UserType>(LUA_TNAME("__index"), [this, table](const UserType& data, const std::string& field) {
            auto found_field = table.find(field);
            if (found_field != table.end())
                found_field->second.get(data, *this);
            else
                luaL_getmetafield(ctx(), -2, field.data());
            return luacpp_placeholder{};
        });

        provide_member<UserType>(
            LUA_TNAME("__newindex"),
            [this, table = std::move(table)](UserType& data, const std::string& field, luacpp_placeholder) {
                auto found_field = table.find(field);
                if (found_field != table.end()) {
                    if (!found_field->second.set)
                        throw luacpp_access_error(std::string("the field '") + field + "' of object type " +
                                                  luacpp_type_registry::get_typespec<UserType>().lua_name().data() +
                                                  " is private");
                    found_field->second.set(data, *this);
                }
                else
                    throw luacpp_access_error(std::string("object of type '") +
                                              luacpp_type_registry::get_typespec<UserType>().lua_name().data() +
                                              "' has no '" + field + "' field");
            });
    }

    template <typename T, typename NameT>
    decltype(auto) extract(NameT) {
        return luacpp_extract<T>(NameT{}, l);
    }

    template <typename T>
    void push(T&& value) {
        luacpp_push(l, std::forward<T>(value));
    }

    template <typename T>
    decltype(auto) get(int stack_idx) {
        return luacpp_get<T>(l, stack_idx);
    }

    template <typename T>
    T pop() {
        auto res = luacpp_get<T>(l, -1);
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
        luacpp_tforeach<luacpp_typespec_list>([this](auto typespec) {
            using type = decltype(typespec.type());
            provide_member<type>(LUA_TNAME("__gc"), [](type* userdata) { userdata->~type(); });

            lua_getglobal(l, typespec.lua_name().data());
            lua_pushvalue(l, -1);
            lua_setfield(l, -2, "__index");
            lua_pop(l, 1);

            if (generate_assist_file)
                assist.field(luacpp_type_registry::get_typespec<type>().lua_name(), true);

            if constexpr (requires { luacpp_usertype_method_loader<type>(); })
                luacpp_usertype_method_loader<type>()(*this);
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
                if constexpr (luacpp_type_registry::get_index<return_t>() != luacpp_type_registry::no_index)
                    assist.function(
                        NameT{}, arity, std::string(luacpp_type_registry::get_typespec<return_t>().lua_name()));
                else
                    assist.function(NameT{}, arity);
                return;
            }
            if constexpr (LuaMemberFunction<type>) {
                using return_t       = typename details::lua_member_function_traits<type>::return_t;
                constexpr auto arity = details::lua_member_function_traits<type>::arity;
                constexpr auto class_name =
                    luacpp_type_registry::get_typespec<typename details::lua_member_function_traits<type>::class_t>()
                        .lua_name();

                if constexpr (luacpp_type_registry::get_index<return_t>() != luacpp_type_registry::no_index)
                    assist.member_function(class_name,
                                           NameT{},
                                           arity,
                                           std::string(luacpp_type_registry::get_typespec<return_t>().lua_name()));
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
            constexpr auto class_name = luacpp_type_registry::get_typespec<std::decay_t<ClassT>>().lua_name();

            std::optional<std::string> result_metatable;
            if constexpr (luacpp_type_registry::get_index<return_t>() != luacpp_type_registry::no_index)
                result_metatable.emplace(luacpp_type_registry::get_typespec<return_t>().lua_name());

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
    luacpp_assist_gen assist;
};
