#pragma once

#include <map>
#include "luawrap.hpp"

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
    std::pair<std::string, luacpp_getset<usertype>> {                                                                  \
        lua_name, {                                                                                                    \
            [](const usertype& v, luactx& ctx) { ctx.push(v.cpp_name); }, [](usertype& v, luactx& ctx) {               \
                ctx.get_new(v.cpp_name);                                                                               \
            }                                                                                                          \
        }                                                                                                              \
    }

#define LUA_GET(usertype, lua_name, cpp_name)                                                                          \
    std::pair<std::string, luacpp_getset<usertype>> {                                                                  \
        lua_name, {                                                                                                    \
            [](const usertype& v, luactx& ctx) {                                                                       \
                ctx.push(v.cpp_name);                                                                                  \
            }                                                                                                          \
        }                                                                                                              \
    }

class luactx;

template <typename T>
struct luacpp_getter {
    void operator()(const T& usertype_value, luactx& ctx) const {
        func(usertype_value, ctx);
    }
    void (*func)(const T&, luactx&);
};

template <typename T>
struct luacpp_setter {
    void operator()(T& usertype_value, luactx& ctx) const {
        func(usertype_value, ctx);
    }
    operator bool() const {
        return func;
    }
    void (*func)(T&, luactx&) = nullptr;
};

template <typename T>
struct luacpp_getset {
    luacpp_getset(auto getter): get{getter} {}
    luacpp_getset(auto getter, auto setter): get{getter}, set(setter) {}

    luacpp_getter<T> get;
    luacpp_setter<T> set;
};

template <typename T>
using luacpp_member_table = std::map<std::string, luacpp_getset<T>>;
