#pragma once

#include <stdexcept>

namespace luacpp {

template <typename F>
struct exception_guard {
    exception_guard(F ifinalizer): finalizer(std::move(ifinalizer)) {}
    ~exception_guard() {
        if (!dismissed && std::uncaught_exceptions() != uncaught)
            finalizer();
    }

    void dismiss() {
        dismissed = true;
    }

    F    finalizer;
    int  uncaught  = std::uncaught_exceptions();
    bool dismissed = false;
};

template <typename F>
struct finalizer {
    finalizer(F ifinalizer): finalizer_f(std::move(ifinalizer)) {}
    ~finalizer() {
        if (!dismissed)
            finalizer_f();
    }

    void dismiss() {
        dismissed = true;
    }

    F    finalizer_f;
    bool dismissed = false;
};

} // namespace luacpp
