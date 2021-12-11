#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <list>

using namespace Catch::literals;

#include "luactx.hpp"

#define TEST_CODE(CHECK_VALUE)                                                                                         \
    "function test(v)\n"                                                                                               \
    "    return v\n"                                                                                                   \
    "end\n"                                                                                                            \
    "\n"                                                                                                               \
    "function test_glob()\n"                                                                                           \
    "    return glob\n"                                                                                                \
    "end\n"                                                                                                            \
    "\n"                                                                                                               \
    "function check(v)\n"                                                                                              \
    "    assert(v == " CHECK_VALUE ")\n"                                                                               \
    "end\n"                                                                                                            \
    "function check_string(v)\n"                                                                                       \
    "    assert(type(v) == \"string\")\n"                                                                              \
    "    assert(v == " CHECK_VALUE ")"                                                                                 \
    "end\n"                                                                                                            \
    "function check_array(v)\n"                                                                                        \
    "    c = " CHECK_VALUE "\n"                                                                                        \
    "    assert(#v == #c)\n"                                                                                           \
    "    assert(type(v) == \"table\")\n"                                                                               \
    "    for i = 1, #v do\n"                                                                                           \
    "        assert(c[i] == v[i])\n"                                                                                   \
    "    end\n"                                                                                                        \
    "end\n"

template <typename T>
void number_test() {
    auto l = luactx(lua_code{TEST_CODE("100")});
    l.extract<void(T)>(LUA_TNAME("check"))(T(100));

    REQUIRE(l.extract<T(T)>(LUA_TNAME("test"))(126) == T(126));

    l.provide(LUA_TNAME("glob"), T(200));
    REQUIRE(l.extract<T()>(LUA_TNAME("test_glob"))() == T(200));
}

TEST_CASE("basic_types") {
    SECTION("uint8_t") {
        number_test<uint8_t>();
    }
    SECTION("uint16_t") {
        number_test<uint16_t>();
    }
    SECTION("uint32_t") {
        number_test<uint32_t>();
    }
    SECTION("uint64_t") {
        number_test<uint64_t>();
    }
    SECTION("int8_t") {
        number_test<int8_t>();
    }
    SECTION("int16_t") {
        number_test<int16_t>();
    }
    SECTION("int32_t") {
        number_test<int32_t>();
    }
    SECTION("int64_t") {
        number_test<int64_t>();
    }
    SECTION("float") {
        auto l = luactx(lua_code{TEST_CODE("100")});
        l.extract<void(float)>(LUA_TNAME("check"))(100);

        REQUIRE(l.extract<float(float)>(LUA_TNAME("test"))(126.126f) == 126.126_a);

        l.provide(LUA_TNAME("glob"), 222.222f);
        REQUIRE(l.extract<float()>(LUA_TNAME("test_glob"))() == 222.222_a);
    }
    SECTION("double") {
        auto l = luactx(lua_code{TEST_CODE("100.101")});
        l.extract<void(double)>(LUA_TNAME("check"))(100.101);

        REQUIRE(l.extract<double(double)>(LUA_TNAME("test"))(126.126) == 126.126_a);

        l.provide(LUA_TNAME("glob"), 222.222);
        REQUIRE(l.extract<double()>(LUA_TNAME("test_glob"))() == 222.222_a);
    }

    SECTION("string") {
        auto l = luactx(lua_code{TEST_CODE("\"check_string\"")});
        l.extract<void(const std::string&)>(LUA_TNAME("check_string"))("check_string");

        REQUIRE(l.extract<std::string(std::string)>(LUA_TNAME("test"))("teststring") == "teststring");

        l.provide(LUA_TNAME("glob"), "teststring");
        REQUIRE(l.extract<std::string()>(LUA_TNAME("test_glob"))() == "teststring");

        l.provide(LUA_TNAME("glob"), std::string_view("teststring2"));
        REQUIRE(l.extract<std::string()>(LUA_TNAME("test_glob"))() == "teststring2");
    }

    SECTION("empty string") {
        auto l = luactx(lua_code{TEST_CODE("\"\"")});
        l.extract<void(const std::string&)>(LUA_TNAME("check_string"))("");

        REQUIRE(l.extract<std::string(std::string)>(LUA_TNAME("test"))("") == "");
    }

    SECTION("table <=> vector<int>") {
        using vec = std::vector<int>;

        auto l = luactx(lua_code{TEST_CODE("{ 10, 11, 12, 15, 18 }")});
        l.extract<void(const vec&)>(LUA_TNAME("check_array"))({10, 11, 12, 15, 18});

        REQUIRE(l.extract<vec(const vec&)>(LUA_TNAME("test"))(vec{9, 8, 7, 6, 5}) == vec{9, 8, 7, 6, 5});

        l.provide(LUA_TNAME("glob"), vec{5, 4, 3, 2, 1, 0, -1, 2, 3, 4});
        REQUIRE(l.extract<vec()>(LUA_TNAME("test_glob"))() == vec{5, 4, 3, 2, 1, 0, -1, 2, 3, 4});
    }

    SECTION("empty table <=> empty vector<int>") {
        using vec = std::vector<int>;

        auto l = luactx(lua_code{TEST_CODE("{}")});
        l.extract<void(const vec&)>(LUA_TNAME("check_array"))({});

        REQUIRE(l.extract<vec(const vec&)>(LUA_TNAME("test"))(vec{}) == vec{});
    }

    SECTION("table <=> array<float, 6>") {
        using vec = std::array<float, 6>;

        auto l = luactx(lua_code{TEST_CODE("{ 10, 11, 12, 15, 18, 15 }")});
        l.extract<void(const vec&)>(LUA_TNAME("check_array"))({10, 11, 12, 15, 18, 15});

        REQUIRE(l.extract<vec(const vec&)>(LUA_TNAME("test"))(vec{9, 8, 7, 6, 5, 2}) == vec{9, 8, 7, 6, 5, 2});

        l.provide(LUA_TNAME("glob"), vec{5, 4, 3, 2, 1, 0});
        REQUIRE(l.extract<vec()>(LUA_TNAME("test_glob"))() == vec{5, 4, 3, 2, 1, 0});
    }

    SECTION("empty table <=> array<float, 0> (very usefull)") {
        using vec = std::array<float, 0>;

        auto l = luactx(lua_code{TEST_CODE("{}")});
        l.extract<void(const vec&)>(LUA_TNAME("check_array"))({});

        REQUIRE(l.extract<vec(const vec&)>(LUA_TNAME("test"))(vec{}) == vec{});
    }

    SECTION("table <=> list<string>") {
        using vec = std::list<std::string>;

        auto l = luactx(lua_code{TEST_CODE("{ \"one\", \"two\", \"three\", \"four\" }")});
        l.extract<void(const vec&)>(LUA_TNAME("check_array"))({"one", "two", "three", "four"});

        REQUIRE(l.extract<vec(const vec&)>(LUA_TNAME("test"))(vec{"aa", "bb", "cc"}) == vec{"aa", "bb", "cc"});

        l.provide(LUA_TNAME("glob"), vec{"hello", ", ", "world", "!"});
        REQUIRE(l.extract<vec()>(LUA_TNAME("test_glob"))() == vec{"hello", ", ", "world", "!"});
    }

    SECTION("empty table <=> empty list<string>") {
        using vec = std::list<std::string>;

        auto l = luactx(lua_code{TEST_CODE("{}")});
        l.extract<void(const vec&)>(LUA_TNAME("check_array"))({});

        REQUIRE(l.extract<vec(const vec&)>(LUA_TNAME("test"))(vec{}) == vec{});
    }
}
