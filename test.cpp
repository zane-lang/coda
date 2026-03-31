#include "include/coda.hpp"
#include <iostream>

int main() {
    Coda coda("test.coda");

    std::cout << "\n--- serialized ---\n";
    std::cout << coda.file.serialize();

    return 0;
}
