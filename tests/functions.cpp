#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "luacpp_ctx.hpp"

using namespace Catch::literals;
using namespace luacpp;

TEST_CASE("functions") {
    SECTION("no_arguments") {
        auto l = luactx(lua_code{"function cppcall() cppfunc() end"});
        auto top = l.top();
        bool called = false;
        l.provide(LUA_TNAME("cppfunc"), [&]{ called = true; });
        l.extract<void()>(LUA_TNAME("cppcall"))();
        REQUIRE(called);
        called = false;
        l.extract<void()>(lua_name{"cppcall"})();
        REQUIRE(called);
        REQUIRE(top == l.top());
    }

    SECTION("many_arguments") {
        auto l = luactx(lua_code{R"(function cppcall() cppfunc("a", "b", 1, 2, 3, 4, 5, 6, 7, true, false) end)"});
        auto top = l.top();
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
        called = false;
        l.extract<void()>(lua_name{"cppcall"})();
        REQUIRE(called);
        REQUIRE(top == l.top());
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
        auto top = l.top();

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
        l.extract<void()>(lua_name{"cppcall"})();
        REQUIRE(calls == 12);

        REQUIRE(top == l.top());
    }

    SECTION("variable arguments") {
        auto l = luactx(lua_code{R"(
            function f(...)
                local args = {...}
                local nargs = #args
                if nargs == 0 then
                    return 0
                elseif nargs == 1 then
                    assert(args[1] == "FIRST")
                    return 1
                elseif nargs == 2 then
                    assert(args[1] == 1)
                    assert(args[2] == "SECOND")
                    return 2
                elseif nargs == 3 then
                    assert(args[1] == "FIRST")
                    assert(args[2] == 2)
                    assert(args[3] == "THIRD")
                    return 3
                end
            end)"
        });

        auto f = l.extract<int(variable_args)>(LUA_TNAME("f"));

        REQUIRE(f() == 0);
        REQUIRE(f("FIRST") == 1);
        REQUIRE(f(1, "SECOND") == 2);
        REQUIRE(f("FIRST", 2, "THIRD") == 3);
    }

    SECTION("multiple return") {
        auto l = luactx(lua_code{R"(
            function f1() return 1, 2, 3 end
            function f2() return 1 end
            function f3(v) if v then return 1 else return 1, 2, 3 end end
        )"});

        auto f1 = l.extract<multiresult<int, int, int>()>(LUA_TNAME("f1"));
        auto f2 = l.extract<multiresult<int>()>(LUA_TNAME("f2"));
        auto f3 = l.extract<multiresult<int, std::optional<int>, std::optional<int>>(bool)>(LUA_TNAME("f3"));

        REQUIRE(f1().storage == std::tuple{1, 2, 3});
        REQUIRE(f2().storage == std::tuple{1});
        REQUIRE(f3(true).storage == std::tuple{1, std::optional<int>{}, std::optional<int>{}});
        REQUIRE(f3(false).storage == std::tuple{1, 2, 3});
    }

    SECTION("explicit return") {
        auto l      = luactx(lua_code{"function cppcall() a, b = cppfunc() assert(a == 'a') assert(b == 'b') end"});
        auto top    = l.top();
        bool called = false;
        l.provide(LUA_TNAME("cppfunc"), [&] {
            called = true;
            return explicit_return(l, "a", "b");
        });
        l.extract<void()>(LUA_TNAME("cppcall"))();
        REQUIRE(called);
        called = false;
        l.extract<void()>(lua_name{"cppcall"})();
        REQUIRE(called);
        REQUIRE(top == l.top());
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
    } catch (const std::exception&) {
        catched = true;
    }
    REQUIRE(catched);
    REQUIRE(l.top() == top);

    catched = false;
    try {
        l.extract<void()>(LUA_TNAME("call2"))();
    }
    catch (const std::exception&) {
        catched = true;
    }
    REQUIRE(catched);
    REQUIRE(l.top() == top);

    catched = false;
    try {
        l.extract<void()>(LUA_TNAME("call3"))();
    }
    catch (const std::exception&) {
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
    catch (const std::exception&) {
        catched = true;
    }
    REQUIRE(catched);
    REQUIRE(l.top() == top);
}
