#include <iostream>

#include "src/lua_usertype.hpp"


#include "src/luactx.hpp"


int main() {
    auto l = luactx(lua_code{"function Main(v) print(v) end function gcrun() collectgarbage(\"collect\") end"});
    //luacpp_type_registry::get_index<vec_sample>();

    l.provide_member<vec_sample>(LUA_TNAME("__tostring"), [](const vec_sample& vec) {
        return std::to_string(vec.x) + ", " + std::to_string(vec.y) + ", " + std::to_string(vec.z);
    });

    l.extract<void(vec_sample)>(LUA_TNAME("Main"))(vec_sample{1, 2, 3});
    l.extract<void()>(LUA_TNAME("gcrun"))();
}
