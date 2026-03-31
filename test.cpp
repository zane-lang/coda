#include "include/coda.hpp"
#include <iostream>

int main() {
    Coda coda("test.coda");

	coda.order();
	coda.save("test.coda");

    return 0;
}
