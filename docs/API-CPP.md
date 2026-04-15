# C++ API — `include/coda.hpp`

Coda is a **header-only** C++17 library. Copy `include/coda.hpp` into your project and `#include` it — no build step required.

---

## Setup

```cpp
#include "path/to/coda.hpp"
```

Requires C++17 or later (`std::variant`, structured bindings, `if constexpr`). No dependencies beyond the standard library.

---

## Parsing

```cpp
// From a file path
coda::Doc doc("project.coda");

// From a string
coda::Doc doc = coda::Doc::parse("name myproject\n");
coda::Doc doc = coda::Doc::parse(source, "optional-filename.coda");

// Empty document (editable root Block, no file needed)
coda::Doc doc;
```

All forms throw `coda::ParseError` on invalid input.

---

## Reading values

`doc.root()` returns a `coda::Block&` — the top-level map of the document. Chain `operator[]` to reach nested nodes, then cast to a concrete type with one of the `as*()` helpers.

```cpp
const coda::Block& root = doc.root();

// Scalar string
std::string name    = root["name"].asString();
std::string debug   = root["compiler"]["debug"].asString();

// Membership test
if (root.has("version")) { ... }

// Block (nested key-value map)
const coda::Block& compiler = root["compiler"].asBlock();
for (const auto& [key, valPtr] : compiler)
    std::cout << key << " = " << valPtr->asString() << "\n";

// Array (bare list)
const coda::Array& targets = root["targets"].asArray();
for (size_t i = 0; i < targets.size(); ++i)
    std::cout << targets[i].asString() << "\n";

// Plain table (rows by index)
const coda::Table& releases = root["releases"].asTable();
for (const auto& row : releases)
    std::cout << row["version"] << "  " << row["date"] << "\n";

// Keyed table (rows by key string)
const coda::KeyedTable& deps = root["deps"].asKeyedTable();
std::string link = deps["plot"]["link"];
for (const auto& [key, row] : deps)
    std::cout << key << " → " << row["link"] << "\n";
```

`as*()` methods are on `coda::detail::Value` — what `Block::operator[]` and `Array::operator[]` return.

| Method | Returns | Throws if wrong type |
|---|---|---|
| `asString()` | `std::string&` | `std::runtime_error` |
| `asBlock()` | `coda::Block&` | `std::runtime_error` |
| `asArray()` | `coda::Array&` | `std::runtime_error` |
| `asTable()` | `coda::Table&` | `std::runtime_error` |
| `asKeyedTable()` | `coda::KeyedTable&` | `std::runtime_error` |

---

## Creating and modifying

```cpp
coda::Doc doc;
coda::Block& root = doc.root();

// Scalars
root["name"]    = "myproject";
root["version"] = "1.0.0";

// Block
coda::Block compiler;
compiler["debug"]    = "false";
compiler["optimize"] = "true";
root["compiler"] = std::move(compiler);

// Array
coda::Array targets;
targets.setHeaderComment("supported build targets");
targets.append("x86_64-linux")
       .append("x86_64-windows")
       .append("aarch64-macos");
root["targets"] = std::move(targets);

// Plain table (column set defined at construction, validated on append)
coda::Table releases({"version", "date"});
releases.append(coda::Row().insert("version", "1.0.0").insert("date", "2025-01-01"));
releases.append(coda::Row().insert("version", "1.1.0").insert("date", "2025-06-15"));
root["releases"] = std::move(releases);

// Keyed table
coda::KeyedTable deps({"link", "version"});
deps.setHeaderComment("optional");
deps.insert("plot", coda::Row()
    .insert("link",    "github.com/zane-lang/plot")
    .insert("version", "4.0.3"));
root["deps"] = std::move(deps);
root["deps"].setComment("dependency table");

// Modify an existing scalar in-place
root["version"].asString() = "2.0.0";

// Append to an existing array
root["targets"].asArray().append("wasm32-wasi");

doc.save("out.coda");
```

Non-const `Block::operator[]` auto-inserts an empty string node when the key is absent, so you can also write:

```cpp
root["version"].asString() = "1.0.0";  // inserts then assigns in one step
```

---

## Sorting

```cpp
// Alphabetical for the whole document (scalars first, then containers)
doc.order();

// Custom weight function — higher weight → closer to the top
doc.order([](const std::string& key) -> float {
    if (key == "name")    return 100;
    if (key == "version") return  90;
    return 0;   // everything else alphabetical at the bottom
});
```

Sorting can also be applied to an individual sub-tree:

```cpp
root["compiler"].asBlock().order();

root["compiler"].asBlock().order([](const std::string& key) -> float {
    return key == "debug" ? 10 : 0;
});
```

