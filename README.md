# <img height="80" align="right" src="https://raw.githubusercontent.com/zane-lang/logos/refs/heads/main/coda/coda.svg" alt="Coda logo" /> <br> Coda

Coda is a small configuration format that is easy to read and easy to write by
hand. It is the format of [Zane](https://github.com/zane-lang/zane)'s project
files (`zane.coda`, `zane-lock.coda`), but nothing in it is specific to Zane:
this repository is a standalone C++ library with bindings for Python, C,
OCaml and Crystal.

The name comes from music: a coda is the passage that ties a piece together.

```coda
# A project's settings
name myproject
version 1.0.0

compiler {
	debug false
	optimize true
}

targets [
	x86_64-linux
	aarch64-macos
]

# One row per dependency, keyed by its first column
deps [
	key url version
	plot github.com/zane-lang/plot 4.0.3
	json github.com/zane-lang/json 1.2.0
]
```

<details>
<summary>The same file in JSON</summary>

```json
{
  "name": "myproject",
  "version": "1.0.0",
  "compiler": { "debug": "false", "optimize": "true" },
  "targets": ["x86_64-linux", "aarch64-macos"],
  "deps": {
    "plot": { "url": "github.com/zane-lang/plot", "version": "4.0.3" },
    "json": { "url": "github.com/zane-lang/json", "version": "1.2.0" }
  }
}
```

</details>

## The format in short

- One entry per line, `key value`, with no `=` or `:`. Indentation is for
  the reader.
- Three structures: **blocks** `{ }` hold named entries, **arrays** `[ ]` hold
  a list, and an array whose first line is a header becomes a **table**. A
  header starting with `key` makes a **keyed table**, whose rows are looked up
  by their first column.
- Every value is a string. Whether `4.0.3` is a version or `true` a boolean is
  up to the program reading it.
- Quotes are needed only around values with spaces or the characters `{}[]"#`.
- Comments start with `#`, and are kept with the entry that follows them, so a
  program can rewrite a file without losing them.

The full grammar is in [`docs/SPEC.md`](https://github.com/zane-lang/coda/blob/main/docs/SPEC.md).

## Using it

**Python**: the `coda-format` package on PyPI includes the compiled library.

```sh
pip install coda-format
```

```python
import coda

with coda.Doc.parse_file("config.coda") as doc:
    root = doc.root()
    print(root["name"])                    # myproject
    print(root["deps"]["plot"]["version"]) # 4.0.3
```

**C++**: Coda is a header-only library. Generate its single header,
`include/coda.hpp`, from a checkout with `devbox run -- just generate`, copy
it into your project or add `include/` to your include path, and
`#include "coda.hpp"`. [`examples/cpp.cpp`](https://github.com/zane-lang/coda/blob/main/examples/cpp.cpp) is a tour of the
API.

**C and other languages**: each [release](https://github.com/zane-lang/coda/releases)
carries the library with a C interface (`ffi/coda_ffi.h`), shared and static,
for Linux, Windows and macOS on x86-64 and AArch64.
`devbox run -- just build` builds the shared library for your machine, and
`devbox run -- just cross-all` every library a release carries.

**OCaml** and **Crystal**: bindings over the same C library live in
[`bindings/ocaml/`](https://github.com/zane-lang/coda/tree/main/bindings/ocaml/) and [`bindings/crystal/`](https://github.com/zane-lang/coda/tree/main/bindings/crystal/).

Each binding has its own API reference:

| API | Reference |
|---|---|
| C++ (`include/coda.hpp`) | [`docs/API-CPP.md`](https://github.com/zane-lang/coda/blob/main/docs/API-CPP.md) |
| Python | [`docs/API-PYTHON.md`](https://github.com/zane-lang/coda/blob/main/docs/API-PYTHON.md) |
| C (`ffi/coda_ffi.h`) | [`docs/API-C-FFI.md`](https://github.com/zane-lang/coda/blob/main/docs/API-C-FFI.md) |
| OCaml | [`docs/API-OCAML.md`](https://github.com/zane-lang/coda/blob/main/docs/API-OCAML.md) |
| Crystal | [`docs/API-CRYSTAL.md`](https://github.com/zane-lang/coda/blob/main/docs/API-CRYSTAL.md) |

Editors: [`tree-sitter-coda`](https://github.com/zane-lang/tree-sitter-coda) is
a Tree-sitter grammar for highlighting Coda files.

## Building and testing

The toolchain (Zig, `just`, Python, OCaml, Crystal) comes from
[devbox](https://www.jetify.com/devbox):

```sh
devbox run -- just test         # every suite: C++, C, Python, OCaml, Crystal
devbox run -- just build        # the shared library for this machine
devbox run -- just cross-all    # the shared library for every supported target
```

`just` with no arguments lists every recipe. All five test suites read their
cases from one file, `tests/catalog/catalog.coda`.
[`contributing/README.md`](https://github.com/zane-lang/coda/blob/main/contributing/README.md) describes the layout and
standards; [`docs/RELEASING.md`](https://github.com/zane-lang/coda/blob/main/docs/RELEASING.md) how a release is made.
