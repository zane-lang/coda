#include "zif.hpp"
#include <cassert>
#include <iostream>

int main() {
	Zif zif("test.zif");
	std::cout << (zif["compiler"]["debug"].asString());
	zif.save("test.zif");
	return 0;
}
