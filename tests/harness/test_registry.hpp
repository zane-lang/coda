#pragma once

#include "test_utils.hpp"
#include "test_data.hpp"

#include <functional>
#include <string>
#include <vector>
#include <utility>

namespace test_framework {

// ─── Test case descriptor ─────────────────────────────────────────────────────

struct TestCase {
	std::string name;
	std::function<bool()> predicate;         // returns true = pass
};

struct ThrowsCase {
	std::string name;
	std::function<void()> action;            // must throw to pass
};

struct ThrowsMsgCase {
	std::string name;
	std::string src;
	std::vector<std::string> needles;
};

struct ThrowsCodeCase {
	std::string name;
	std::string src;
	int expected_code;                       // cast from enum at registration
};

struct Suite {
	std::string name;
	std::vector<TestCase> checks;
	std::vector<ThrowsCase> throws;
	std::vector<ThrowsMsgCase> throws_msg;
	std::vector<ThrowsCodeCase> throws_code;
};

// ─── Registry ─────────────────────────────────────────────────────────────────

inline std::vector<Suite>& registry() {
	static std::vector<Suite> suites;
	return suites;
}

// ─── Builder (fluent API) ─────────────────────────────────────────────────────

class SuiteBuilder {
	Suite s_;
public:
	explicit SuiteBuilder(std::string name) { s_.name = std::move(name); }

	SuiteBuilder& check(std::string name, std::function<bool()> pred) {
		s_.checks.push_back({ std::move(name), std::move(pred) });
		return *this;
	}

	SuiteBuilder& check_throws(std::string name, std::function<void()> action) {
		s_.throws.push_back({ std::move(name), std::move(action) });
		return *this;
	}

	SuiteBuilder& check_throws_msg(std::string name, std::string src,
	                                std::vector<std::string> needles) {
		s_.throws_msg.push_back({ std::move(name), std::move(src), std::move(needles) });
		return *this;
	}

	SuiteBuilder& check_throws_code(std::string name, std::string src, int code) {
		s_.throws_code.push_back({ std::move(name), std::move(src), code });
		return *this;
	}

	void commit() {
		registry().push_back(std::move(s_));
	}
};

inline SuiteBuilder define_suite(std::string name) {
	return SuiteBuilder(std::move(name));
}

// ─── Runner (implemented per-binary) ──────────────────────────────────────────
// Each test binary provides run_all_suites() which knows how to execute
// checks, throws, throws_msg, throws_code using its own parse function.

void run_all_suites();

} // namespace test_framework
