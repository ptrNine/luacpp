#include <iostream>

#include "src/lua_usertype.hpp"


#include "src/luactx.hpp"


int main() {
    auto l = luactx(lua_code{"function Main() print(test_var) end"});
    //luacpp_type_registry::get_index<vec_sample>();

    l.provide_member<vec_sample>(LUA_TNAME("__tostring"), [](const vec_sample& vec) {
        return std::to_string(vec.x) + ", " + std::to_string(vec.y) + ", " + std::to_string(vec.z);
    });

    auto& v = l.provide(LUA_TNAME("test_var"), vec_sample(228, 228, 228));

    auto main_f = l.extract<void()>(LUA_TNAME("Main"));

    main_f();

    v.y = 1337.f;
    main_f();
}
