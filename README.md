# <img height="80" align="right" src="https://raw.githubusercontent.com/zane-lang/logos/refs/heads/main/coda/coda.svg" /> <br> Coda

<img width="700" alt="imprint_coda_20260403010920" src="https://github.com/user-attachments/assets/8384c670-bf40-45b4-bf1f-5fcaa2f35324" />

A compact configuration format designed to be easily read- and writeable. The name comes from music — a coda is the concluding passage that ties a composition together. A `.coda` file is the single source of truth for configuration.

The above file in JSON:

<img width="700" alt="imprint_coda_20260403010924" src="https://github.com/user-attachments/assets/fde8d7b5-3dd0-42ac-a640-980de4baf017" />

---

## What is Coda?

* Whitespace-sensitive, line-oriented.
* Every leaf value is a string; interpretation is left to the consumer.
* Quotes are optional unless a value contains whitespace or syntax characters (`{}[]"#`).
* Comments are preserved and attach to the node that follows them.

Coda has three structural constructs: **blocks** `{}`, **arrays** `[]`, and **tables** (inferred from array headers). For the full language specification, see [`docs/SPEC.md`](docs/SPEC.md).

---

## Installation

**Python** — install the pre-built package from PyPI (includes the compiled native library):

```bash
pip install coda-format
```

```python
from coda_format import CodaDoc

with CodaDoc.parse_file("config.coda") as doc:
    print(doc.root()["key"])
```

**C++** — Coda is a **header-only library**. Copy or symlink `include/coda.hpp` and `#include` it directly — no build step required.

```cpp
#include "path/to/coda.hpp"
```

**C FFI / other languages** — build the shared library with `rake build` (see [Building & testing](#building--testing) below).

---

## API documentation

| API | File |
|---|---|
| C++ header (`include/coda.hpp`) | [`docs/API-CPP.md`](docs/API-CPP.md) |
| Python bindings (`bindings/python/coda.py`) | [`docs/API-PYTHON.md`](docs/API-PYTHON.md) |
| C FFI (`ffi/coda_ffi.h`) | [`docs/API-C-FFI.md`](docs/API-C-FFI.md) |

---

## Building & testing

This repo is built and tested via `rake`.

```bash
rake generate     # regenerate include/coda.hpp (requires quom)
rake build        # build host shared library (libcoda_ffi.so)
rake cross-all    # cross-compile for all supported targets
rake test         # run all tests
rake test-cpp
rake test-c-ffi
rake test-py-ffi
```

Cross-compiled FFI artifacts are placed under `dist/<target>/`.
