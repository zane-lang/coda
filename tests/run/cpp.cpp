#include "../../src/coda.hpp"
#include <iostream>

int main (int argc, char *argv[]) {
	Coda coda;
	coda.root()["test"] = coda::KeyedTable({ "age" })
		.insert(
			"hello",
			coda::Row().insert("age", "20"))
		.insert(
			"bye",
			coda::Row().insert("age", "20"))
	;
	std::cout << coda.serialize();
	return 0;
}
