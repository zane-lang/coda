#pragma once

// ─── Test macros & Coda-specific check helpers ────────────────────────────────
// Depends on: test_utils.hpp (for pass() / fail())
//             coda.hpp       (for coda::ParseError, coda::ParseErrorCode)
//
// Include this header in any translation unit that writes Coda tests.
// The helpers that call parse() are C++-only; the plain CHECK/CHECK_THROWS
// macros work in both C++ and C FFI test files.

#include "test_utils.hpp"
#include "../../include/coda.hpp"

#include <exception>
#include <initializer_list>
#include <sstream>
#include <string>
#include <string_view>

using namespace test_framework;

// ─── Generic assertion macros ─────────────────────────────────────────────────

// Evaluate expr; pass if truthy, fail with the expression text otherwise.
// Any exception is caught and reported as a failure.
#define CHECK(name, ...)                                                        \
	do {                                                                        \
		try {                                                                   \
			if (__VA_ARGS__) { pass(name); }                                    \
			else { fail(name, "expression was false: " #__VA_ARGS__); }         \
		} catch (const std::exception& _e) {                                    \
			fail(name, (std::string("unexpected exception: ") + _e.what()).c_str()); \
		}                                                                       \
	} while (0)

// Pass if expr throws anything; fail if it completes normally.
#define CHECK_THROWS(name, expr)                                                \
	do {                                                                        \
		try {                                                                   \
			(void)(expr);                                                       \
			fail(name, "expected an exception but none was thrown");             \
		} catch (...) {                                                         \
			pass(name);                                                         \
		}                                                                       \
	} while (0)

// Pass if expr throws exactly ExType; fail on wrong type or no throw.
#define CHECK_THROWS_TYPE(name, ExType, expr)                                   \
	do {                                                                        \
		try {                                                                   \
			(void)(expr);                                                       \
			fail(name, (std::string("expected " #ExType " but nothing was thrown")).c_str()); \
		} catch (const ExType&) {                                               \
			pass(name);                                                         \
		} catch (const std::exception& _e) {                                    \
			fail(name, (std::string("wrong exception type (std::exception): ") + _e.what()).c_str()); \
		} catch (...) {                                                         \
			fail(name, "wrong exception type (non-std exception)");             \
		}                                                                       \
	} while (0)

// ─── Coda-specific parse helpers ─────────────────────────────────────────────

// Convenience wrapper so helpers below don't repeat the construction.
inline coda::CodaFile coda_parse(const std::string& src) {
	return coda::detail::Parser(src).parse();
}

// Pass if parsing src throws coda::ParseError whose message contains at least
// one of the given needle strings.
inline void check_parse_throws_msg(
	const std::string&              test_name,
	const std::string&              src,
	std::initializer_list<std::string_view> needles)
{
	try {
		(void)coda_parse(src);
		fail(test_name.c_str(), "expected coda::ParseError but parsing succeeded");
	} catch (const coda::ParseError& e) {
		std::string_view msg(e.what());
		bool ok = (needles.size() == 0);
		for (auto n : needles) ok = ok || (msg.find(n) != std::string_view::npos);
		if (!ok) {
			std::ostringstream oss;
			oss << "ParseError message did not contain any expected substrings.\n"
			    << "  message: " << e.what();
			fail(test_name.c_str(), oss.str().c_str());
		} else {
			pass(test_name.c_str());
		}
	} catch (const std::exception& e) {
		fail(test_name.c_str(),
		     (std::string("wrong exception type (std::exception): ") + e.what()).c_str());
	} catch (...) {
		fail(test_name.c_str(), "wrong exception type (non-std exception)");
	}
}

// Pass if parsing src throws coda::ParseError with e.code == expected_code.
inline void check_parse_throws_code(
	const std::string&   test_name,
	const std::string&   src,
	coda::ParseErrorCode expected_code)
{
	try {
		(void)coda_parse(src);
		fail(test_name.c_str(), "expected coda::ParseError but parsing succeeded");
	} catch (const coda::ParseError& e) {
		if (e.code == expected_code) {
			pass(test_name.c_str());
		} else {
			std::ostringstream oss;
			oss << "ParseError code mismatch. Got message: " << e.what();
			fail(test_name.c_str(), oss.str().c_str());
		}
	} catch (const std::exception& e) {
		fail(test_name.c_str(),
		     (std::string("wrong exception type (std::exception): ") + e.what()).c_str());
	} catch (...) {
		fail(test_name.c_str(), "wrong exception type (non-std exception)");
	}
}

// Pass if parse → serialize → parse → serialize produces the same output twice.
inline bool roundtrip_equal(const std::string& src) {
	auto        f1   = coda_parse(src);
	std::string ser1 = f1.serialize();
	auto        f2   = coda_parse(ser1);
	std::string ser2 = f2.serialize();
	return ser1 == ser2;
}
