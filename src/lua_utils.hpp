#pragma once

#include <stdexcept>

template <typename F>
struct luacpp_exception_guard {
    luacpp_exception_guard(F ifinalizer): finalizer(std::move(ifinalizer)) {}
    ~luacpp_exception_guard() {
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
struct luacpp_finalizer {
    luacpp_finalizer(F ifinalizer): finalizer(std::move(ifinalizer)) {}
    ~luacpp_finalizer() {
        if (!dismissed)
            finalizer();
    }

    void dismiss() {
        dismissed = true;
    }

    F    finalizer;
    bool dismissed = false;
};
