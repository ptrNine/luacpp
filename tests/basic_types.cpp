#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <list>
#include <map>
#include <array>

#include "lua.hpp"

using namespace Catch::literals;
using namespace luacpp;

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
    "end\n"                                                                                                            \
    "function call_cpp()\n"                                                                                            \
    "    cppfunc(" CHECK_VALUE ")\n"                                                                                   \
    "end\n"

template <typename T>
void number_test() {
    auto l = luactx(lua_code{TEST_CODE("100")});
    auto top = l.top();

    l.extract<void(T)>(LUA_TNAME("check"))(T(100));

    REQUIRE(l.extract<T(T)>(LUA_TNAME("test"))(126) == T(126));

    l.provide(LUA_TNAME("glob"), T(200));
    REQUIRE(l.extract<T()>(LUA_TNAME("test_glob"))() == T(200));

    l.provide(LUA_TNAME("cppfunc"), [](T number) {
        REQUIRE(number == 100);
    });
    l.extract<void()>(LUA_TNAME("call_cpp"))();

    REQUIRE(top == l.top());
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
        auto top = l.top();
        l.extract<void(float)>(LUA_TNAME("check"))(100);

        REQUIRE(l.extract<float(float)>(LUA_TNAME("test"))(126.126f) == 126.126_a);

        l.provide(LUA_TNAME("glob"), 222.222f);
        REQUIRE(l.extract<float()>(LUA_TNAME("test_glob"))() == 222.222_a);

        l.provide(LUA_TNAME("cppfunc"), [](float number) {
            REQUIRE(number == 100_a);
        });
        l.extract<void()>(LUA_TNAME("call_cpp"))();

        REQUIRE(top == l.top());
    }
    SECTION("double") {
        auto l = luactx(lua_code{TEST_CODE("100.101")});
        auto top = l.top();
        l.extract<void(double)>(LUA_TNAME("check"))(100.101);

        REQUIRE(l.extract<double(double)>(LUA_TNAME("test"))(126.126) == 126.126_a);

        l.provide(LUA_TNAME("glob"), 222.222);
        REQUIRE(l.extract<double()>(LUA_TNAME("test_glob"))() == 222.222_a);

        l.provide(LUA_TNAME("cppfunc"), [](double number) {
            REQUIRE(number == 100.101_a);
        });
        l.extract<void()>(LUA_TNAME("call_cpp"))();

        REQUIRE(top == l.top());
    }

    SECTION("bool") {
        auto l = luactx(
            lua_code{"function check_true(v) assert(v) end function check_false(v) assert(v == false) end function "
                     "test(v) return v end function call_cpp() cppfunc(true) end"});

        auto top = l.top();
        l.extract<void(bool)>(LUA_TNAME("check_true"))(true);
        l.extract<void(bool)>(LUA_TNAME("check_false"))(false);
        REQUIRE(l.extract<bool(bool)>(LUA_TNAME("test"))(true));
        REQUIRE(!l.extract<bool(bool)>(LUA_TNAME("test"))(false));

        l.provide(LUA_TNAME("cppfunc"), [](bool v) {
            REQUIRE(v);
        });
        l.extract<void()>(LUA_TNAME("call_cpp"))();

        REQUIRE(top == l.top());
    }

    SECTION("nil (!?!?!)") {
        auto l = luactx(lua_code{"function check_nil(v) assert(v == nil) end"});
        auto top = l.top();
        l.extract<void(std::nullptr_t)>(LUA_TNAME("check_nil"))(nullptr);
        l.extract<void(const char*)>(LUA_TNAME("check_nil"))(nullptr);
        REQUIRE(top == l.top());
    }

    SECTION("string") {
        auto l = luactx(lua_code{TEST_CODE("\"check_string\"")});
        auto top = l.top();
        l.extract<void(const std::string&)>(LUA_TNAME("check_string"))("check_string");

        REQUIRE(l.extract<std::string(std::string)>(LUA_TNAME("test"))("teststring") == "teststring");

        l.provide(LUA_TNAME("glob"), "teststring");
        REQUIRE(l.extract<std::string()>(LUA_TNAME("test_glob"))() == "teststring");

        l.provide(LUA_TNAME("glob"), std::string_view("teststring2"));
        REQUIRE(l.extract<std::string()>(LUA_TNAME("test_glob"))() == "teststring2");

        l.provide(LUA_TNAME("cppfunc"), [](const std::string& v) {
            REQUIRE(v == "check_string");
        });
        l.extract<void()>(LUA_TNAME("call_cpp"))();

        REQUIRE(top == l.top());
    }

    SECTION("empty string") {
        auto l = luactx(lua_code{TEST_CODE("\"\"")});
        auto top = l.top();
        l.extract<void(const std::string&)>(LUA_TNAME("check_string"))("");

        REQUIRE(l.extract<std::string(std::string)>(LUA_TNAME("test"))("") == "");

        l.provide(LUA_TNAME("cppfunc"), [](const std::string& v) {
            REQUIRE(v.empty());
        });
        l.extract<void()>(LUA_TNAME("call_cpp"))();

        REQUIRE(top == l.top());
    }

    SECTION("table <=> vector<int>") {
        using vec = std::vector<int>;

        auto l = luactx(lua_code{TEST_CODE("{ 10, 11, 12, 15, 18 }")});
        auto top = l.top();
        l.extract<void(const vec&)>(LUA_TNAME("check_array"))({10, 11, 12, 15, 18});

        REQUIRE(l.extract<vec(const vec&)>(LUA_TNAME("test"))(vec{9, 8, 7, 6, 5}) == vec{9, 8, 7, 6, 5});

        l.provide(LUA_TNAME("glob"), vec{5, 4, 3, 2, 1, 0, -1, 2, 3, 4});
        REQUIRE(l.extract<vec()>(LUA_TNAME("test_glob"))() == vec{5, 4, 3, 2, 1, 0, -1, 2, 3, 4});

        l.provide(LUA_TNAME("cppfunc"), [](const vec& v) {
            REQUIRE(v == vec{10, 11, 12, 15, 18});
        });
        l.extract<void()>(LUA_TNAME("call_cpp"))();

        REQUIRE(top == l.top());
    }

    SECTION("empty table <=> empty vector<int>") {
        using vec = std::vector<int>;

        auto l = luactx(lua_code{TEST_CODE("{}")});
        auto top = l.top();
        l.extract<void(const vec&)>(LUA_TNAME("check_array"))({});

        REQUIRE(l.extract<vec(const vec&)>(LUA_TNAME("test"))(vec{}) == vec{});

        l.provide(LUA_TNAME("cppfunc"), [](const vec& v) {
            REQUIRE(v.empty());
        });
        l.extract<void()>(LUA_TNAME("call_cpp"))();

        REQUIRE(top == l.top());
    }

    SECTION("table <=> array<float, 6>") {
        using vec = std::array<float, 6>;

        auto l = luactx(lua_code{TEST_CODE("{ 10, 11, 12, 15, 18, 15 }")});
        auto top = l.top();
        l.extract<void(const vec&)>(LUA_TNAME("check_array"))({10, 11, 12, 15, 18, 15});

        REQUIRE(l.extract<vec(const vec&)>(LUA_TNAME("test"))(vec{9, 8, 7, 6, 5, 2}) == vec{9, 8, 7, 6, 5, 2});

        l.provide(LUA_TNAME("glob"), vec{5, 4, 3, 2, 1, 0});
        REQUIRE(l.extract<vec()>(LUA_TNAME("test_glob"))() == vec{5, 4, 3, 2, 1, 0});

        l.provide(LUA_TNAME("cppfunc"), [](const vec& v) {
            REQUIRE(v == vec{10, 11, 12, 15, 18, 15});
        });
        l.extract<void()>(LUA_TNAME("call_cpp"))();

        REQUIRE(top == l.top());
    }

    SECTION("empty table <=> array<float, 0> (very usefull)") {
        using vec = std::array<float, 0>;

        auto l = luactx(lua_code{TEST_CODE("{}")});
        auto top = l.top();
        l.extract<void(const vec&)>(LUA_TNAME("check_array"))({});

        REQUIRE(l.extract<vec(const vec&)>(LUA_TNAME("test"))(vec{}) == vec{});

        l.provide(LUA_TNAME("cppfunc"), [](const vec& v) {
            REQUIRE(v.empty());
        });
        l.extract<void()>(LUA_TNAME("call_cpp"))();

        REQUIRE(top == l.top());
    }

    SECTION("table <=> list<string>") {
        using vec = std::list<std::string>;

        auto l = luactx(lua_code{TEST_CODE("{ \"one\", \"two\", \"three\", \"four\" }")});
        auto top = l.top();
        l.extract<void(const vec&)>(LUA_TNAME("check_array"))({"one", "two", "three", "four"});

        REQUIRE(l.extract<vec(const vec&)>(LUA_TNAME("test"))(vec{"aa", "bb", "cc"}) == vec{"aa", "bb", "cc"});

        l.provide(LUA_TNAME("glob"), vec{"hello", ", ", "world", "!"});
        REQUIRE(l.extract<vec()>(LUA_TNAME("test_glob"))() == vec{"hello", ", ", "world", "!"});

        l.provide(LUA_TNAME("cppfunc"), [](const vec& v) {
            REQUIRE(v == vec{"one", "two", "three", "four"});
        });
        l.extract<void()>(LUA_TNAME("call_cpp"))();

        REQUIRE(top == l.top());
    }

    SECTION("table <=> list<string_like>") {
        struct string_like {
            std::string str;
            string_like(std::string_view s): str(s) {}
            operator std::string_view() const {
                return str;
            }
            bool operator==(const string_like& v) const {
                return str == v.str;
            }
        };

        using vec = std::list<string_like>;

        auto l = luactx(lua_code{TEST_CODE("{ \"one\", \"two\", \"three\", \"four\" }")});
        auto top = l.top();
        l.extract<void(const vec&)>(LUA_TNAME("check_array"))({{"one"}, {"two"}, {"three"}, {"four"}});

        REQUIRE(l.extract<vec(const vec&)>(LUA_TNAME("test"))(vec{{"aa"}, {"bb"}, {"cc"}}) ==
                vec{{"aa"}, {"bb"}, {"cc"}});

        l.provide(LUA_TNAME("glob"), vec{{"hello"}, {", "}, {"world"}, {"!"}});
        REQUIRE(l.extract<vec()>(LUA_TNAME("test_glob"))() == vec{{"hello"}, {", "}, {"world"}, {"!"}});

        l.provide(LUA_TNAME("cppfunc"), [](const vec& v) {
            REQUIRE(v == vec{{"one"}, {"two"}, {"three"}, {"four"}});
        });
        l.extract<void()>(LUA_TNAME("call_cpp"))();

        REQUIRE(top == l.top());
    }

    SECTION("table <=> map<string, vector<int>>") {
        using map_t = std::map<std::string, std::vector<int>>;

        auto l = luactx(lua_code{R"(
            function check_map(test_v)
                c = {one = {1, 2, 1, 1}, two = {2, 3, 2}}
                assert(#c == #test_v)
                assert(type(test_v) == "table")
                for k,v in pairs(test_v) do
                    cv = c[k]
                    for i = 1, #v do
                        assert(cv[i] == v[i])
                    end
                end
                for k,v in pairs(c) do
                    cv = test_v[k]
                    for i = 1, #v do
                        assert(cv[i] == v[i])
                    end
                end
            end

            function test(v)
                return v
            end

            function call_cpp()
                cppfunc({one = {1, 2, 1, 1}, two = {2, 3, 2}})
            end
        )"});
        auto top = l.top();
        auto map_v = map_t{{"one", {1, 2, 1, 1}}, {"two", {2, 3, 2}}};
        l.extract<void(const map_t&)>(LUA_TNAME("check_map"))(map_v);

        REQUIRE(l.extract<map_t(const map_t&)>(LUA_TNAME("test"))(map_v) == map_v);

        l.provide(LUA_TNAME("cppfunc"), [&](const map_t& v) {
            REQUIRE(v == map_v);
        });
        l.extract<void()>(LUA_TNAME("call_cpp"))();

        REQUIRE(top == l.top());
    }

    SECTION("table <=> map<double, vector<string>>") {
        using map_t = std::map<double, std::vector<std::string>>;

        auto l = luactx(lua_code{R"(
            function check_map(test_v)
                c = {}
                c[1.1] = {"a", "b"}
                c[2.23] = {"z", "y", "x"}
                c[-26] = {"lol"}
                assert(#c == #test_v)
                assert(type(test_v) == "table")
                for k,v in pairs(test_v) do
                    cv = c[k]
                    for i = 1, #v do
                        assert(cv[i] == v[i])
                    end
                end
                for k,v in pairs(c) do
                    cv = test_v[k]
                    for i = 1, #v do
                        assert(cv[i] == v[i])
                    end
                end
            end

            function test(v)
                return v
            end

            function call_cpp()
                c = {}
                c[1.1] = {"a", "b"}
                c[2.23] = {"z", "y", "x"}
                c[-26] = {"lol"}
                cppfunc(c)
            end
        )"});
        auto top = l.top();
        auto map_v = map_t{{1.1, {"a", "b"}}, {2.23, {"z", "y", "x"}}, {-26.0, {"lol"}}};
        l.extract<void(const map_t&)>(LUA_TNAME("check_map"))(map_v);

        REQUIRE(l.extract<map_t(const map_t&)>(LUA_TNAME("test"))(map_v) == map_v);

        l.provide(LUA_TNAME("cppfunc"), [&](const map_t& v) {
            REQUIRE(v == map_v);
        });
        l.extract<void()>(LUA_TNAME("call_cpp"))();

        REQUIRE(top == l.top());
    }

    SECTION("empty table <=> empty list<string>") {
        using vec = std::list<std::string>;

        auto l = luactx(lua_code{TEST_CODE("{}")});
        auto top = l.top();
        l.extract<void(const vec&)>(LUA_TNAME("check_array"))({});

        REQUIRE(l.extract<vec(const vec&)>(LUA_TNAME("test"))(vec{}) == vec{});

        l.provide(LUA_TNAME("cppfunc"), [](const vec& v) {
            REQUIRE(v.empty());
        });
        l.extract<void()>(LUA_TNAME("call_cpp"))();

        REQUIRE(top == l.top());
    }

    SECTION("complex type") {
        using type =
            std::tuple<bool, std::vector<std::tuple<double, std::array<std::string, 2>>>>;

        auto l = luactx(lua_code{
            "function test(v)\n"
            "    assert(v[1] == true)\n"
            "    assert(v[2][1][1] == 228.228)\n"
            "    assert(v[2][1][2][1] == \"one\")\n"
            "    assert(v[2][1][2][2] == \"two\")\n"
            "    assert(v[2][2][1] == 229.229)\n"
            "    assert(v[2][2][2][1] == \"three\")\n"
            "    assert(v[2][2][2][2] == \"four\")\n"
            "    assert(v[2][3][1] == 111.111)\n"
            "    assert(v[2][3][2][1] == \"five\")\n"
            "    assert(v[2][3][2][2] == \"six\")\n"
            "end\n"
            "function testget()\n"
            "    return {\n"
            "        false, {\n"
            "            {101.101, {\"aaa\", \"bbb\"}},\n"
            "            {202.202, {\"cccc\", \"dddd\"}},\n"
            "            {303.303, {\"eeeee\", \"fffff\"}},\n"
            "        }\n"
            "    }\n"
            "end\n"
            "function call_cpp()\n"
            "    cppfunc({true, {{99.9, {\"abc\", \"defg\"}}}})\n"
            "end\n"
        });
        auto top = l.top();

        l.extract<void(const type&)>(LUA_TNAME("test"))(type{
            true,
            {
                {228.228, {"one", "two"}},
                {229.229, {"three", "four"}},
                {111.111, {"five", "six"}},
            }
        });

        REQUIRE(l.extract<type()>(LUA_TNAME("testget"))() == type{false,
                                                                  {
                                                                      {101.101, {"aaa", "bbb"}},
                                                                      {202.202, {"cccc", "dddd"}},
                                                                      {303.303, {"eeeee", "fffff"}},
                                                                  }});

        l.provide(LUA_TNAME("cppfunc"), [](const type& v) {
            REQUIRE(v == type{true, {{99.9, {"abc", "defg"}}}});
        });
        l.extract<void()>(LUA_TNAME("call_cpp"))();

        REQUIRE(top == l.top());
    }

    SECTION("nested tables") {
        auto l = luactx(lua_code{
                "function test1() return a.d end\n"
                "function test2() return a.b.c end\n"
                "t = { a = \"its t.a\" }\n"
                "function test3() return t.a .. t.b.c end\n"});

        l.provide(LUA_TNAME("a.b.c"), "a.b.c");
        l.provide(LUA_TNAME("a.d"), 200);
        l.provide(LUA_TNAME("t.b.c"), " and t.b.c");

        REQUIRE(l.extract<int()>(LUA_TNAME("test1"))() == 200);
        REQUIRE(l.extract<std::string()>(LUA_TNAME("test2"))() == "a.b.c");
        REQUIRE(l.extract<std::string()>(LUA_TNAME("test3"))() == "its t.a and t.b.c");
    }

    SECTION("optional") {
        auto l = luactx(lua_code{
            "function test1(v) assert(v == 200) return v end\n"
            "function test2(v) assert(v == nil) return v end\n"});

        auto f1 = l.extract<std::optional<int>(std::optional<int>)>(LUA_TNAME("test1"));
        auto f2 = l.extract<std::optional<int>(std::optional<int>)>(LUA_TNAME("test2"));

        REQUIRE(f1(200) == 200);
        REQUIRE(!f2({}).has_value());
    }

    SECTION("field not exists") {
        auto l = luactx(lua_code{"one = 3 two = {three = {four = 4}}"});
        auto top = l.top();
        bool catched = false;
        try {
            l.extract<int>(LUA_TNAME("one.two.three"));
        }
        catch (const errors::access_error&) {
            catched = true;
        }
        REQUIRE(catched);
        REQUIRE(l.top() == top);

        catched = false;
        try {
            REQUIRE(l.extract<int>(lua_name("two.three.four")) == 4);
        }
        catch (const errors::access_error&) {
            catched = true;
        }
        REQUIRE(!catched);
        REQUIRE(l.top() == top);
    }
}
