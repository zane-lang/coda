# Build/test entry points for Coda.
#
# Layering: devbox (toolchain: zig, just, python3, dune, ocaml)
#             -> just (this file, task names)
#               -> scripts/tasks.py (the actual build/test logic)
#
# Canonical invocation inside CI and locally:
#   devbox run -- just test      # full suite in the devbox environment
#
# include/coda.hpp is a generated amalgamation (git-ignored); the test/build
# recipes regenerate it on demand, so a clean checkout works.

default:
	@just --list

list:
	@just --list

generate:
	python3 scripts/tasks.py generate

build:
	python3 scripts/tasks.py build

test-cpp: generate
	python3 scripts/tasks.py test-cpp

test-c-ffi: generate
	python3 scripts/tasks.py test-c-ffi

test-py-ffi:
	python3 scripts/tasks.py test-py-ffi

test-ocaml:
	python3 scripts/tasks.py test-ocaml

test: cross-all
	python3 scripts/tasks.py test

run: generate
	python3 scripts/tasks.py run

cross target:
	python3 scripts/tasks.py cross {{target}}

cross-all: generate
	python3 scripts/tasks.py cross-all

install libdir="/usr/local/lib":
	python3 scripts/tasks.py install {{libdir}}

clean:
	python3 scripts/tasks.py clean
