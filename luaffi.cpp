#include <iostream>
#include <array>
#include "src/luactx.hpp"
#include "src/luawrap.hpp"
#include "src/lua_usertype_registry.hpp"

static int foo(lua_State* L) {
    int        n   = lua_gettop(L); /* number of arguments */
    lua_Number sum = 0.0;
    int        i;
    for (i = 1; i <= n; i++) {
        if (!lua_isnumber(L, i)) {
            lua_pushliteral(L, "incorrect argument");
            lua_error(L);
        }
        sum += lua_tonumber(L, i);
    }
    lua_pushnumber(L, sum / n); /* first result */
    lua_pushnumber(L, sum);     /* second result */
    lua_pushnumber(L, sum * 100.0);     /* second result */
    return 3;                   /* number of results */
}

int lolkek(int v) {
    return v;
}

#include <vector>

int main() {
    auto l = luactx("hellolua.lua");



    std::cout << "Top: " << l.top() << std::endl;
    try {
        auto v = l.extract<std::vector<std::vector<int>>>(LUA_TNAME("test"));
        for (auto& e : v) {
            for (auto& c : e)
                std::cout << c << " ";
            std::cout << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "panic: " << e.what() << " [[[ top: " << l.top() << std::endl;
    }
    std::cout << "top: " << l.top() << std::endl;

    l.provide(LUA_TNAME("kekchick"), std::tuple{"string", 228, false});
    //l.provide(LUA_TNAME("t.t.t.testfunc"), [](const std::string& str) {
    //    std::cout << str << std::endl;
    //});

    //std::cout << "stack: " << lua_gettop(l.ctx()) << std::endl;

    std::cout << "topA: " << l.top() << std::endl;
    auto hello = l.extract<void()>(LUA_TNAME("kek.mda.lol"));
    std::cout << "topB: " << l.top() << std::endl;
    hello();

    std::cout << "topC: " << l.top() << std::endl;

    //luacpp_get<vec3f>(l.ctx(), 0);

    //auto idx = lua_usertype_registry.to_index<int>();
    //lua_usertype_registry.dispatch_by_index(luacpp_userdata{0, {}}, [](auto) {});

}
