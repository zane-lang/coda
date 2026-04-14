# Python API — `bindings/python/coda.py`

The Python wrapper uses `ctypes` to talk to `libcoda_ffi` and exposes a class hierarchy that mirrors the Coda node types.

## Setup

1. Build the shared library:
   ```bash
   rake build        # host platform only
   rake cross-all    # all supported targets
   ```
2. Import the module:
   ```python
   from bindings.python.coda import (
       Doc, Block, Array,
       Table, KeyedTable, Row,
   )
   ```

---

## Parsing

```python
# From a string (use as a context manager to auto-free)
with Doc.parse(text) as doc:
    ...

# From a string with an optional filename hint for error messages
with Doc.parse(text, filename="project.coda") as doc:
    ...

# From a file path
with Doc.parse_file("project.coda") as doc:
    ...

# Create an empty document
doc = Doc.new()
```

---

## Reading values

`doc.root()` returns a `Block` — the top-level map. From there, index with `[]` to reach child nodes and cast them with `as_*()`.

```python
with Doc.parse(text) as doc:
    root = doc.root()                          # Block

    # Scalar string — as_string() returns the node; .value gives the str
    name  = root["name"].as_string().value     # "myproject"
    debug = root["compiler"]["debug"].as_string().value

    # Block (nested key-value map)
    compiler = root["compiler"].as_block()
    for key, node in compiler:
        print(key, node.as_string().value)

    # Bare-list array
    targets = root["compiler"]["targets"].as_array()
    for item in targets:
        print(item.as_string().value)

    # Plain table (rows by index)
    releases = root["releases"].as_table()
    for row in releases:
        print(row["version"])

    # Keyed table (rows by key)
    deps = root["deps"].as_keyed_table()
    print(deps.header_comment)                 # "optional deps"
    plot_row = deps["plot"]                    # Row
    print(plot_row["link"])                    # "github.com/zane-lang/plot"
```

---

## Creating and modifying

Scalar string values are represented as plain Python `str`. Container nodes (`Block`, `Array`, `Table`, `KeyedTable`) are created directly without passing a `doc` argument.

```python
doc  = Doc.new()
root = doc.root()

# Insert a scalar — pass a plain str
root["name"] = "myproject"

# Insert a block
compiler = Block()
compiler["debug"] = "false"
root["compiler"] = compiler

# Modify an existing scalar in-place
root["name"].as_string().value = "renamed"

doc.save("out.coda")
```

---

## Sorting

```python
doc.order()                                         # alphabetical, scalars first
doc.order_weighted([("name", 100), ("type", 90)])   # by weight (higher → top)
```

Sorting can also be applied to an individual sub-tree:

```python
root["compiler"].as_block().order()
```

---

## Serialisation

```python
text = doc.serialize()           # default tab indent
text = doc.serialize(indent="  ")

doc.save("out.coda")
doc.save("out.coda", indent="  ")

# Convenience: sort by weight and serialise in one call
text = doc.order_weighted_and_serialize([("name", 100), ("type", 90)])
```

---

## Error handling

```python
from bindings.python.coda import ParseError

try:
    doc = Doc.parse(bad_text)
except ParseError as e:
    print(e)           # "unexpected token (line 3, col 5)"
    print(e.code)      # integer error code
    print(e.line)
    print(e.col)
    print(e.offset)    # byte offset into the source
```

`ParseError` inherits from `Error` (which inherits from `Exception`). All other coda runtime errors raise `Error` directly.

---

## Class reference

### `Doc`

| Method / attribute | Description |
|---|---|
| `Doc.parse(text, filename?)` | Parse UTF-8 text; returns `Doc` |
| `Doc.parse_file(path)` | Parse from a file path |
| `Doc.new()` | Create an empty document |
| `root()` | Return the root `Block` |
| `serialize(indent?)` | Return Coda text as `str` (default: tab indent) |
| `save(path, indent?)` | Serialise and write to disk |
| `order()` | Sort all keys alphabetically (scalars first) |
| `order_weighted(weights)` | Sort by `[(key, float), …]` weight list (higher → top) |
| `order_weighted_and_serialize(weights, indent?)` | Sort by weight then serialise; returns `str` |
| `free()` | Explicitly free the document (auto-called by context manager) |

`Doc` implements the context-manager protocol (`with Doc.parse(...) as doc:`), which calls `free()` on exit.

### `Node`

Base class for all node types. Never instantiated directly.

