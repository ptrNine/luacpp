#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cmath>

template <typename T, typename U, typename E = decltype(T{} + U{})>
constexpr bool approx_eq(T a, U b, E epsilon = std::numeric_limits<E>::epsilon() * 100) {
    return std::fabs(a - b) <= ((std::fabs(a) < std::fabs(b) ? std::fabs(b) : std::fabs(a)) * epsilon);
}

template <typename T>
struct vector3 {
    constexpr vector3() noexcept = default;
    constexpr vector3(T v) noexcept: x(v), y(v), z(v) {}
    constexpr vector3(T ix, T iy, T iz) noexcept: x(ix), y(iy), z(iz) {}

    template <typename U>
    constexpr auto operator+(const vector3<U>& v) const noexcept {
        return vector3{x + v.x, y + v.y, z + v.z};
    }

    template <typename U>
    constexpr auto operator-(const vector3<U>& v) const noexcept {
        return vector3{x - v.x, y - v.y, z - v.z};
    }

    template <typename U>
    constexpr auto operator*(U n) const noexcept {
        return vector3{x * n, y * n, z * n};
    }

    template <typename U>
    friend constexpr auto operator*(U n, const vector3& vec) noexcept {
        return vector3{vec.x * n, vec.y * n, vec.z * n};
    }

    template <typename U>
    constexpr auto operator/(U n) const noexcept {
        return vector3{x / n, y / n, z / n};
    }

    template <typename U>
    constexpr auto dot(const vector3<U>& v) const noexcept {
        return x * v.x + y * v.y + z * v.z;
    }

    constexpr auto magnitude2() const noexcept {
        return dot(*this);
    }

    auto magnitude() const noexcept {
        auto m2 = magnitude2();
        return std::sqrt(sizeof(T) > 4 ? double(m2) : float(m2));
    }

    template <typename U>
    constexpr auto cross(const vector3<U>& v) const noexcept {
        return vector3{
            y * v.z - z * v.y,
            z * v.x - x * v.z,
            x * v.y - y * v.x
        };
    }

    template <typename U>
    constexpr bool operator==(const vector3<U>& v) const noexcept {
        return approx_eq(x, v.x) && approx_eq(y, v.y) && approx_eq(z, v.z);
    }

    T x, y, z;
};


#include "luacpp_basic.hpp"


using luavec3 = vector3<double>;

template <>
struct luacpp::typespec_list_s<0> {
    using type = std::tuple<typespec<luavec3, LUA_TNAME("vec3")>>;
};

#include "luacpp_ctx.hpp"

using namespace Catch::literals;
using namespace luacpp;

void lua_setup_usertypes(luactx& l) {
    auto memtable = member_table<luavec3>{{"x",
                                           /* Weird, put pretty flexible */
                                           {[](const luavec3& v, luactx& ctx) { ctx.push(v.x); },
                                            [](luavec3& v, luactx& ctx) {
                                                ctx.get_new(v.x);
                                            }}},
                                          /* lua_getsetez may be used for simple member cases */
                                          lua_getsetez(y),
                                          lua_getsetez(z)};
    l.set_member_table(memtable);

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
}
