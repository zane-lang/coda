#include "../../include/coda.hpp"
int main (int argc, char *argv[]) {
	Coda coda("tests/catalog/example.coda");
	coda.order();
	coda.save("tests/catalog/example.coda");
	return 0;
}
