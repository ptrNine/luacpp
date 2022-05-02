#pragma once

#include <map>
#include <vector>
#include <string>

#define lua_getsetez(lua_or_cpp_name)                                                                                  \
    {                                                                                                                  \
        #lua_or_cpp_name, {                                                                                            \
            [](auto v, auto ctx) { ctx.push(v.lua_or_cpp_name); }, [](auto v, auto ctx) {                              \
                ctx.get_new(v.lua_or_cpp_name);                                                                        \
            }                                                                                                          \
        }                                                                                                              \
    }

#define lua_getez(lua_or_cpp_name)                                                                                     \
    {                                                                                                                  \
        #lua_or_cpp_name, {                                                                                            \
            [](auto v, auto ctx) {                                                                                     \
                ctx.push(v.lua_or_cpp_name);                                                                           \
            }                                                                                                          \
        }                                                                                                              \
    }

#define LUA_GETSET(usertype, lua_name, cpp_name)                                                                       \
    std::pair<std::string, luacpp::getset<usertype>> {                                                                 \
        lua_name, {                                                                                                    \
            [](const luacpp::usertype& v, luacpp::luactx& ctx) { ctx.push(v.cpp_name); },                              \
                [](luacpp::usertype& v, luacpp::luactx& ctx) {                                                         \
                    ctx.get_new(v.cpp_name);                                                                           \
                }                                                                                                      \
        }                                                                                                              \
    }

#define LUA_GET(usertype, lua_name, cpp_name)                                                                          \
    std::pair<std::string, luacpp::getset<usertype>> {                                                                 \
        lua_name, {                                                                                                    \
            [](const luacpp::usertype& v, luacpp::luactx& ctx) {                                                       \
                ctx.push(v.cpp_name);                                                                                  \
            }                                                                                                          \
        }                                                                                                              \
    }

namespace luacpp {

class luactx;

template <typename T>
struct getter {
    void operator()(const T& usertype_value, luactx& ctx) const {
        func(usertype_value, ctx);
    }
    void (*func)(const T&, luactx&);
};

template <typename T>
struct setter {
    setter() = default;
    setter(auto f): func(f) {}

    void operator()(T& usertype_value, luactx& ctx) const {
        func(usertype_value, ctx);
    }
    operator bool() const {
        return func;
    }
    void (*func)(T&, luactx&) = nullptr;
};

template <typename T>
struct getset {
    getset(auto getter): get{getter} {}
    getset(auto getter, auto setter): get{getter}, set(setter) {}

    getter<T> get;
    setter<T> set;
};

template <typename T>
using member_table = std::map<std::string, getset<T>>;

template <typename T>
using ordered_member_table = std::vector<std::pair<std::string, getset<T>>>;

} // namespace luacpp
