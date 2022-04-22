#pragma once

#include <cstddef>

namespace luacpp
{
constexpr auto to_int(char c) {
    if (c >= 'A' && c <= 'Z')
        return c - 'A' + 10;
    else if (c >= 'a' && c <= 'z')
        return c - 'a' + 10;
    else
        return c - '0';
}

template <size_t o = 0, size_t max, size_t s>
constexpr auto parse_int_n(const char (&str)[s]) {
    int    base   = 10;
    size_t offset = o;

    if (str[0] == '0' && o + max != 1) {
        switch (str[1]) {
        case 'b':
            base = 2;
            offset += 2;
            break;
        case 'x':
            base = 16;
            offset += 2;
            break;
        default: base = 8; offset += 1;
        }
    }

    long long int result = 0;
    for (size_t i = offset; i < offset + max; ++i) {
        if (str[i] == '\'')
            continue;
        result *= base;
        result += to_int(str[i]);
    }

    return result;
}

template <size_t o = 0, size_t s>
constexpr auto parse_int(const char (&str)[s]) {
    return parse_int_n<o, s - o, s>(str);
}

} // namespace luacpp
