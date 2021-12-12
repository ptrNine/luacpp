#pragma once

#include <lua.hpp>
#include <stdexcept>
#include "lua_utils.hpp"

class luactx_panic : public std::runtime_error {
public:
    luactx_panic(const char* msg): std::runtime_error(msg) {}
};

inline void luacpp_call(lua_State* l, int nargs, int nresults) {
    auto rc = lua_pcall(l, nargs, nresults, 0);
    switch (rc) {
    case LUA_ERRRUN: {
        auto finalizer = luacpp_finalizer{[&] {
            lua_pop(l, 1);
        }};
        throw luactx_panic(lua_tostring(l, -1));
    }
    case LUA_ERRMEM:
        throw luactx_panic("Lua memory allocation error");
    case LUA_ERRERR:
        throw luactx_panic("Error in non-existed error handler :)");
    }
}
