#include "../include/coda.hpp"
int main (int argc, char *argv[]) {
	Coda coda("tests/example.coda");
	coda.order();
	coda.save("tests/example.coda");
	return 0;
}
