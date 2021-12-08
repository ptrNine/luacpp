#include <iostream>

#include "src/lua_usertype.hpp"


#include "src/luactx.hpp"
#include "src/lua_assist_gen.hpp"

#include <vector>

template <>
struct luacpp_usertype_method_loader<vec_sample> {
    void operator()(luactx& l) const {
        l.provide(LUA_TNAME("__tostring"), &vec_sample::tostring);
        l.set_member_table<vec_sample>({lua_getsetez(x), lua_getsetez(y), lua_getsetez(z)});

        l.provide_member<vec_sample>(
            LUA_TNAME("new"),
            [](float x, float y, float z) { return vec_sample(x, y, z); },
            [](const vec_sample& v) { return v; });
        l.provide(LUA_TNAME("test"),
                  (void(vec_sample::*)(int)) & vec_sample::test,
                  (void(vec_sample::*)(std::string)) & vec_sample::test);
    }
};

int main() {
    auto l = luactx("hellolua.lua");


    luacpp_assist_gen assist;

    assist.field("vec_sample", true);
    assist.field("vec_sample.x");
    assist.field("vec_sample.y");
    assist.field("vec_sample.z");
    assist.function("vec_sample.new", {"x", "y", "z"}, "vec_sample");

    std::cout << "ASSIST: \n\n" << assist.generate() << std::endl;

    /*
    l.provide(
        LUA_TNAME("test"),
        []() { std::cout << "NOARG" << std::endl; },
        [](float number, std::string str) { std::cout << number << " " << str << std::endl; },
        [](std::string str, float number) { std::cout << str << " " << number << std::endl; },
        [](std::string str, std::string str2) { std::cout << str << " " << str2 << std::endl; },
        [](float number, int number2) { std::cout << number << " " << number2 << std::endl; },
        [](vec_sample vec) { std::cout << vec.x << " " << vec.y << " " << vec.z << std::endl; }
        );
        */

    l.extract<void()>(LUA_TNAME("main"))();
}



/*
template <>
struct luacpp_usertype_method_loader<vec_sample> {
    void operator()(luactx& l) const {
        l.provide(LUA_TNAME("__tostring"), &vec_sample::tostring);
        l.provide_member<vec_sample>(LUA_TNAME("new"), [](float x, float y, float z) {
            return vec_sample(x, y, z);
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
*/
