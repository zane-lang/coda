#include "../../src/coda.hpp"
#include <iostream>

int main (int argc, char *argv[]) {
	Coda coda("tests/catalog/example.coda");
	std::cout << coda.serialize();
	coda.save("tests/catalog/example.coda");
	return 0;
}