---

## Serialisation

```cpp
// Get the document text (uses the current indent unit, default: "\t")
std::string text = doc.serialize();

// Choose the indent unit
doc.useTabs();       // \t  (default)
doc.useSpaces(2);    // two spaces

// Write to disk
doc.save("out.coda");

// Override the indent unit and write in one call
doc.save("out.coda", "  ");
```

Individual nodes can be serialised independently:

```cpp
// Top-level (root-style, no braces)
std::string s = root["compiler"].asBlock().serialize();

// Nested (wrapped in { })
std::string s = root["compiler"].asBlock().serialize(0, "\t");
```

---

## Error handling

All parse errors throw `coda::ParseError` (inherits `std::exception`):

```cpp
try {
    coda::Doc doc("project.coda");
} catch (const coda::ParseError& e) {
    std::cerr << e.what() << "\n";    // fully formatted, includes source line + caret
    std::cerr << e.message  << "\n";  // bare message
    std::cerr << e.filename << "\n";  // source file name
    std::cerr << e.loc.line << ":" << e.loc.col << "\n";   // 1-based location
}
```

`e.what()` returns a multi-line string that includes the offending source line and a `^` caret. Runtime type errors (e.g. calling `asBlock()` on a string node) throw `std::runtime_error`.

---

## Comments

Nodes carry the `#` comments read from the source. `coda::detail::Value` (returned by `Block::operator[]`) exposes `getComment()`/`setComment()`. `Array`, `Table`, and `KeyedTable` additionally carry a *header comment* for the `#` lines directly before the first element or header row.

```cpp
// Read comments after parsing
std::string c = root["deps"].getComment();          // "dependency table"
std::string h = root["deps"].asKeyedTable().getHeaderComment();  // "optional"

// Set comments when building
root["deps"].setComment("dependency table");
root["deps"].asKeyedTable().setHeaderComment("optional");

// Row-level comments (inside a table)
for (auto& [key, row] : root["deps"].asKeyedTable())
    std::cout << row.getComment() << "\n";
```

Comments are stored and serialised without the leading `#` — the serialiser adds it automatically.

---

## Class reference

### `coda::Doc`

| Member | Return type | Description |
|---|---|---|
| `Doc()` | — | Create an empty document with an empty root `Block` |
| `Doc(path)` | — | Load and parse a `.coda` file; throws `ParseError` on failure |
| `Doc::parse(text, filename?)` | `Doc` | Parse from a `std::string`; `filename` is used in error messages |
| `root()` | `Block&` | Return the root block |
| `order()` | `void` | Sort all keys alphabetically (scalars first, then containers) |
| `order(fn)` | `void` | Sort by a `float(const std::string&)` weight function |
| `useTabs()` | `void` | Set indent unit to `\t` (default) |
| `useSpaces(n)` | `void` | Set indent unit to `n` spaces |
| `save(path)` | `void` | Serialise and write to disk using the current indent unit |
| `save(path, unit)` | `void` | Set the indent unit, then write to disk |
| `serialize()` | `std::string` | Return Coda text using the current indent unit |

### `coda::detail::Value`

Every node stored in a `Block` or `Array` is a `coda::detail::Value`. You don't construct these directly — they are returned by `operator[]`.

| Member | Return type | Description |
|---|---|---|
| `asString()` | `std::string&` | Throws `std::runtime_error` if not a string |
| `asBlock()` | `coda::Block&` | Throws `std::runtime_error` if not a block |
| `asArray()` | `coda::Array&` | Throws `std::runtime_error` if not an array |
| `asTable()` | `coda::Table&` | Throws `std::runtime_error` if not a table |
| `asKeyedTable()` | `coda::KeyedTable&` | Throws `std::runtime_error` if not a keyed table |
| `isContainer()` | `bool` | `true` for `Block`, `Array`, `Table`, `KeyedTable`; `false` for strings |
| `getComment()` | `const std::string&` | Pre-node `#` comment (without the `#`) |
| `setComment(s)` | `void` | Set pre-node comment |

Values are constructed implicitly when assigning to `Block::operator[]`:

```cpp
root["x"]        = "hello";            // std::string / const char*
root["compiler"] = coda::Block{};      // Block
root["targets"]  = coda::Array{};      // Array
root["releases"] = coda::Table({"version", "date"});       // Table
root["deps"]     = coda::KeyedTable({"link", "version"});  // KeyedTable
```

### `coda::Block`

Ordered map of `string → Value`. The root of every document is a `Block`.

