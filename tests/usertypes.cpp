#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "lua.hpp"

using namespace Catch::literals;
using namespace luacpp;

void lua_setup_usertypes(luactx& l) {
    auto memtable = ordered_member_table<luavec3>{{"x",
                                           /* Weird, put pretty flexible */
                                           {[](const luavec3& v, luactx& ctx) { ctx.push(v.x); },
                                            [](luavec3& v, luactx& ctx) {
                                                ctx.get_new(v.x);
                                            }}},
                                          /* lua_getsetez may be used for simple member cases */
                                          lua_getsetez(y),
                                          lua_getsetez(z)};
    l.annotate({.comment = "the x value", .explicit_type = "number"});
    l.annotate({.comment = "the y value", .explicit_type = "number"});
    l.set_member_table(memtable);

    l.annotate({.comment = "constructor"});
    l.annotate({.argument_names = {"vector"}});
    l.annotate({.argument_names = {"value"}});
    l.annotate({.argument_names = {"x", "y", "z"}});
    l.provide(
        LUA_TNAME("vec3.new"),
        [] { return luavec3(0); },
        [](const luavec3& v) { return v; }, // deep-copy
        [](double v) { return luavec3(v); },
        [](double x, double y, double z) { return luavec3(x, y, z); });

    l.provide(LUA_TNAME("__add"), &luavec3::operator+<double>);
    l.provide(LUA_TNAME("__sub"), &luavec3::operator-<double>);
    l.provide_commutative_op(LUA_TNAME("__mul"), &luavec3::operator*<double>);
    l.provide(LUA_TNAME("__div"), &luavec3::operator/<double>);
    l.provide(LUA_TNAME("__eq"), &luavec3::operator==<double>);
    l.provide(LUA_TNAME("magnitude"), &luavec3::magnitude);
    l.provide(LUA_TNAME("dot"), &luavec3::dot<double>);
    l.provide(LUA_TNAME("cross"), &luavec3::cross<double>);
    l.provide_member<luavec3>(LUA_TNAME("__tostring"), [](const luavec3& v) {
        return std::to_string(v.x) + " " + std::to_string(v.y) + " " + std::to_string(v.z);
    });

    l.provide(LUA_TNAME("test"), &string_like::test);
}

auto code = R"(
function test()
    assert(vec3.new() == vec3.new(0))
    assert(vec3.new(1) == vec3.new(1, 1, 1))
    assert(vec3.new(1, 2, 3):new() == vec3.new(1, 2, 3))

    assert(vec3.new(4, 2, -4):magnitude() == 6)
    assert(vec3.new(-2, 4, 4):dot(vec3.new(2, -4, -4)) == -36)
    assert(vec3.new(1, -2, 3):cross(vec3.new(-1, 2, 3)) == vec3.new(-12, -6, 0))

    assert(vec3.new(1, 2, 3) * 4 == vec3.new(4, 8, 12))
    assert(4 * vec3.new(1, 2, 3) == vec3.new(4, 8, 12))
    assert(vec3.new(4, 8, 12) / 4 == vec3.new(1, 2, 3))

    assert(vec3.new(4, 0, 4) + vec3.new(0, -4, 0) == vec3.new(4, -4, 4))
    assert(vec3.new(8, 10, 12) - vec3.new(0, 2, 4) == vec3.new(8))

    v1 = vec3.new(10, 20, 30)
    assert(v1.x == 10)
    v1.x = 1
    assert(v1.x == 1)

    assert(v1.y == 20)
    v1.y = 2
    assert(v1.y == 2)

    assert(v1.z == 30)
    v1.z = 3
    assert(v1.z == 3)
end
)";

constexpr auto assist_txt = R"(
---@class string_like
string_like = {
    ---@param self string_like
    ---@return string
    test = function(self) end,
    __index = string_like
}
---@class vec3
vec3 = {
    ---@param self vec3
    ---@param a vec3
    ---@return vec3
    __add = function(self, a) end,
    ---@param self vec3
    ---@param a number
    ---@return vec3
    __div = function(self, a) end,
    ---@param self vec3
    ---@param a vec3
    ---@return boolean
    __eq = function(self, a) end,
    ---@param self vec3
    ---@param a number
    ---@return vec3
    ---@overload fun(a:number,b:vec3):vec3
    __mul = function(self, a) end,
    ---@param self vec3
    ---@param a vec3
    ---@return vec3
    __sub = function(self, a) end,
    ---@param self vec3
    ---@return string
    __tostring = function(self) end,
    ---@param self vec3
    ---@param a vec3
    ---@return vec3
    cross = function(self, a) end,
    ---@param self vec3
    ---@param a vec3
    ---@return number
    dot = function(self, a) end,
    ---@param self vec3
    ---@return number
    magnitude = function(self) end,
    ---constructor
    ---@return vec3
    ---@overload fun(vector:vec3):vec3
    ---@overload fun(value:number):vec3
    ---@overload fun(x:number,y:number,z:number):vec3
    new = function() end,
    ---the x value
    ---@type number
    x = nil,
    ---the y value
    ---@type number
    y = nil,
    ---@type any
    z = nil,
    __index = vec3
})";

TEST_CASE("usertypes") {
    SECTION("basic") {
        auto l = luactx(lua_code{code});
        lua_setup_usertypes(l);
        l.extract<void()>(LUA_TNAME("test"))();
    }
    SECTION("field not exists") {
        luactx l;
        lua_setup_usertypes(l);
        l.load_and_call(lua_code{"v = vec3.new(0)"});

        auto top = l.top();
        bool catched = false;
        try {
            l.extract<int>(LUA_TNAME("v.unexisted_field.boom"));
        }
        catch (const errors::access_error&) {
            catched = true;
        }
        REQUIRE(catched);
        REQUIRE(l.top() == top);
    }
    SECTION("wrong type") {
        luactx l;
        lua_setup_usertypes(l);
        l.load_and_call(lua_code{"v = vec3.new(0)"});

        auto top = l.top();
        bool catched = false;
        try {
            l.extract<int>(LUA_TNAME("v.unexisted_field"));
        }
        catch (const errors::cast_error&) {
            catched = true;
        }
        REQUIRE(catched);
        REQUIRE(l.top() == top);
    }
    SECTION("assist generator") {
        auto l = luactx(lua_code{code}, true);
        lua_setup_usertypes(l);
        REQUIRE(l.generate_assist() == assist_txt);
    }
    SECTION("registered string-like") {
        auto l = luactx(lua_code{"function test_func(v) assert(v:test() == \"test kek\") end"});
        lua_setup_usertypes(l);
        l.extract<void(const string_like&)>(LUA_TNAME("test_func"))(string_like("kek"));
    }
}
