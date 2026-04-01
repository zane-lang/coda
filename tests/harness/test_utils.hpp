#pragma once

// ─── Test framework ───────────────────────────────────────────────────────────
// Provides: suite(), pass(), fail(), print_summary()
// Counters are global within a single translation-unit run (one binary = one
// set of counters).  Include this header in every test binary; do NOT include
// it in more than one .cpp per binary or you will get ODR violations on the
// inline functions.

#include <cstdio>

// ANSI colour helpers
#define ANSI_GREEN  "\033[32m"
#define ANSI_RED    "\033[31m"
#define ANSI_YELLOW "\033[33m"
#define ANSI_BLUE   "\033[34m"
#define ANSI_BOLD   "\033[1m"
#define ANSI_RESET  "\033[0m"

namespace test_framework {

inline int passed = 0;
inline int failed = 0;

inline void suite(const char* name) {
	std::printf("\n" ANSI_YELLOW "[%s]" ANSI_RESET "\n", name);
}

inline void pass(const char* name) {
	++passed;
	std::printf("  " ANSI_GREEN "✓" ANSI_RESET "  %s\n", name);
}

inline void fail(const char* name, const char* reason) {
	++failed;
	std::printf("  " ANSI_RED "✗" ANSI_RESET "  %s\n      %s\n", name, reason);
}

inline void print_summary() {
	std::printf("\n══════════════════════════════\n");
	std::printf("  " ANSI_GREEN "Passed: %d" ANSI_RESET "\n", passed);
	if (failed > 0)
		std::printf("  " ANSI_RED "Failed: %d" ANSI_RESET "\n", failed);
	else
		std::printf("  Failed: %d\n", failed);
	std::printf("══════════════════════════════\n");
}

} // namespace test_framework
