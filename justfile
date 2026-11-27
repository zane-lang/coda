# Build/test entry points for Coda.
#
# Layering: devbox (toolchain: zig, just, python3, dune, ocaml, crystal)
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

test-crystal:
	python3 scripts/tasks.py test-crystal

# The static archive the Crystal binding links: build/crystal/libcoda_ffi.a.
build-crystal:
	python3 scripts/tasks.py build-crystal

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

# Check prose with the prose-lint commit and lockfile that CI pins in prose.yml.
prose:
	#!/usr/bin/env bash
	set -euo pipefail
	command -v uv >/dev/null || { echo 'just prose needs uv on PATH: https://docs.astral.sh/uv/' >&2; exit 1; }
	dir="${XDG_CACHE_HOME:-$HOME/.cache}/prose-lint/107c5f768ea3ce4936d6fdff1b0f30fac9237615"
	if [ ! -d "$dir" ]; then
		rm -rf "$dir.tmp"
		git clone -q https://github.com/zane-lang/prose-lint "$dir.tmp"
		git -C "$dir.tmp" checkout -q 107c5f768ea3ce4936d6fdff1b0f30fac9237615
		mv "$dir.tmp" "$dir"
	fi
	uv run --frozen --project "$dir" prose-lint .
