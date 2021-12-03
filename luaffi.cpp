#include <iostream>

#include "src/lua_usertype.hpp"

struct vec_sample {
    float x, y, z;
};

template <>
struct luacpp_usertype_list<0> {
    using type = std::tuple<
        luacpp_usertype<std::string_view, luacpp_memclass::box, LUA_TNAME("strview")>,
        luacpp_usertype<std::u16string_view, luacpp_memclass::box, LUA_TNAME("strview2")>,
        luacpp_usertype<vec_sample, luacpp_memclass::box, LUA_TNAME("vec_sample")>
        >;
};

#include "src/luactx.hpp"


int main() {
    auto l = luactx(lua_code{"function Main(v) print(v) end function gcrun() collectgarbage(\"collect\") end"});
    l.extract<void(vec_sample)>(LUA_TNAME("Main"))(vec_sample{});
    l.extract<void()>(LUA_TNAME("gcrun"))();
}
