#pragma once

#include "luawrap.hpp"
#include "lua_member_table.hpp"
#include <map>

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

class luactx_panic : public std::runtime_error {
public:
    luactx_panic(const std::string& msg): std::runtime_error(msg) {}
};

struct lua_code {
    std::string code;
};

class luactx {
public:
    luactx(): l(luaL_newstate()) {
        if (!l)
            throw luactx_newstate_failed();

        lua_atpanic(l, panic_wrapper);
        luaL_openlibs(l);

        register_usertypes();
    }

    luactx(const char* entry_file): luactx() {
        auto guard = luacpp_exception_guard{[l = this->l] {
            lua_close(l);
        }};

        switch (luaL_loadfile(l, entry_file)) {
        case LUA_ERRSYNTAX:
            throw luactx_syntax_error(lua_tostring(l, -1));
        case LUA_ERRMEM:
            throw luactx_memory_error();
        case LUA_ERRFILE:
            throw luactx_cannot_open_file(entry_file);
        }

        lua_call(l, 0, 0);
    }

    luactx(const lua_code& code): luactx() {
        auto guard = luacpp_exception_guard{[l = this->l] {
            lua_close(l);
        }};

        switch (luaL_loadstring(l, code.code.data())) {
        case LUA_ERRSYNTAX:
            throw luactx_syntax_error(lua_tostring(l, -1));
        case LUA_ERRMEM:
            throw luactx_memory_error();
        }

        lua_call(l, 0, 0);
    }

    ~luactx() {
        //lua_gc(l, LUA_GCCOLLECT, 0);
        lua_close(l);
    }

    static int panic_wrapper(lua_State* l) {
        auto msg = std::string(lua_tostring(l, -1));
        lua_pop(l, 1);
        throw luactx_panic(msg);
    }

    [[nodiscard]]
    lua_State* ctx() const {
        return l;
    }

    template <typename NameT, typename T>
    decltype(auto) provide(NameT, T&& value) {
        return lua_provide(NameT{}, l, std::forward<T>(value));
    }

    template <typename NameT, typename F1, typename F2, typename... Fs>
    auto provide(NameT, F1&& function1, F2&& function2, Fs&&... functions) {
        return lua_provide_overloaded(
            NameT{}, l, std::forward<F1>(function1), std::forward<F2>(function2), std::forward<Fs>(functions)...);
    }

    template <typename UserType, typename NameT, typename T>
    decltype(auto) provide_member(NameT, T&& value) {
        return lua_provide(
            luacpp_type_registry::get_typespec<UserType>().lua_name().dot(NameT{}), l, std::forward<T>(value));
    }

    template <typename UserType, typename NameT, typename F1, typename F2, typename... Fs>
    auto provide_member(NameT, F1&& function1, F2&& function2, Fs&&... functions) {
        return lua_provide_overloaded(luacpp_type_registry::get_typespec<UserType>().lua_name().dot(NameT{}),
                                      l,
                                      std::forward<F1>(function1),
                                      std::forward<F2>(function2),
                                      std::forward<Fs>(functions)...);
    }

    template <typename UserType>
    void set_member_table(luacpp_member_table<UserType> table) {
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

private:
    void register_usertypes() {
        luacpp_tforeach<luacpp_typespec_list>([this](auto typespec) {
            using type = decltype(typespec.type());
            provide_member<type>(LUA_TNAME("__gc"), [](type* userdata) { userdata->~type(); });

            lua_getglobal(l, typespec.lua_name().data());
            lua_pushvalue(l, -1);
            lua_setfield(l, -2, "__index");
            lua_pop(l, 1);

            if (generate_dummy_file_for_autocompletion) {
                autocompletion_file.append(typespec.lua_name().data());
                autocompletion_file.append(" = {}\n");
            }

            if constexpr (requires { luacpp_usertype_method_loader<type>(); })
                luacpp_usertype_method_loader<type>()(*this);
        });
    }

private:
    lua_State*  l;

    std::string autocompletion_file;
    bool        generate_dummy_file_for_autocompletion = false;
};
