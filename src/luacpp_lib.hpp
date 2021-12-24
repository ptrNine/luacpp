#pragma once

#ifdef WITH_LUAJIT
    #include "lua.hpp"
#else
    #include "lua.h"
    #include "lauxlib.h"
    #include "lualib.h"
#endif

#include <stdexcept>
#include "luacpp_utils.hpp"

#if LUA_VERSION_NUM >= 502
    #define lua_objlen(state, idx) lua_rawlen(state, idx)
#endif

namespace luacpp
{
namespace errors
{
    class panic : public std::runtime_error {
    public:
        panic(const char* msg): std::runtime_error(msg) {}
    };
} // namespace errors

inline void luacall(lua_State* l, int nargs, int nresults) {
    auto rc = lua_pcall(l, nargs, nresults, 0);
    switch (rc) {
    case LUA_ERRRUN: {
        auto finalize = finalizer{[&] {
            lua_pop(l, 1);
        }};
        throw errors::panic(lua_tostring(l, -1));
    }
    case LUA_ERRMEM: throw errors::panic("Lua memory allocation error");
    case LUA_ERRERR: throw errors::panic("Error in non-existed error handler :)");
    }
}
} // namespace luacpp
