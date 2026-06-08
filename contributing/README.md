# Contributing to Coda

This is the canonical place for repo-specific standards. Read it before making
changes.

## Repository layout

```
src/                 C++ source of truth (header-only library)
  coda.hpp           umbrella include + quom amalgamation entry point
  ast.hpp parser.hpp
  helpers/           OrderedMap, Variant
include/             GENERATED single-header amalgamation (git-ignored)
ffi/                 C ABI (coda_ffi.h / coda_ffi.cpp)
bindings/            shippable language bindings ONLY (no tests here)
  python/  ocaml/
tests/               tests ONLY
  catalog/           catalog.coda  <-- single source of truth for test cases
  harness/cpp/       C++ harness (framework / adapter / runner / macros)
  harness/python/    Python harness
  cpp/               test_cpp.cpp (C++ API)
  c/                 test_c_ffi.cpp (C ABI — C++-written, exercises the C FFI)
  python/            test_python_ffi.py
  ocaml/             test_ocaml.ml (catalog-driven)
scripts/             tasks.py (build/test logic) + zig wrappers
examples/  docs/  highlighted/
```

## Build & test

Everything runs inside a Devbox environment that pins the toolchain
(zig, just, python3, dune, ocaml).

```bash
devbox run -- just test        # full suite (this is what CI runs)
devbox run -- just test-cpp
devbox run -- just test-c-ffi
devbox run -- just test-py-ffi
devbox run -- just test-ocaml
devbox run -- just generate    # regenerate include/coda.hpp (quom)
devbox run -- just build       # host shared library
devbox run -- just cross-all   # cross-compile every release target
```

Layering: **devbox → just → scripts/tasks.py**. `just` recipes are thin
wrappers around `tasks.py`.

> Note: there is no `devbox run --just <name>` flag. Use
> `devbox run -- just <name>` (everything after `--` is passed verbatim).

## The single source of truth for tests

All four language test runners (C++, C FFI, Python, OCaml) consume the SAME
catalog file: `tests/catalog/catalog.coda`. To add a test case, **edit
catalog.coda only** — do not add language-specific fixtures.

The C FFI suite lives in `tests/c/` (separate from `tests/cpp/`) because, while
it is written in C++, it tests the **C ABI surface**. It runs the shared catalog
plus a handful of FFI-only checks (ABI version, `coda_doc_t*` lifecycle,
status codes, file parsing) that have no portable catalog equivalent.

The OCaml runner implements every catalog op that maps onto its binding surface.
The only skipped ops are the `*_throws` family (e.g. `as_array_on_scalar_throws`):
those assert that the C++/Python API *raises* on a type error, but the OCaml
binding uses `option` / status-code returns instead of exceptions, so the
assertion is semantically inapplicable rather than missing.

A catalog entry looks like:

```coda
{
    suite "Scalars & escapes"
    name "unquoted string value"
    src "name myproject\n"
    checks [
        { op get_string  field name  eq myproject }
    ]
}
```

Action-style entries (`action roundtrip`, `action parse_fail_msg`,
`action parse_fail_code`) and the `checks` op vocabulary are implemented in the
per-language harness `run_check` dispatchers. The OCaml runner implements a
subset and reports unsupported ops as SKIPPED (never as false failures).

## Generated header

`include/coda.hpp` is produced by `quom` from `src/coda.hpp`. It is a build
artifact and is **git-ignored**. `just generate` (and the test/build recipes)
recreate it on demand. Never hand-edit it; edit `src/`.

## Style

- C/C++/OCaml: tabs for indentation (matches the existing tree).
- See `.editorconfig`.
- Prefer the AST `Variant::match` / `visitContent` for type dispatch — do NOT
  use try/catch on `as*()` accessors as control flow.
