#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include "luacpp_ctx.hpp"

using namespace luacpp;

constexpr auto luacode = R"(
function no_arg()
    return 1
end

function three_arg(a, b, c)
    return a + b + c
end

function lua_no_arg()
    return cpp_no_arg()
end

function lua_three_arg(a, b, c)
    return cpp_three_arg(a, b, c)
end

function overloaded1()
    return cpp_overloaded()
end

function overloaded2()
    return cpp_overloaded(true, 228)
end

function overloaded3()
    return cpp_overloaded(228, false)
end

function overloaded4()
    return cpp_overloaded(1, 2, 3, 4)
end

function overloaded5()
    return cpp_overloaded(true, false, true)
end
)";

TEST_CASE("call_lua") {
    auto l         = luactx(lua_code{luacode});
    auto no_arg    = l.extract<double()>(LUA_TNAME("no_arg"));
    auto three_arg = l.extract<double(double, double, double)>(LUA_TNAME("three_arg"));

    BENCHMARK("no_args") {
        return no_arg();
    };

    BENCHMARK("3_number_args_sum") {
        return three_arg(1.2, 3.3, 4.4);
    };
}

TEST_CASE("bidirectional_call") {
    auto l          = luactx(lua_code{luacode});
    auto lua_no_arg = l.extract<double()>(LUA_TNAME("lua_no_arg"));
    l.provide(LUA_TNAME("cpp_no_arg"), [] { return 229.0; });
    auto lua_three_arg = l.extract<double(double, double, double)>(LUA_TNAME("lua_three_arg"));
    l.provide(LUA_TNAME("cpp_three_arg"), [](double a, double b, double c) { return a + b + c; });

    BENCHMARK("no_arg") {
        return lua_no_arg();
    };

    BENCHMARK("three_arg") {
        return lua_three_arg(1.2, 3.3, 4.4);
    };
}

TEST_CASE("bidirectional_overloaded_call") {
    auto l           = luactx(lua_code{luacode});
    auto overloaded1 = l.extract<double()>(LUA_TNAME("overloaded1"));
    auto overloaded2 = l.extract<double()>(LUA_TNAME("overloaded2"));
    auto overloaded3 = l.extract<double()>(LUA_TNAME("overloaded3"));
    auto overloaded4 = l.extract<double()>(LUA_TNAME("overloaded4"));
    auto overloaded5 = l.extract<double()>(LUA_TNAME("overloaded5"));

    l.provide(
        LUA_TNAME("cpp_overloaded"),
        [](double a, double b, double c, double d) { return a + b + c + d; },
        [](double a, bool b) { return a * (b ? 1.0 : 2.0); },
        [] { return 1.0; },
        [](bool a, bool b, bool c) { return (a && b && c) ? 1.0 : 200.0; },
        [](bool a, double b) { return b * (a ? 2.0 : 3.0); }
        );

    BENCHMARK("double()") {
        return overloaded1();
    };
    BENCHMARK("double(double, bool)") {
        return overloaded2();
    };
    BENCHMARK("double(bool, double)") {
        return overloaded3();
    };
    BENCHMARK("double(double, double, double, double)") {
        return overloaded4();
    };
    BENCHMARK("double(bool, bool, bool)") {
        return overloaded5();
    };
}
