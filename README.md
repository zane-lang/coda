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

Coda is a **header-only C++ library**. To use it in your project, copy or symlink `include/coda.hpp` and `#include` it directly — no build step required.

```cpp
#include "path/to/coda.hpp"
```

For Python, build the shared library with `just` and use `bindings/python/coda.py` (see [Building & testing](#building--testing-just) below).

---

## Quick Start

### C++ (`include/coda.hpp`)

```cpp
#include "include/coda.hpp"

int main() {
	Coda coda("project.coda");
	auto& root = coda.root();

	// Access scalar values
	std::string name  = root["name"].asString();
	std::string debug = root["compiler"]["debug"].asString();

	// Access a keyed table
	std::string plotLink = root["deps"]["plot"]["link"].asString();

	// Iterate a bare list
	for (const auto& target : root["compiler"]["targets"].asArray()) {
		std::cout << target.asString() << "\n";
	}

	// Iterate a block
	for (const auto& [key, value] : root["compiler"].asBlock()) {
		std::cout << key << ": " << value.asString() << "\n";
	}

	// Modify and save
	root["name"].asString() = "newproject";
	coda.save("project.coda");

	return 0;
}
```

#### Sorting

Fields can be sorted for consistent output:

```cpp
// Alphabetical, scalars before containers
coda.order();

// Custom weight function (higher weight = earlier)
coda.order([](const std::string& key) -> float {
	if (key == "name") return 100;
	if (key == "type") return 90;
	return 0;
});
```

---

### Python bindings (`bindings/python/coda.py`)

The Python wrapper uses `ctypes` to load `libcoda_ffi` and provides `CodaDoc` and related node helpers.

```python
from bindings.python.coda import CodaDoc

text = """\
deps [
	# optional deps
	key link version
	plot github.com/zane-lang/plot 4.0.3
]
"""

with CodaDoc.parse(text) as doc:
	file = doc.file()
	deps = file["deps"].as_keyed_table()
	print(deps.header_comment)              # "optional deps"
	print(deps["plot"]["link"])             # "github.com/zane-lang/plot"
```

---

### C FFI (`ffi/coda_ffi.h`)

Coda also ships a C ABI for embedding in other languages. See `ffi/coda_ffi.h` for the full surface.

Highlights:

* parse from bytes / file
* serialize back to Coda text
* walk arrays and map-like nodes (file/block/table)
* get/set `comment` and `headerComment`
* reorder keys (`order`, `order_weighted`)
* ABI version check (`coda_ffi_abi_version`)

(When using `coda_str_t` views, treat returned pointers as borrowed: they remain valid only as long as the underlying document and node storage remain unchanged.)

---

## Building & testing (Just)

This repo is built and tested via `just`.

Common commands:

```bash
just generate     # regenerate include/coda.hpp (requires quom)
just cross-all    # cross compile for all available platforms
just test         # run all tests
just test-cpp
just test-c-ffi
just test-py-ffi
```

Cross-compiled FFI artifacts (if enabled by your recipes) are placed under `dist/<target>/`.
