#pragma once

#include "luawrap.hpp"

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
