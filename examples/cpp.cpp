#include "../../src/coda.hpp"
#include <iostream>

int main (int argc, char *argv[]) {
	coda::Doc doc;
	doc.root()["test"] = coda::KeyedTable({ "age" })
		.insert(
			"hello",
			coda::Row().insert("age", "20"))
		.insert(
			"bye",
			coda::Row().insert("age", "20"))
	;
	std::cout << doc.serialize();
	return 0;
}
