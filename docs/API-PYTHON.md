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
       CodaDoc, CodaBlock, CodaString, CodaArray,
       CodaTable, CodaKeyedTable, CodaRow,
   )
   ```

---

## Parsing

```python
# From a string (use as a context manager to auto-free)
with CodaDoc.parse(text) as doc:
    ...

# From a string with an optional filename hint for error messages
with CodaDoc.parse(text, filename="project.coda") as doc:
    ...

# From a file path
with CodaDoc.parse_file("project.coda") as doc:
    ...

# Create an empty document
doc = CodaDoc.new()
```

---

## Reading values

`doc.root()` returns a `CodaBlock` — the top-level map. From there, index with `[]` to reach child nodes and cast them with `as_*()`.

```python
with CodaDoc.parse(text) as doc:
    root = doc.root()                          # CodaBlock

    # Scalar string
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
    plot_row = deps["plot"]                    # CodaRow
    print(plot_row["link"])                    # "github.com/zane-lang/plot"
```

---

## Creating and modifying

```python
doc  = CodaDoc.new()
root = doc.root()

# Insert a scalar
root["name"] = CodaString(doc, "myproject")

# Insert a block
compiler = CodaBlock(doc)
compiler["debug"] = CodaString(doc, "false")
root["compiler"] = compiler

# Modify an existing scalar
root["name"].as_string().value = "renamed"

doc.save("out.coda")
```

---

## Sorting

```python
doc.order()                                         # alphabetical, scalars first
doc.order_weighted([("name", 100), ("type", 90)])   # by weight (higher → top)
```

---

## Serialisation

```python
text = doc.serialize()           # default tab indent
text = doc.serialize(indent="  ")

doc.save("out.coda")
doc.save("out.coda", indent="  ")
```

---

## Class reference

### `CodaDoc`

| Method / attribute | Description |
|---|---|
| `CodaDoc.parse(text, filename?)` | Parse UTF-8 text; returns `CodaDoc` |
| `CodaDoc.parse_file(path)` | Parse from a file |
| `CodaDoc.new()` | Create an empty document |
| `root()` | Return the root `CodaBlock` |
| `serialize(indent?)` | Return Coda text as `str` |
| `save(path, indent?)` | Write to disk |
| `order()` | Sort all keys alphabetically |
| `order_weighted(weights)` | Sort by `[(key, float), …]` weight list |
| `free()` | Explicitly free the document (auto-called by context manager) |

### `CodaBlock`

Ordered map of `str → CodaNode`.

| Operation | Description |
|---|---|
| `node["key"]` | Look up child node |
| `node["key"] = value` | Insert or replace a child |
| `del node["key"]` | Remove a child |
| `"key" in node` | Membership test |
| `for key, child in node` | Iterate in insertion order |
| `len(node)` | Number of entries |
| `node.get_or_insert("key")` | Get or create an empty string node |
| `node.order()` | Sort this block's keys |
| `node.order_weighted(weights)` | Sort by weight |
| `node.comment` | Pre-node comment string (get/set) |

### `CodaString`

Leaf string value.

| Operation | Description |
|---|---|
| `CodaString(doc, "value")` | Create a new string node |
| `node.value` | Get/set the string value |
| `str(node)` | Equivalent to `node.value` |

### `CodaArray`

Ordered list of `CodaNode`.

| Operation | Description |
|---|---|
| `node[i]` | Get item by index |
| `node[i] = value` | Replace item |
| `del node[i]` | Remove item |
| `node.append(value)` | Append a node |
| `for item in node` | Iterate |
| `len(node)` | Length |
| `node.header_comment` | Comment before the first element (get/set) |

### `CodaTable`

Plain (anonymous-row) table.

| Operation | Description |
|---|---|
| `node[i]` | Get row by index (returns `CodaRow`) |
| `node[i] = row` | Replace row |
| `del node[i]` | Remove row |
| `for row in node` | Iterate rows |
| `len(node)` | Row count |
| `node.columns()` | List of column name strings |
| `node.append_col(name)` | Add a column |
| `node.append(row)` | Append a `CodaRow` |
| `node.header_comment` | Comment before the header row (get/set) |

### `CodaKeyedTable`

Keyed table — rows indexed by their key string.

| Operation | Description |
|---|---|
| `node["key"]` | Get row by key (returns `CodaRow`) |
| `node["key"] = row` | Insert or replace row |
| `del node["key"]` | Remove row |
| `"key" in node` | Membership test |
| `for key, row in node` | Iterate in insertion order |
| `len(node)` | Row count |
| `node.columns()` | List of (non-key) column name strings |
| `node.append_col(name)` | Add a column |
| `node.header_comment` | Comment before the header row (get/set) |
| `node.order()` | Sort rows alphabetically by key |
| `node.order_weighted(weights)` | Sort rows by weight |

### `CodaRow`

A single table row — flat map of column name → string value.

| Operation | Description |
|---|---|
| `row["col"]` | Get column value as `str` |
| `row["col"] = "val"` | Set column value |
| `del row["col"]` | Remove column |
| `"col" in row` | Membership test |
| `for col, val in row` | Iterate columns |
| `len(row)` | Column count |
| `row.get("col", default)` | Get with fallback |
| `row.comment` | Row-level comment (get/set) |

---

## Comments

Every node class exposes a `.comment` property for the `#` lines that appear directly above it in the source file. `CodaArray`, `CodaTable`, and `CodaKeyedTable` additionally expose `.header_comment` for the comment that appears before the header row / first element.

```python
deps = root["deps"].as_keyed_table()
print(deps.header_comment)   # comment before the column header line
print(deps["plot"].comment)  # comment above the "plot" row
```
