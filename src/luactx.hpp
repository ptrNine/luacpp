#pragma once

#include "luawrap.hpp"

class luactx_newstate_failed : public std::runtime_error {
public:
    luactx_newstate_failed(): std::runtime_error("Failed to create lua state") {}
};

class luactx_cant_exec_file : public std::runtime_error {
public:
    luactx_cant_exec_file(const std::string& filename, const std::string& luaerror):
        std::runtime_error("Failed to execute lua file: " + filename + ": " + luaerror) {}
};

class luactx_panic : public std::runtime_error {
public:
    luactx_panic(const std::string& msg): std::runtime_error(msg) {}
};

class luactx {
public:
    luactx(const char* entry_file): l(luaL_newstate()) {
        if (!l)
            throw luactx_newstate_failed();

        luaL_openlibs(l);
        auto rc = luaL_loadfile(l, entry_file);
        if (rc) {
            lua_close(l);
            throw luactx_cant_exec_file(entry_file, "can't open file");
        }

        rc = lua_pcall(l, 0, 0, 0);
        if (rc) {
            std::string error = lua_tostring(l, -1);
            lua_close(l);
            throw luactx_cant_exec_file(entry_file, error);
        }

        lua_atpanic(l, panic_wrapper);
    }

    ~luactx() {
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
    auto provide(NameT, T&& value) {
        return lua_provide(NameT{}, l, std::forward<T>(value));
    }

    template <typename T, typename NameT>
    auto extract(NameT) {
        return luacpp_extract<T>(NameT{}, l);
    }

    template <typename T>
    void push(T&& value) {
        luacpp_push(l, std::forward<T>(value));
    }

    template <typename T>
    T get(int stack_idx) {
        return luacpp_get<T>(l, stack_idx);
    }

    template <typename T>
    T pop() {
        auto res = luacpp_get<T>(l, -1);
        lua_pop(l, 1);
        return res;
    }

    template <typename T>
    void pop_discard(int count = 1) {
        lua_pop(l, count);
    }

    int top() {
        return lua_gettop(l);
    }

private:
    lua_State* l;
};
