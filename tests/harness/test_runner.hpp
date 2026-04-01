#pragma once

#include "test_checks.hpp"
#include "test_utils.hpp"

#include <exception>
#include <string>

namespace test_framework {

inline void run_catalog(ParseAdapter& adapter) {
	auto catalog = build_catalog();
	std::string current_suite;

	for (const auto& entry : catalog) {
		if (entry.suite != current_suite) {
			current_suite = entry.suite;
			suite(current_suite.c_str());
		}

		try {
			if (entry.run(adapter)) {
				pass(entry.name.c_str());
			} else {
				fail(entry.name.c_str(), "returned false");
			}
		} catch (const std::exception& e) {
			fail(entry.name.c_str(),
			     (std::string("unexpected exception: ") + e.what()).c_str());
		} catch (...) {
			fail(entry.name.c_str(), "unknown exception");
		}
	}
}

} // namespace test_framework
