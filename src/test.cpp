#include "coda.hpp"
#include <cassert>
#include <iostream>

int main() {
	Coda coda("test.coda");
	std::cout << (coda["test"][0]["link"].asString());
	coda.save("test.coda");
	return 0;
}
