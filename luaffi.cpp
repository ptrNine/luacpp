#include <iostream>

#include "src/lua_usertype.hpp"


#include "src/luactx.hpp"


template <>
struct luacpp_usertype_method_loader<vec_sample> {
    void operator()(luactx& l) const {
        l.provide(LUA_TNAME("__tostring"), &vec_sample::tostring);
        l.provide_member<vec_sample>(LUA_TNAME("new"), [](float x, float y, float z) {
            return vec_sample(x, y, z);
        });
        l.provide_member<vec_sample>(LUA_TNAME("new"), [](float x) {
            return vec_sample(x, x, x);
        });
    }
};

int main() {
    auto l = luactx(lua_code{"function Main() v = vec_sample.new(1, 2, 3) v2 = vec_sample.new(1) print(v) print(v2) end"});
    auto& v = l.provide(LUA_TNAME("test_var"), vec_sample(228, 228, 228));

    auto main_f = l.extract<void()>(LUA_TNAME("Main"));
    main_f();
    v.y = 1337.f;
    main_f();
}
