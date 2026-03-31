test:
	clang++ test.cpp -o build/main
	./build/main

generate:
	quom src/coda.hpp include/coda.hpp
