# Crystal API — `bindings/crystal/`

The Crystal binding declares the hardened C FFI in `lib_coda.cr` and exposes classes for each Coda node kind in `coda.cr`, mirroring the Python API.

## Build and require

The binding links the static archive `libcoda_ffi.a` and the C++ runtime. Build the archive with the C compiler Crystal links with (`$CC`, else `cc`), so both agree on the C++ runtime:

```bash
devbox run -- just build-crystal    # build/crystal/libcoda_ffi.a
```

Put its directory on Crystal's library path, ahead of Crystal's own:

```bash
export CRYSTAL_LIBRARY_PATH="$PWD/build/crystal:$(crystal env CRYSTAL_LIBRARY_PATH)"
```

```crystal
require "path/to/coda/bindings/crystal/coda"
```

The binding checks the library's ABI version at startup and raises `Coda::Error` on a mismatch.

## Documents and lifetime

```crystal
Coda::Doc.parse(text, filename: "project.coda") do |doc|
  root = doc.root
end

Coda::Doc.parse_file("project.coda") do |doc|
  # ...
end

File.open("project.coda") do |file|
  Coda::Doc.parse(file, filename: "project.coda") do |doc|
    # ...
  end
end

Coda::Doc.new do |doc|
  # ...
end
```

A document owns all of its nodes. The block forms call `free` when the block returns. Every constructor also has a form without a block, which returns the document; call `free` when done. The finalizer is only a fallback, and calling `free` more than once is safe.

Node objects become invalid when their document is freed or when their node or subtree is removed or replaced. Using an invalid node raises `Coda::Error`; stale nodes never alias newly created nodes.

```crystal
doc = Coda::Doc.parse("name x\n")
node = doc.root["name"]
doc.free
node.to_s # raises Coda::Error
```

Ordering does not invalidate nodes.

## Ownership when building

A newly created container or row starts detached. Inserting it transfers it into exactly one parent in the same document.

The binding rejects:

- inserting a node created in another document;
- attaching the same node to two parents;
- attaching a node beneath itself or one of its descendants;
- reusing a node after it was removed or replaced.

```crystal
Coda::Doc.new do |doc|
  root = doc.root
  block = Coda::Block.new
  root["compiler"] = block
  block["debug"] = "false"
end
```

Create a fresh node for each destination rather than reusing one attached instance.

## Reading values

```crystal
Coda::Doc.parse(text) do |doc|
  root = doc.root

  name = root["name"].as_string.value
  compiler = root["compiler"].as_block
  targets = root["targets"].as_array
  releases = root["releases"].as_table
  deps = root["deps"].as_keyed_table
end
```

`as_string`, `as_block`, `as_array`, `as_table`, and `as_keyed_table` raise `TypeCastError` for the wrong kind. Lookups return the node's own class, so `case node when Coda::Block` also works.

String nodes also implement `to_s` and `==` with a `String`.

## Blocks

```crystal
root["name"] = "myproject"
root.insert("version", "1.0.0")

value = root["name"]      # raises KeyError when absent
maybe = root["name"]?     # nil when absent
exists = root.has_key?("name")
length = root.size

root.delete("version")
```

A block is `Enumerable({String, Node})`: iteration yields key and node pairs in insertion order. `keys` lists the keys.

`get_or_insert(key)` is the explicit operation that creates an empty string node when the key is absent. Normal indexing never inserts.

## Arrays

```crystal
root["targets"] = Coda::Array.new
targets = root["targets"].as_array

targets << "x86_64-linux"
targets.append(Coda::Block.new)

item = targets[0]
targets[-1] = "replacement"
targets.delete_at(0)
```

Negative indices count from the end; an index out of range raises `IndexError`. An array is `Enumerable(Node)`. Array order is preserved by document ordering.

## Plain tables

Column declaration order is preserved, including empty tables. Duplicate columns are rejected.

```crystal
root["releases"] = Coda::Table.new(["version", "date"])
releases = root["releases"].as_table

releases << Coda::Row.new.insert("version", "1.0.0").insert("date", "2026-01-01")

releases.columns # => ["version", "date"]
```

A row must contain exactly the declared fields when attached. Columns can only be appended before the first row. Once attached, existing field values may be changed, but required fields cannot be deleted and unknown fields cannot be added.

```crystal
releases[0]["version"] = "1.1.0"
```

Table indexing supports negative indices and returns `Row`. A table is `Enumerable(Row)`.

## Keyed tables

`columns` lists the columns after the key.

```crystal
root["deps"] = Coda::KeyedTable.new(["link", "version"])
deps = root["deps"].as_keyed_table

deps["plot"] = Coda::Row.new
  .insert("link", "github.com/zane-lang/plot")
  .insert("version", "4.0.3")

link = deps["plot"]["link"]
```

Missing keys raise `KeyError` and do not insert; `[]?` returns `nil` instead. A keyed table is `Enumerable({String, Row})`, and `keys` lists the row keys.

```crystal
deps.order
deps.order_weighted([{"plot", 100.0}, {"http", 50.0}])
```

These methods sort keyed rows by row key. Document-level ordering applies the same behavior recursively.

## Rows

A row is a flat insertion-ordered mapping of strings, and an `Enumerable({String, String})`:

```crystal
row = Coda::Row.new
row["name"] = "example"
row.insert("version", "1")

row.each { |column, value| }
row.to_h # => {"name" => "example", "version" => "1"}
```

`row[column]` raises `KeyError` for a missing column; `row[column]?` returns `nil`. Before attachment, fields can be freely added and removed. Attached rows obey their table schema.

## Comments

Every node has `comment`. Arrays and tables also have `header_comment`.

```crystal
node.comment = "shown above this node"
table.header_comment = "shown above the table header"
row.comment = "shown above this row"
```

Comments are stored without the leading `#`.

## Ordering

```crystal
doc.order
doc.order_weighted([{"name", 100.0}, {"version", 90.0}])

root["compiler"].as_block.order
root["deps"].as_keyed_table.order
```

Block fields are ordered with scalars first, then containers, alphabetically within each group. Weighted ordering sorts by descending weight with alphabetical ties. Keyed-table rows use their keys. Array and plain-table row order is preserved.

Existing nodes remain valid after ordering; only index-based iteration order changes.

## Serialization

```crystal
text = doc.serialize
text = doc.serialize(indent: "  ")

doc.save("out.coda")
doc.save("out.coda", indent: "  ")

subtree = root["compiler"].serialize
```

Serializing an invalid or stale node raises `Coda::Error` rather than returning the document root.

## Errors

```crystal
begin
  Coda::Doc.parse(bad_text, filename: "project.coda")
rescue error : Coda::ParseError
  puts error.code
  puts error.line, error.col, error.offset
  puts Coda.parse_error_code_name(error.code)
end
```

`Coda::ParseError` inherits from `Coda::Error`. Ownership, stale-handle, schema, and serialization failures raise `Coda::Error`; lookups raise `KeyError` or `IndexError`, and narrowing to the wrong kind raises `TypeCastError`.
