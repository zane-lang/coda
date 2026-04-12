# C++ API — `include/coda.hpp`

Coda is a **header-only** library. Copy `include/coda.hpp` into your project and `#include` it — no build step required.

```cpp
#include "path/to/coda.hpp"
```

---

## Parsing

```cpp
// From file
Coda coda("project.coda");

// From string
Coda coda = Coda::parse("name myproject\n");
Coda coda = Coda::parse(source, "optional-filename.coda");
```

---

## Reading values

`coda.root()` returns a `coda::Block&` — the top-level map of the document. From there you can chain `operator[]` calls to reach any node, then cast to the concrete type with one of the `as*()` helpers.

```cpp
auto& root = coda.root();   // coda::Block&

// Scalar string
std::string name  = root["name"].asString();
std::string debug = root["compiler"]["debug"].asString();

// Block (nested key-value map)
coda::Block& compiler = root["compiler"].asBlock();
for (const auto& [key, val] : compiler) {
    std::cout << key << ": " << val->asString() << "\n";
}

// Bare-list array
coda::Array& targets = root["compiler"]["targets"].asArray();
for (const auto& item : targets) {
    std::cout << item->asString() << "\n";
}

// Plain table (anonymous rows, accessed by index)
coda::Table& releases = root["releases"].asTable();
for (size_t i = 0; i < releases.size(); ++i) {
    std::cout << releases[i]["version"] << "\n";
}

// Keyed table (rows accessed by their key string)
coda::KeyedTable& deps = root["deps"].asKeyedTable();
std::string plotLink = deps["plot"]["link"];
```

**`as*()` methods on `coda::detail::Value`**

| Method | Returns |
|---|---|
| `asString()` | `std::string&` |
| `asBlock()` | `coda::Block&` |
| `asArray()` | `coda::Array&` |
| `asTable()` | `coda::Table&` |
| `asKeyedTable()` | `coda::KeyedTable&` |

All methods throw `std::runtime_error` if the node is the wrong type.

---

## Mutating and saving

```cpp
// Modify a scalar in-place
root["name"].asString() = "newproject";

// Use 2-space indentation
coda.useSpaces(2);     // or coda.useTabs()

// Write to disk
coda.save("project.coda");
coda.save("project.coda", "  ");   // override indent in one call

// Get the serialised text without writing
std::string text = coda.serialize();
```

---

## Sorting

```cpp
// Alphabetical, scalars before containers (stable within each group)
coda.order();

// Custom weight function — higher weight → closer to the top
coda.order([](const std::string& key) -> float {
    if (key == "name") return 100;
    if (key == "type") return  90;
    return 0;   // everything else alphabetical at the bottom
});
```

Sorting can also be applied to an individual sub-tree:

```cpp
root["compiler"].asBlock().order();
```

---

## `Coda` class reference

| Member | Description |
|---|---|
| `Coda(path)` | Load and parse a `.coda` file |
| `Coda::parse(text, filename?)` | Parse from a `std::string` |
| `root()` | Return the root `coda::Block&` |
| `order()` | Sort all keys alphabetically (scalars first) |
| `order(fn)` | Sort by a `float(key)` weight function |
| `useTabs()` | Use `\t` as the indent unit (default) |
| `useSpaces(n)` | Use `n` spaces as the indent unit |
| `save(path)` | Serialise and write to disk |
| `save(path, unit)` | Override indent unit, then write |
| `serialize()` | Return the Coda text as `std::string` |

---

## Node type reference

| C++ type | Coda construct | Key operations |
|---|---|---|
| `coda::Block` | `{ key value … }` / root | `operator[]`, iteration (`begin`/`end`), `order()` |
| `coda::Array` | `[ … ]` bare list | `operator[]`, iteration, `append()`, `headerComment` |
| `coda::Table` | plain table | `operator[](size_t)`, iteration, `columns()` |
| `coda::KeyedTable` | keyed table | `operator[](string)`, iteration, `columns()` |
| `std::string` | scalar leaf | direct string operations |
