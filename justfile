# ----------------------------------------------------------------
# Variables
# ----------------------------------------------------------------

zig_cc  := `pwd` + "/scripts/zig-cc"
zig_cxx := `pwd` + "/scripts/zig-cxx"
zig_ar  := `pwd` + "/scripts/zig-ar"

# pick your C++ standard
flags       := "-O3 -std=c++20"
flags_cross := "-O3 -std=c++20"

inc := "-Iinclude"

targets := "x86_64-linux-gnu x86_64-linux-musl aarch64-linux-gnu aarch64-linux-musl x86_64-windows-gnu aarch64-windows-gnu"

default:
	@just --list

test:
	mkdir -p build
	clang++ -std=c++20 -I. tests/test_main.cpp -o build/tests
	./build/tests

# Test the C FFI
test-c-ffi:
	mkdir -p build
	# Build native static lib (not cross-compiled)
	clang++ -std=c++20 -c -fPIC -I. ffi/coda_ffi.cpp -o build/coda_ffi.o
	ar rcs build/libcoda_ffi_native.a build/coda_ffi.o
	# Build and run test
	clang++ -std=c++20 -I. tests/test_c_ffi.cpp build/libcoda_ffi_native.a -o build/test_c_ffi
	./build/test_c_ffi

# Test both FFI layers
test-ffi: test-c-ffi

# Run all tests
test-all: cross-all
	- just test
	- just test-ffi


run: generate
	mkdir -p build
	clang++ -std=c++20 -I. tests/run.cpp -o build/run
	./build/run

generate:
	quom src/coda.hpp include/coda.hpp

ffi: cross-all
	@echo "Running FFI example..."
	@python3 tests/ffi/main.py

# ----------------------------------------------------------------
# Host build
# ----------------------------------------------------------------
build:
	#!/usr/bin/env sh
	set -e
	mkdir -p build

	{{zig_cxx}} {{flags}} {{inc}} -fPIC -shared \
		-o build/libcoda_ffi.so \
		ffi/coda_ffi.cpp

# ----------------------------------------------------------------
# Cross build for one target
# ----------------------------------------------------------------
cross target:
	#!/usr/bin/env sh
	set -e

	mkdir -p dist/{{target}}

	{{zig_cxx}} {{flags_cross}} {{inc}} \
		-target {{target}} \
		-fPIC \
		-c -o dist/{{target}}/coda_ffi.o \
		ffi/coda_ffi.cpp

	case "{{target}}" in
		*windows*)
			{{zig_ar}} rcs dist/{{target}}/libcoda_ffi.lib dist/{{target}}/coda_ffi.o

			{{zig_cxx}} -target {{target}} -shared \
				-Wl,--whole-archive dist/{{target}}/libcoda_ffi.lib -Wl,--no-whole-archive \
				-lkernel32 -lws2_32 \
				-o dist/{{target}}/libcoda_ffi.dll

			echo "→ dist/{{target}}/libcoda_ffi.lib"
			echo "→ dist/{{target}}/libcoda_ffi.dll"
		;;
		*)
			{{zig_ar}} rcs dist/{{target}}/libcoda_ffi.a dist/{{target}}/coda_ffi.o

			{{zig_cxx}} -target {{target}} -shared -fPIC \
				-o dist/{{target}}/libcoda_ffi.so \
				dist/{{target}}/coda_ffi.o \
				-lpthread

			echo "→ dist/{{target}}/libcoda_ffi.a"
			echo "→ dist/{{target}}/libcoda_ffi.so"
		;;
	esac

	rm dist/{{target}}/coda_ffi.o


cross-all: generate
	just clean
	@for t in {{targets}}; do just cross $t; done
	just install

install:
	sudo cp dist/x86_64-linux-gnu/libcoda_ffi.so /usr/local/lib/

# ----------------------------------------------------------------
# Clean build artifacts 
# ----------------------------------------------------------------

clean:
	rm -rf build dist libcoda_ffi.a