| Member | Return type | Description |
|---|---|---|
| `Block()` | — | Construct an empty block |
| `operator[](key)` | `detail::Value&` | Non-const: auto-inserts empty string if absent; const: throws `std::out_of_range` |
| `insert(key, value)` | `Block&` | Insert or replace a value; returns `*this` for chaining |
| `has(key)` | `bool` | `true` if the key exists |
| `order()` | `void` | Sort keys alphabetically (scalars first, then containers) |
| `order(fn)` | `void` | Sort by a `float(const std::string&)` weight function |
| `serialize(unit?)` | `std::string` | Serialise as a top-level block (no surrounding braces); `unit` defaults to `"\t"` |
| `serialize(indent, unit)` | `std::string` | Serialise as a nested block wrapped in `{ }`; `indent` is the current nesting depth |
| `begin()` / `end()` | iterator | Iterate `pair<string, unique_ptr<Value>>` in insertion order |

### `coda::Array`

Ordered list of `Value`.

| Member | Return type | Description |
|---|---|---|
| `Array()` | — | Construct an empty array |
| `append(value)` | `Array&` | Append an element; returns `*this` for chaining |
| `operator[](i)` | `detail::Value&` | Get element by index; throws `std::out_of_range` if out of range |
| `size()` | `size_t` | Number of elements |
| `getHeaderComment()` | `const std::string&` | Comment before the first element |
| `setHeaderComment(s)` | `void` | Set header comment |
| `begin()` / `end()` | iterator | Iterate `unique_ptr<Value>` in insertion order — dereference with `*` or `->` to reach the `Value` |

### `coda::Table`

Plain (anonymous-row) table. Column names are validated on `append`.

| Member | Return type | Description |
|---|---|---|
| `Table(headers)` | — | Construct with a non-empty `std::set<std::string>` of column names |
| `append(row)` | `Table&` | Append a `Row`; validates fields against the header set; returns `*this` for chaining |
| `operator[](i)` | `Row&` | Get row by index; throws `std::out_of_range` if out of range |
| `size()` | `size_t` | Row count |
| `empty()` | `bool` | `true` if there are no rows |
| `front()` | `const Row&` | First row |
| `getHeaderComment()` | `const std::string&` | Comment before the header row |
| `setHeaderComment(s)` | `void` | Set header comment |
| `begin()` / `end()` | iterator | Iterate `Row` |

### `coda::KeyedTable`

Keyed table — rows indexed by a key string. Column names are validated on `insert`.

| Member | Return type | Description |
|---|---|---|
| `KeyedTable(headers)` | — | Construct with a non-empty `std::set<std::string>` of (non-key) column names |
| `insert(key, row)` | `KeyedTable&` | Insert or replace a row; validates fields; returns `*this` for chaining |
| `operator[](key)` | `Row&` | Non-const: auto-inserts empty row if absent; const: throws `std::out_of_range` |
| `empty()` | `bool` | `true` if there are no rows |
| `getHeaderComment()` | `const std::string&` | Comment before the header row |
| `setHeaderComment(s)` | `void` | Set header comment |
| `begin()` / `end()` | iterator | Iterate `pair<string, Row>` in insertion order |

### `coda::Row`

A single table row — flat map of `string → string`.

| Member | Return type | Description |
|---|---|---|
| `Row()` | — | Construct an empty row |
| `insert(col, value)` | `Row&` | Set column value; returns `*this` for chaining |
| `operator[](col)` | `std::string&` | Non-const: auto-inserts empty string if absent; const: throws `std::out_of_range` |
| `getComment()` | `const std::string&` | Row-level `#` comment (without the `#`) |
| `setComment(s)` | `void` | Set row-level comment |
| `begin()` / `end()` | iterator | Iterate `pair<string, string>` in insertion order |

### `coda::ParseError`

Thrown by all parse functions. Inherits `std::exception`.

| Member | Type | Description |
|---|---|---|
| `what()` | `const char*` | Fully formatted message including source line and `^` caret |
| `message` | `std::string` | Bare error description |
| `filename` | `std::string` | Source file name (empty if not provided) |
| `loc.line` | `int` | 1-based line number |
| `loc.col` | `int` | 1-based column number |
| `loc.offset` | `size_t` | Byte offset into the source |
| `sourceLine` | `std::string` | The source line that caused the error |
| `code` | `ParseErrorCode` | Enum value identifying the error kind |

`ParseErrorCode` values: `UnexpectedToken`, `UnexpectedEOF`, `DuplicateKey`, `DuplicateField`, `RaggedRow`, `InvalidEscape`, `UnterminatedString`, `NestedBlock`, `ContentAfterBrace`, `KeyInBlock`.
