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

For Python, build the shared library with `rake` and use `bindings/python/coda.py` (see [Building & testing](#building--testing-rake) below).

---

## Quick Start

### C++ (`include/coda.hpp`)

Copy `include/coda.hpp` into your project and include it. No build step required for the parser itself.

#### Parsing and reading

```cpp
#include "include/coda.hpp"

int main() {
	// From file
	Coda coda("project.coda");
	auto& root = coda.root();           // coda::Block&

	// From string
	Coda coda2 = Coda::parse("name myproject\n");
	auto& root2 = coda2.root();

	// Scalars
	std::string name  = root["name"].asString();
	std::string debug = root["compiler"]["debug"].asString();

	// Block (nested map)
	coda::Block& compiler = root["compiler"].asBlock();
	for (const auto& [key, val] : compiler) {
		std::cout << key << ": " << val->asString() << "\n";
	}

	// Bare-list array
	coda::Array& targets = root["compiler"]["targets"].asArray();
	for (const auto& item : targets) {
		std::cout << item->asString() << "\n";
	}

	// Keyed table
	coda::KeyedTable& deps = root["deps"].asKeyedTable();
	std::string plotLink = deps["plot"]["link"];

	return 0;
}
```

#### Mutating and saving

```cpp
// Modify a scalar in-place
root["name"].asString() = "newproject";

// Set indent to 2 spaces and write back
coda.useSpaces(2);
coda.save("project.coda");

// Or get the serialised text
std::string text = coda.serialize();
```

#### Sorting

```cpp
// Alphabetical, scalars before containers (stable within each group)
coda.order();

// Custom weight function (higher weight → closer to top)
coda.order([](const std::string& key) -> float {
	if (key == "name") return 100;
	if (key == "type") return  90;
	return 0;
});
```

**`Coda` class summary**

| Member | Description |
|---|---|
| `Coda(path)` | Load and parse a `.coda` file |
| `Coda::parse(text, filename?)` | Parse from a string |
| `root()` | Return the root `coda::Block&` |
| `order()` | Sort all keys alphabetically (scalars first) |
| `order(fn)` | Sort by a `float(key)` weight function |
| `useTabs()` / `useSpaces(n)` | Set the serialisation indent unit |
| `save(path)` / `save(path, unit)` | Write back to disk |
| `serialize()` | Return the Coda text as `std::string` |

---

### Python bindings (`bindings/python/coda.py`)

The Python wrapper uses `ctypes` to load `libcoda_ffi` and exposes a class hierarchy that mirrors the Coda node types. Build the shared library first with `rake build` (see [Building & testing](#building--testing-rake)).

#### Parsing and reading

```python
from bindings.python.coda import CodaDoc

text = """\
name myproject

compiler {
	debug false
	targets [
		x86_64-linux
		aarch64-linux
	]
}

deps [
	# optional deps
	key link version
	plot github.com/zane-lang/plot 4.0.3
]
"""

with CodaDoc.parse(text) as doc:
	root = doc.root()                       # CodaBlock

	# Scalars
	name  = root["name"].as_string().value  # "myproject"

	# Blocks
	compiler = root["compiler"].as_block()
	debug    = compiler["debug"].as_string().value  # "false"

	# Bare-list array
	targets = root["compiler"]["targets"].as_array()
	for item in targets:
		print(item.as_string().value)

	# Keyed table
	deps = root["deps"].as_keyed_table()
	print(deps.header_comment)              # "optional deps"
	plot_row = deps["plot"]                 # CodaRow
	print(plot_row["link"])                 # "github.com/zane-lang/plot"
```

#### Creating and modifying

```python
doc = CodaDoc.new()
root = doc.root()

name_node = CodaString(doc, "myproject")
root["name"] = name_node

doc.save("out.coda")
```

#### Sorting

```python
doc.order()                                         # alphabetical, scalars first
doc.order_weighted([("name", 100), ("type", 90)])   # by weight
```

**Key classes**

| Class | Description |
|---|---|
| `CodaDoc` | Document lifecycle; `parse()`, `parse_file()`, `new()`, `root()`, `serialize()`, `save()` |
| `CodaBlock` | Ordered map of `str → CodaNode`; `__getitem__`, `__setitem__`, `__iter__`, `__len__` |
| `CodaString` | Leaf string value; `.value` property |
| `CodaArray` | Ordered list of `CodaNode`; `__getitem__`, `__iter__`, `append()` |
| `CodaTable` | Plain table; `.columns()`, row access by index |
| `CodaKeyedTable` | Keyed table; `.columns()`, row access by key string |
| `CodaRow` | One row inside a table; `__getitem__(col)` returns `str` |

All node classes expose a `.comment` property. `CodaArray`, `CodaTable`, and `CodaKeyedTable` also expose `.header_comment`.

---

### C FFI (`ffi/coda_ffi.h`)

The C ABI (`libcoda_ffi`) makes the parser embeddable in any language with a C FFI. Include `ffi/coda_ffi.h` and link against the shared library built by `rake build`.

#### Lifecycle

```c
#include "ffi/coda_ffi.h"

coda_error_t err = {0};
const char src[] = "name myproject\n";
coda_doc_t* doc = coda_doc_parse(src, sizeof(src) - 1, "example.coda", &err);
if (!doc) {
	fprintf(stderr, "parse error: %.*s\n", (int)err.message.len, err.message.ptr);
	coda_error_clear(&err);
	return 1;
}

// … use doc …

coda_doc_free(doc);
```

#### Reading values

```c
coda_node_t root = coda_doc_root(doc);           // CODA_NODE_BLOCK

// Get a string value
coda_node_t name_node = coda_map_get(doc, root, "name", 4);
coda_str_t  name      = coda_string_get(doc, name_node);
printf("%.*s\n", (int)name.len, name.ptr);       // "myproject"

// Traverse a keyed table
coda_node_t deps = coda_map_get(doc, root, "deps", 4);
// deps is CODA_NODE_KEYED_TABLE
size_t nrows = coda_keyed_table_row_count(doc, deps);
for (size_t r = 0; r < nrows; ++r) {
	coda_str_t  key = coda_keyed_table_row_key_at(doc, deps, r);
	coda_node_t row = coda_keyed_table_row_at(doc, deps, r);
	coda_str_t  lnk = coda_row_get(doc, row, "link", 4);
	printf("%.*s → %.*s\n", (int)key.len, key.ptr, (int)lnk.len, lnk.ptr);
}
```

> **`coda_str_t` lifetime**: these are *borrowed* views into the document's internal storage. Copy the contents before any mutation or `coda_doc_free()`.

#### Serialising

```c
coda_owned_str_t out = coda_doc_serialize(doc, "\t", 1, NULL);
printf("%.*s", (int)out.len, out.ptr);
coda_owned_str_free(out);
```

**Core functions**

| Function | Description |
|---|---|
| `coda_doc_parse(src, len, file?, err?)` | Parse UTF-8 bytes |
| `coda_doc_parse_file(path, err?)` | Parse from file path |
| `coda_doc_new()` | Create an empty document |
| `coda_doc_root(doc)` | Get root `CODA_NODE_BLOCK` handle |
| `coda_doc_serialize(doc, indent, len, err?)` | Serialise to owned string |
| `coda_doc_order(doc)` | Sort all keys (alphabetical, scalars first) |
| `coda_doc_order_weighted(doc, keys, weights, n)` | Sort by weight array |
| `coda_doc_free(doc)` | Free the document |
| `coda_map_get(doc, block, key, klen)` | Lookup key in BLOCK |
| `coda_string_get(doc, node)` | Read string value |
| `coda_keyed_table_row_get(doc, kt, key, klen)` | Lookup row in KEYED_TABLE |
| `coda_row_get(doc, row, col, clen)` | Read column value from ROW |
| `coda_ffi_abi_version()` | ABI version integer |

See `ffi/coda_ffi.h` for the complete surface including array, table, and mutation APIs.

---

## Building & testing (Rake)

This repo is built and tested via `rake`.

Common commands:

```bash
rake generate     # regenerate include/coda.hpp (requires quom)
rake cross-all    # cross compile for all available platforms
rake test         # run all tests
rake test-cpp
rake test-c-ffi
rake test-py-ffi
```

Cross-compiled FFI artifacts (if enabled by your recipes) are placed under `dist/<target>/`.