| Method / attribute | Description |
|---|---|
| `node.comment` | Pre-node `#` comment (get/set) |
| `node.is_container()` | `True` for `Block`, `Array`, `Table`, `KeyedTable` |
| `node.serialize(indent?)` | Serialise this sub-tree to a `str` |
| `node.as_string()` | Narrow to a string node; raises `TypeError` if wrong type |
| `node.as_block()` | Narrow to `Block`; raises `TypeError` if wrong type |
| `node.as_array()` | Narrow to `Array`; raises `TypeError` if wrong type |
| `node.as_table()` | Narrow to `Table`; raises `TypeError` if wrong type |
| `node.as_keyed_table()` | Narrow to `KeyedTable`; raises `TypeError` if wrong type |

`as_string()` returns the node itself with a `.value` property exposed:

```python
node = root["name"].as_string()
print(node.value)       # read
node.value = "renamed"  # write
print(str(node))        # same as node.value
```

### `Block`

Ordered map of `str → Node`. Scalar values may be supplied as plain `str`; they are wrapped automatically.

| Operation | Description |
|---|---|
| `node["key"]` | Look up child node |
| `node["key"] = value` | Insert or replace a child (`str` or container node) |
| `node.insert("key", value)` | Insert or replace a child; returns `node` for chaining |
| `del node["key"]` | Remove a child |
| `"key" in node` | Membership test |
| `for key, child in node` | Iterate in insertion order |
| `len(node)` | Number of entries |
| `node.get_or_insert("key")` | Look up key, inserting an empty string node if absent |
| `node.order()` | Sort this block's keys alphabetically (scalars first) |
| `node.order_weighted(weights)` | Sort by `[(key, float), …]` weight list |
| `node.comment` | Pre-node comment string (get/set) |

### `Array`

Ordered list of `Node`. Scalar values may be supplied as plain `str`.

| Operation | Description |
|---|---|
| `node[i]` | Get item by index |
| `node[i] = value` | Replace item (`str` or container node) |
| `del node[i]` | Remove item |
| `node.append(value)` | Append a node; returns `node` for chaining |
| `for item in node` | Iterate |
| `len(node)` | Length |
| `node.header_comment` | Comment before the first element (get/set) |

### `Table`

Plain (anonymous-row) table.

| Operation | Description |
|---|---|
| `Table(columns?)` | Create a new table, optionally with an initial column list |
| `node[i]` | Get row by index (returns `Row`) |
| `node[i] = row` | Replace row |
| `del node[i]` | Remove row |
| `for row in node` | Iterate rows |
| `len(node)` | Row count |
| `node.columns()` | List of column name strings |
| `node.append_col(name)` | Add a column |
| `node.append(row)` | Append a `Row`; returns `node` for chaining |
| `node.header_comment` | Comment before the header row (get/set) |

### `KeyedTable`

Keyed table — rows indexed by their key string.

| Operation | Description |
|---|---|
| `KeyedTable(columns?)` | Create a new keyed table, optionally with an initial column list |
| `node["key"]` | Get row by key (returns `Row`) |
| `node["key"] = row` | Insert or replace row |
| `node.insert("key", row)` | Insert or replace row; returns `node` for chaining |
| `del node["key"]` | Remove row |
| `"key" in node` | Membership test |
| `for key, row in node` | Iterate in insertion order |
| `len(node)` | Row count |
| `node.columns()` | List of (non-key) column name strings |
| `node.append_col(name)` | Add a column |
| `node.header_comment` | Comment before the header row (get/set) |
| `node.order()` | Sort rows alphabetically by key |
| `node.order_weighted(weights)` | Sort rows by weight |

### `Row`

A single table row — flat map of column name → string value.

| Operation | Description |
|---|---|
| `row["col"]` | Get column value as `str` |
| `row["col"] = "val"` | Set column value |
| `del row["col"]` | Remove column |
| `"col" in row` | Membership test |
| `for col, val in row` | Iterate columns |
| `len(row)` | Column count |
| `row.get("col", default)` | Get value with fallback |
| `row.comment` | Row-level comment (get/set) |

---

## Comments

Every node class exposes a `.comment` property for the `#` lines that appear directly above it in the source file. `Array`, `Table`, and `KeyedTable` additionally expose `.header_comment` for the comment that appears before the first element or header row.

```python
deps = root["deps"].as_keyed_table()
print(deps.header_comment)   # comment before the column header line
print(deps["plot"].comment)  # comment above the "plot" row
```
