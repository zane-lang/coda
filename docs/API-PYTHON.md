# Python API — `coda`

The Python package uses `ctypes` over the hardened C FFI and exposes classes for each Coda node kind.

## Installation and imports

```bash
pip install coda-format
```

```python
from coda import (
    Doc, Block, Array, Table, KeyedTable, Row,
    Error, ParseError, get_abi_version, parse_error_code_name,
)
```

From a source checkout, import the package rather than the implementation module so the runtime safety hooks are installed:

```python
from bindings.python import Doc, Block, Array, Table, KeyedTable, Row
```

Build the native library first:

```bash
devbox run -- just build
```

## Documents and lifetime

```python
with Doc.parse(text, filename="project.coda") as doc:
    root = doc.root()

with Doc.parse_file("project.coda") as doc:
    ...

with open("project.coda", "rb") as file:
    with Doc.parse_fp(file, filename="project.coda") as doc:
        ...

with Doc.new() as doc:
    ...
```

A document owns all of its nodes. The context manager calls `free()` on exit; `__del__` is only a fallback.

Node wrappers become invalid when their document is freed or when their node/subtree is removed or replaced. Using an invalid wrapper raises `Error`; stale wrappers never alias newly created nodes.

```python
node = doc.root()["name"]
doc.free()
str(node)  # raises Error
```

Ordering does not invalidate node wrappers.

## Ownership when building

A newly created container or row starts detached. Inserting it transfers it into exactly one parent in the same document.

The binding rejects:

- inserting a node created in another document;
- attaching the same node to two parents;
- attaching a node beneath itself or one of its descendants;
- reusing a node after it was removed or replaced.

```python
with Doc.new() as doc:
    root = doc.root()
    block = Block()
    root["compiler"] = block
    block["debug"] = "false"
```

Create a fresh node for each destination rather than reusing one attached instance.

## Reading values

```python
with Doc.parse(text) as doc:
    root = doc.root()

    name = root["name"].as_string().value
    compiler = root["compiler"].as_block()
    targets = root["targets"].as_array()
    releases = root["releases"].as_table()
    deps = root["deps"].as_keyed_table()
```

`as_string`, `as_block`, `as_array`, `as_table`, and `as_keyed_table` raise `TypeError` for the wrong kind.

String nodes also implement `str(node)` and comparison with Python strings.

## Blocks

```python
root["name"] = "myproject"
root.insert("version", "1.0.0")

value = root["name"]       # raises KeyError when absent
exists = "name" in root
length = len(root)

del root["version"]
```

Iteration yields `(key, Node)` pairs in insertion order.

`get_or_insert(key)` is the explicit operation that creates an empty string node when the key is absent. Normal indexing never inserts.

## Arrays

```python
root["targets"] = Array()
targets = root["targets"].as_array()

targets.append("x86_64-linux")
targets.append(Block())

item = targets[0]
targets[-1] = "replacement"
del targets[0]
```

Python-style negative indices are supported. Array order is preserved by document ordering.

## Plain tables

Column declaration order is preserved, including empty tables. Duplicate columns are rejected.

```python
root["releases"] = Table(["version", "date"])
releases = root["releases"].as_table()

row = Row()
row["version"] = "1.0.0"
row["date"] = "2026-01-01"
releases.append(row)

assert releases.columns() == ["version", "date"]
```

A row must contain exactly the declared fields when attached. Columns can only be appended before the first row. Once attached, existing field values may be changed, but required fields cannot be deleted and unknown fields cannot be added.

```python
releases[0]["version"] = "1.1.0"
```

Table indexing supports negative indices and returns `Row`.

## Keyed tables

```python
root["deps"] = KeyedTable(["link", "version"])
deps = root["deps"].as_keyed_table()

row = Row()
row["link"] = "github.com/zane-lang/plot"
row["version"] = "4.0.3"
deps["plot"] = row

link = deps["plot"]["link"]
```

Missing keys raise `KeyError` and do not insert. Iteration yields `(key, Row)` pairs.

```python
deps.order()
deps.order_weighted([("plot", 100.0), ("http", 50.0)])
```

These methods sort keyed rows by row key. Document-level ordering applies the same behavior recursively.

## Rows

A row is a flat insertion-ordered mapping of strings:

```python
row = Row()
row["name"] = "example"
row.insert("version", "1")

for column, value in row:
    ...
```

Before attachment, fields can be freely added and removed. Attached rows obey their table schema.

## Comments

Every node has a `.comment` property. Arrays and tables also have `.header_comment`.

```python
node.comment = "shown above this node"
table.header_comment = "shown above the table header"
row.comment = "shown above this row"
```

Comments are stored without the leading `#`.

## Ordering

```python
doc.order()
doc.order_weighted([("name", 100.0), ("version", 90.0)])

root["compiler"].as_block().order()
root["deps"].as_keyed_table().order()
```

Block fields are ordered with scalars first, then containers, alphabetically within each group. Weighted ordering sorts by descending weight with alphabetical ties. Keyed-table rows use their keys. Array and plain-table row order is preserved.

Existing node wrappers remain valid after ordering; only index-based iteration order changes.

## Serialization

```python
text = doc.serialize()
text = doc.serialize(indent="  ")

doc.save("out.coda")
doc.save("out.coda", indent="  ")

subtree = root["compiler"].serialize()
```

Serializing an invalid or stale node raises `Error` rather than returning the document root.

## Errors

```python
try:
    Doc.parse(bad_text, filename="project.coda")
except ParseError as error:
    print(error.code)
    print(error.line, error.col, error.offset)
    print(parse_error_code_name(error.code))
```

`ParseError` inherits from `Error`. Ownership, stale-handle, schema, type, and serialization failures raise `Error` or the standard lookup/type exception documented by the operation.
