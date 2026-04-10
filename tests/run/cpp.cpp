#include "../../src/coda.hpp"
#include <iostream>

int main (int argc, char *argv[]) {
	Coda coda;
	coda["test"] = coda::CodaTable(["age"]).insert("peter", coda::CodaRow().insert("age", "20"))
	std::cout << coda.serialize();
	return 0;
}
