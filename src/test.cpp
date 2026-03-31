#include "coda.hpp"
#include <cassert>
#include <iostream>

int main() {
	Coda coda("test.coda");

	// deps is a keyed CodaTable
	for (auto& [k, v] : coda["deps"].asTable()) {
		std::cout << k << " -> " << v["link"].asString() << "\n";
	}

	// coda.save("test.coda");
	return 0;
}
