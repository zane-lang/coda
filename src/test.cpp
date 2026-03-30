#include "coda.hpp"
#include <cassert>
#include <iostream>

int main() {
	Coda coda("test.coda");
	std::cout << (std::string)coda["deps"]["http"]["link"];
	coda.save("test.coda");
	return 0;
}
