#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include "luactx.hpp"

using namespace Catch::literals;

TEST_CASE("functions") {
    SECTION("no_arguments") {
        auto l = luactx(lua_code{"function cppcall() cppfunc() end"});
        bool called = false;
        l.provide(LUA_TNAME("cppfunc"), [&]{ called = true; });
        l.extract<void()>(LUA_TNAME("cppcall"))();
        REQUIRE(called);
    }

    SECTION("many_arguments") {
        auto l = luactx(lua_code{R"(function cppcall() cppfunc("a", "b", 1, 2, 3, 4, 5, 6, 7, true, false) end)"});
        bool called = false;
        l.provide(
            LUA_TNAME("cppfunc"),
            [&](std::string a, std::string b, int c, float d, double e, uint32_t f, int64_t g, int16_t h, uint8_t i, bool j, bool k) {
                REQUIRE(a == "a");
                REQUIRE(b == "b");
                REQUIRE(c == 1);
                REQUIRE(d == 2_a);
                REQUIRE(e == 3_a);
                REQUIRE(f == 4);
                REQUIRE(g == 5);
                REQUIRE(h == 6);
                REQUIRE(i == 7);
                REQUIRE(j);
                REQUIRE(!k);
                called = true;
            });
        l.extract<void()>(LUA_TNAME("cppcall"))();
        REQUIRE(called);
    }
}

TEST_CASE("functions_error_conditions") {
    auto l = luactx(lua_code{R"(function call0() cppfunc() end function call2() cppfunc(1, 2) end function call3() cppfunc("string") end)"});
    l.provide(LUA_TNAME("cppfunc"), [](int) {});
    bool catched = false;
    try {
        l.extract<void()>(LUA_TNAME("call0"))();
    } catch (...) {
        catched = true;
    }
    REQUIRE(catched);

    catched = false;
    try {
        l.extract<void()>(LUA_TNAME("call2"))();
    }
    catch (...) {
        catched = true;
    }
    REQUIRE(catched);

    catched = false;
    try {
        l.extract<void()>(LUA_TNAME("call3"))();
    }
    catch (...) {
        catched = true;
    }
    REQUIRE(catched);

}
