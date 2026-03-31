#include "coda.hpp"
#include <iostream>

int main() {
    Coda coda("test.coda");

    // Check where the comment actually is
    std::cout << "compiler.comment: [" << coda["compiler"].comment << "]\n";
    std::cout << "ö.comment: [" << coda["compiler"]["ö"].comment << "]\n";

    std::cout << "\n--- serialized ---\n";
    std::cout << coda.file.serialize();

    return 0;
}
