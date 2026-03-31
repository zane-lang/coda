test:
	mkdir -p build
	clang++ -std=c++20 -I. tests/test_main.cpp -o build/tests
	./build/tests

generate:
	quom src/coda.hpp include/coda.hpp
