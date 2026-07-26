# C++ API — `include/coda.hpp`

Coda is a header-only C++17 library. Use the generated single header from a release artifact, or generate it from a source checkout:

```bash
devbox run -- just generate
```

```cpp
#include "path/to/coda.hpp"
```

## Parsing and documents

```cpp
coda::Doc fromFile("project.coda");
coda::Doc fromText = coda::Doc::parse("name myproject\n");
coda::Doc withName = coda::Doc::parse(source, "project.coda");
coda::Doc empty;
```

Invalid input throws `coda::ParseError`. `doc.root()` returns the top-level `coda::Block`.

```cpp
const coda::Block& root = fromText.root();
std::string name = root["name"].asString();
```

`Block::operator[]` is lookup-only and throws `std::out_of_range` for a missing key. Use `has()` or `contains()` when absence is expected.

## Values

Block and array elements are `coda::detail::Value` objects. Narrow them with:

| Method | Result |
|---|---|
| `asString()` | `std::string&` |
| `asBlock()` | `coda::Block&` |
| `asArray()` | `coda::Array&` |
| `asTable()` | `coda::Table&` |
| `asKeyedTable()` | `coda::KeyedTable&` |

Each method throws `std::runtime_error` when the value has another kind. `isContainer()` is true for blocks, arrays, tables, and keyed tables.

## Creating and modifying

```cpp
coda::Doc doc;
coda::Block& root = doc.root();

root.insert("name", "myproject")
    .insert("version", "1.0.0");

coda::Block compiler;
compiler.insert("debug", "false");
root.insert("compiler", std::move(compiler));

coda::Array targets;
targets.append("x86_64-linux")
       .append("aarch64-macos");
root.insert("targets", std::move(targets));
```

`Block::insert` replaces an existing value and returns `*this`. Arrays preserve element order.

## Plain tables

Table columns have an explicit declaration order. That order is preserved while parsing, iterating through `getColumnOrder()`, and serializing, including empty tables.

```cpp
coda::Table releases({"version", "date"});
releases.append(
    coda::Row()
        .insert("version", "1.0.0")
        .insert("date", "2026-01-01")
);
root.insert("releases", std::move(releases));
```

Constructors:

```cpp
coda::Table(std::vector<std::string> orderedColumns);
coda::Table(std::initializer_list<std::string> orderedColumns);
coda::Table(std::set<std::string> columns); // compatibility: sorted set order
```

Duplicate column names throw `std::invalid_argument`. `append` requires each row to contain exactly the declared columns; missing or unknown fields also throw `std::invalid_argument`.

Useful members:

| Member | Description |
|---|---|
| `operator[](index)` | Row lookup; throws when out of range |
| `size()` / `empty()` | Row count and emptiness |
| `getHeaders()` | Unordered membership set |
| `getColumnOrder()` | Declared column order |
| `getHeaderComment()` / `setHeaderComment()` | Comment before the header |
| `begin()` / `end()` | Row iteration |

## Keyed tables

```cpp
coda::KeyedTable deps({"link", "version"});
deps.insert(
    "plot",
    coda::Row()
        .insert("link", "github.com/zane-lang/plot")
        .insert("version", "4.0.3")
);
root.insert("deps", std::move(deps));
```

Keyed tables have the same ordered-column and row-validation rules as plain tables. Rows are addressed by key:

```cpp
std::string link = root["deps"].asKeyedTable()["plot"]["link"];
```

Useful members:

| Member | Description |
|---|---|
| `insert(key, row)` | Insert or replace a validated row |
| `operator[](key)` | Lookup; throws when absent |
| `size()` / `empty()` | Row count and emptiness |
| `getColumnOrder()` | Declared non-key columns |
| `order()` | Sort row keys alphabetically |
| `order(weightFn)` | Sort row keys by descending weight; ties alphabetically |
| `begin()` / `end()` | Iterate `(key, Row)` pairs |

## Rows

A `coda::Row` is an insertion-ordered map of column names to string values.

```cpp
coda::Row row;
row.insert("name", "example");
std::string name = row["name"];
```

`operator[]` throws for an absent field. Row comments are available through `getComment()` and `setComment()`.

## Comments

Each `Value` has a pre-node comment:

```cpp
root["deps"].setComment("dependency table");
std::string comment = root["deps"].getComment();
```

Arrays and both table types also have a header comment:

```cpp
root["deps"].asKeyedTable().setHeaderComment("optional dependencies");
```

Comments are stored without the leading `#`; serialization adds it.

## Ordering

```cpp
doc.order();

doc.order([](const std::string& key) -> float {
    if (key == "name") return 100.0f;
    if (key == "version") return 90.0f;
    return 0.0f;
});
```

Default ordering places scalar block fields first, then container fields; each group is alphabetical. Weighted ordering uses descending weight with an alphabetical tie-break.

Ordering recurses into nested blocks and arrays. Keyed-table rows are sorted by row key or row-key weight. Array element order and plain-table row order are preserved.

Individual blocks and keyed tables can be ordered directly:

```cpp
root["compiler"].asBlock().order();
root["deps"].asKeyedTable().order();
```

## Serialization and files

```cpp
std::string text = doc.serialize();

doc.useTabs();
doc.useSpaces(2);
doc.save("out.coda");
doc.save("out.coda", "  ");
```

A root-style block serializes without braces:

```cpp
std::string text = doc.root().serialize();
```

A nested block can be serialized with braces:

```cpp
std::string text = root["compiler"].asBlock().serialize(0, "\t");
```

## Error handling

```cpp
try {
    coda::Doc doc("project.coda");
} catch (const coda::ParseError& error) {
    std::cerr << error.what() << '\n';
    std::cerr << error.message << '\n';
    std::cerr << error.loc.line << ':' << error.loc.col << '\n';
}
```

`ParseError` exposes `code`, `loc`, `message`, `filename`, `sourceLine`, and a fully formatted `what()` string with the source line and caret.
