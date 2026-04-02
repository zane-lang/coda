# ----------------------------------------------------------------
# Variables
# ----------------------------------------------------------------

zig_cc  := `pwd` + "/scripts/zig-cc"
zig_cxx := `pwd` + "/scripts/zig-cxx"
zig_ar  := `pwd` + "/scripts/zig-ar"

test_flags := "-O0 -g -std=c++20"
flags       := "-O3 -std=c++20"
flags_cross := "-O3 -std=c++20"

inc := "-Iinclude"

targets := "x86_64-linux-gnu x86_64-linux-musl aarch64-linux-gnu aarch64-linux-musl x86_64-windows-gnu aarch64-windows-gnu"

default:
	@just --list

test-cpp:
	mkdir -p build
	{{zig_cxx}} {{test_flags}} -I. tests/cpp/test_main.cpp -o build/tests
	./build/tests

test-c-ffi:
	mkdir -p build
	# Build native static lib (not cross-compiled) because of name collisions
	{{zig_cxx}} {{test_flags}} -I. -c -fPIC ffi/coda_ffi.cpp -o build/coda_ffi.o
	{{zig_ar}} rcs build/libcoda_ffi_native.a build/coda_ffi.o
	# Build and run test
	{{zig_cxx}} {{test_flags}} -I. tests/cpp/test_c_ffi.cpp build/libcoda_ffi_native.a -o build/test_c_ffi
	./build/test_c_ffi

# Test Python FFI
test-py-ffi:
	@python3 tests/python/test_python_ffi.py

# Run all tests
test: cross-all
	- just test-cpp
	- just test-c-ffi
	- just test-py-ffi


run: generate
	mkdir -p build
	{{zig_cxx}} {{test_flags}} -I. tests/run/cpp.cpp -o build/run
	./build/run

generate:
	quom src/coda.hpp include/coda.hpp

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
	@echo "Cross build done. Artifacts are in dist/."
	just install

install libdir="/usr/local/lib":
	sudo install -d "{{libdir}}"
	sudo install -m 755 dist/x86_64-linux-gnu/libcoda_ffi.so "{{libdir}}/"

# ----------------------------------------------------------------
# Clean build artifacts 
# ----------------------------------------------------------------

clean:
	rm -rf build dist libcoda_ffi.a coda_ffi.o
