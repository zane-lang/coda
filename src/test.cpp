#include "zif.hpp"
#include <cassert>
#include <iostream>

int main() {
	Zif zif("test.zif");
	zif["compiler"]["debug"] = "hi";
	std::cout << (zif["compiler"]["debug"].asString());
	zif.save("test.zif");
	return 0;
}
