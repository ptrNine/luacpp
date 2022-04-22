#pragma once

#include "luacpp_parse_int.hpp"

namespace luacpp
{
#define BINOP(OP)                                                                                                      \
    template <auto c2>                                                                                                 \
    constexpr auto operator OP(int_const<c2> rhs) const {                                                              \
        return int_c<(c OP rhs)>;                                                                                      \
    }                                                                                                                  \
    constexpr auto operator OP(auto rhs) const { return c OP rhs; }

#define UNOP(OP)                                                                                                       \
    constexpr auto operator OP() const { return int_c<OP c>; }

template <auto>
struct int_const;

template <auto v>
static inline constexpr auto int_c = int_const<v>{};

template <auto c>
struct int_const {
    static constexpr auto value = c;

    constexpr operator auto() const {
        return c;
    }

    BINOP(+)
    BINOP(-)
    BINOP(*)
    BINOP(/)
    BINOP(%)
    BINOP(&&)
    BINOP(||)
    BINOP(^)
    BINOP(|)
    BINOP(&)
    BINOP(>>)
    BINOP(<<)

    BINOP(==)
    BINOP(!=)
    BINOP(>)
    BINOP(>=)
    BINOP(<=)
    BINOP(<)

    UNOP(-)
    UNOP(+)
    UNOP(~)
    UNOP(!)
};

#undef BINOP
#undef UNOP

using true_t = int_const<true>;
using false_t = int_const<false>;

static constexpr inline auto true_c  = int_c<true>;
static constexpr inline auto false_c = int_c<false>;

namespace literals
{
    template <char... cs>
    constexpr auto operator"" _c() {
        constexpr auto c = parse_int<0, sizeof...(cs)>({cs...});
        return int_c<c>;
    }
} // namespace literals

} // namespace luacpp
