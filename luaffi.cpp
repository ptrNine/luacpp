#include <iostream>

#include "src/lua_usertype.hpp"


#include "src/luactx.hpp"


int main() {
    auto l = luactx(lua_code{"function Main() print(test_var) end"});
    //luacpp_type_registry::get_index<vec_sample>();

    l.provide(LUA_TNAME("__tostring"), &vec_sample::tostring);

    auto& v = l.provide(LUA_TNAME("test_var"), vec_sample(228, 228, 228));



    auto main_f = l.extract<void()>(LUA_TNAME("Main"));

    main_f();

    v.y = 1337.f;
    main_f();
}
