#pragma once

#include <string>
#include <cmath>

template <typename T, typename U, typename E = decltype(T{} + U{})>
constexpr bool approx_eq(T a, U b, E epsilon = std::numeric_limits<E>::epsilon() * 100) {
    return std::fabs(a - b) <= ((std::fabs(a) < std::fabs(b) ? std::fabs(b) : std::fabs(a)) * epsilon);
}

struct string_like {
    std::string str;
    string_like(std::string_view s): str(s) {}
    operator std::string_view() const {
        return str;
    }
    bool operator==(const string_like& v) const {
        return str == v.str;
    }
    std::string test() const {
        return "test " + str;
    }
};

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
    using type = std::tuple<typespec<luavec3, LUA_TNAME("vec3")>, typespec<string_like, LUA_TNAME("string_like")>>;
};

#include "luacpp_ctx.hpp"

