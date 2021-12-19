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
        [] { return luavec3(); },
        [](const luavec3& v) { return v; }, // deep-copy
        [](double v) { return luavec3(v); },
        [](double x, double y, double z) { return luavec3(x, y, z); });

    l.provide(LUA_TNAME("__add"), &luavec3::operator+<double>);
    l.provide(LUA_TNAME("__sub"), &luavec3::operator-<double>);
    l.provide(LUA_TNAME("__eq"), &luavec3::operator==<double>);
    l.provide(LUA_TNAME("magnitude"), &luavec3::magnitude);
    l.provide(LUA_TNAME("cross"), &luavec3::cross<double>);
    l.provide_member<luavec3>(LUA_TNAME("__tostring"), [](const luavec3& v) {
        return std::to_string(v.x) + " " + std::to_string(v.y) + " " + std::to_string(v.z);
    });
}

auto code = R"(
function check()
    v1 = vec3.new()
    v2 = vec3.new(2)
    v3 = vec3.new(1, 2, 3)
    v4 = vec3.new(v3);
    v5 = v4:new();

    assert(v3 == v4)
    assert(v4 == v5)

    print(v1, v2, v3)

    a1 = v1 + v3
    a2 = v1 - v3
    print(a1, a2)

    a3 = v2 + v3
    a4 = v2 - v3
    print(a3, a4)

    print(v3:magnitude())
    print(v3:cross(v3))
end
)";

TEST_CASE("usertypes") {
    auto l = luactx(lua_code{code});
    lua_setup_usertypes(l);

    SECTION("members") {
        l.extract<void()>(LUA_TNAME("check"))();
    }
}
