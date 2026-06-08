#pragma once

// ─── Generic assertion macros for the C++/C-FFI test binaries ─────────────────
// Depends on framework.hpp for pass()/fail(). These are the only ad-hoc
// assertions used outside the catalog-driven runner (e.g. lifecycle/ABI checks
// in the C FFI binary). Catalog tests should be added to catalog.coda instead.

#include "framework.hpp"

#include <exception>
#include <string>

using namespace test_framework;

// Evaluate expr; pass if truthy, fail with the expression text otherwise.
// Any exception is caught and reported as a failure.
#define CHECK(name, ...)                                                         \
	do {                                                                         \
		try {                                                                    \
			if (__VA_ARGS__) { pass(name); }                                     \
			else { fail(name, "expression was false: " #__VA_ARGS__); }          \
		} catch (const std::exception& _e) {                                     \
			fail(name, (std::string("unexpected exception: ") + _e.what()).c_str()); \
		}                                                                        \
	} while (0)

// Pass if expr throws anything; fail if it completes normally.
#define CHECK_THROWS(name, expr)                                                 \
	do {                                                                         \
		try {                                                                    \
			(void)(expr);                                                        \
			fail(name, "expected an exception but none was thrown");             \
		} catch (...) {                                                          \
			pass(name);                                                          \
		}                                                                        \
	} while (0)
