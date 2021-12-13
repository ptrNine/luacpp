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

    SECTION("overloaded") {
        auto code = R"(
            function cppcall()
                cppfunc()
                cppfunc(1, 2, 3)
                cppfunc("one", 2, "three")
                cppfunc({{true, {"one", "two"}}, {false, {"three", "four"}}})
                cppfunc({{true, {"one", "two"}}})
                cppfunc({{true, {"one", 2}}, {false, {"three", 4}}})
            end
        )";
        int calls = 0;
        auto l = luactx(lua_code{code});

        using type1 = std::array<std::tuple<bool, std::array<std::string, 2>>, 2>;
        using type2 = std::array<std::tuple<bool, std::array<std::string, 2>>, 1>;
        using type3 = std::vector<std::tuple<bool, std::tuple<std::string, double>>>;

        l.provide(
            LUA_TNAME("cppfunc"),
            [&] { ++calls; },
            [&](double a, double b, double c) {
                ++calls;
                REQUIRE(a == 1_a);
                REQUIRE(b == 2_a);
                REQUIRE(c == 3_a);
            },
            [&](const std::string& a, double b, const std::string& c) {
                ++calls;
                REQUIRE(a == "one");
                REQUIRE(b == 2_a);
                REQUIRE(c == "three");
            },
            [&](const type1& v) {
                ++calls;
                std::cout << std::get<0>(v[0]) << " " << std::get<1>(v[0])[0] << " " << std::get<1>(v[0])[1] << std::endl;
                REQUIRE(v == type1{{{true, {"one", "two"}}, {false, {"three", "four"}}}});
            },
            [&](const type2& v) {
                ++calls;
                REQUIRE(v == type2{{{true, {"one", "two"}}}});
            },
            [&](const type3& v) {
                ++calls;
                REQUIRE(v == type3{{{true, {"one", 2}}, {false, {"three", 4}}}});
            });
        l.extract<void()>(LUA_TNAME("cppcall"))();
        REQUIRE(calls == 6);
    }
}

TEST_CASE("functions_error_conditions") {
    auto code = R"(
    function call0()
        cppfunc()
    end
    function call2()
        cppfunc(1, 2)
    end
    function call3()
        cppfunc("string")
    end
    function call4()
        cppfunc2({{true, 22.0}, {false, 33.0}, {true, "string"}})
    end
    )";

    auto l = luactx(lua_code{code});
    l.provide(LUA_TNAME("cppfunc"), [](int) {});

    auto top = l.top();
    bool catched = false;
    try {
        l.extract<void()>(LUA_TNAME("call0"))();
    } catch (...) {
        catched = true;
    }
    REQUIRE(catched);
    REQUIRE(l.top() == top);

    catched = false;
    try {
        l.extract<void()>(LUA_TNAME("call2"))();
    }
    catch (...) {
        catched = true;
    }
    REQUIRE(catched);
    REQUIRE(l.top() == top);

    catched = false;
    try {
        l.extract<void()>(LUA_TNAME("call3"))();
    }
    catch (...) {
        catched = true;
    }
    REQUIRE(catched);
    REQUIRE(l.top() == top);

    catched = false;
    using type1 = std::vector<std::tuple<bool, double>>;
    l.provide(LUA_TNAME("cppfunc2"), [](const type1&) {});
    try {
        l.extract<void()>(LUA_TNAME("call4"))();
    }
    catch (...) {
        catched = true;
    }
    REQUIRE(catched);
    REQUIRE(l.top() == top);
}
