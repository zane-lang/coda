#include "../../src/coda.hpp"
#include <iostream>

int main (int argc, char *argv[]) {
	Coda coda("tests/catalog/catalog.coda");
	std::cout << coda.serialize();
	return 0;
}
