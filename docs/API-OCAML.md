# OCaml API — `bindings/ocaml/`

The OCaml binding is a thin Dune library over the C FFI. It exposes opaque document handles, integer node handles, variant status values, and functions that closely mirror `ffi/coda_ffi.h`.

Compared with the Python and C++ wrappers, the OCaml surface is deliberately low-level: lookups return `option` where absence is expected, mutations return a `status`, and type mismatches generally return the C FFI's neutral value (`0`, `""`, or a non-`Ok` status) instead of raising.

---

## Setup

The binding lives in `bindings/ocaml/` and is built by the repository's root `dune-project`.

```bash
devbox run -- dune build
devbox run -- just test-ocaml
```

The library stanza is named `coda`, so OCaml code in this workspace can use:

```ocaml
open Coda
```

The OCaml stubs compile `ffi/coda_ffi.cpp` into the OCaml library via `bindings/ocaml/coda_ffi_bridge.cpp`, so the test binding does not require a separately installed `libcoda_ffi` shared library.

---

## Parsing

```ocaml
open Coda

let doc =
  match parse_file "project.coda" with
  | Ok d -> d
  | Error e -> failwith e.message

let doc_from_string =
  match parse "name myproject\n" (Some "inline.coda") with
  | Ok d -> d
  | Error e -> failwith e.message

(* Empty editable document: parse an empty source string. *)
let empty_doc =
  match parse "" None with
  | Ok d -> d
  | Error e -> failwith e.message
```

Parsing returns `(doc, error) result`. `error` contains the C FFI parse code, 1-based line and column, byte offset, and formatted message.

```ocaml
type error = {
  code : int;
  line : int;
  col : int;
  offset : int;
  message : string;
}
```

> The current OCaml wrapper does not expose an explicit `doc_free` function. The public API treats `doc` as an opaque handle owned by the binding for the lifetime of the process.

---

## Reading values

`root doc` returns the root block node. A `node` is an integer handle; `0` is the null / invalid handle. Use `map_get` and `keyed_table_row_get` for optional lookups.

```ocaml
let root_node = root doc

(* Scalar string *)
let name =
  match map_get doc root_node "name" with
  | Some n -> string_get doc n
  | None -> ""

(* Membership test *)
let has_version = map_get doc root_node "version" <> None

(* Block entries in insertion order *)
let compiler =
  match map_get doc root_node "compiler" with
  | Some n -> n
  | None -> 0

let keys =
  List.init (map_len doc compiler) (fun i -> map_key_at doc compiler i)

(* Array elements *)
let targets = Option.get (map_get doc root_node "targets")
let target_values =
  List.init (array_len doc targets) (fun i ->
    string_get doc (array_get doc targets i))

(* Plain table rows *)
let releases = Option.get (map_get doc root_node "releases")
let first_release = table_row_at doc releases 0
let first_version = row_get doc first_release "version"

(* Keyed table rows *)
let deps = Option.get (map_get doc root_node "deps")
let plot_link =
  match keyed_table_row_get doc deps "plot" with
  | Some row -> row_get doc row "link"
  | None -> ""
```

Node kinds and status values are mapped to variants:

```ocaml
type node_kind =
  | NodeNull | NodeString | NodeBlock | NodeArray
  | NodeTable | NodeKeyedTable | NodeRow

type status = Ok | Err | NotFound | BadKind | OutOfRange
```

---

## Creating and modifying

Create new nodes in the target document arena, then attach them with the relevant mutator. To create an empty document, parse an empty string.

```ocaml
let doc =
  match parse "" None with
  | Ok d -> d
  | Error e -> failwith e.message

let root_node = root doc

(* Scalar *)
let name = new_string doc "myproject"
let _ = map_set doc root_node "name" name

let version = map_get_or_insert doc root_node "version"
let _ = string_set doc version "1.0.0"

(* Block *)
let compiler = new_block doc
let _ = map_set doc compiler "debug" (new_string doc "false")
let _ = map_set doc compiler "optimize" (new_string doc "true")
let _ = map_set doc root_node "compiler" compiler

(* Array *)
let targets = new_array doc
let _ = node_header_comment_set doc targets "supported build targets"
let _ = array_push doc targets (new_string doc "x86_64-linux")
let _ = array_push doc targets (new_string doc "x86_64-windows")
let _ = map_set doc root_node "targets" targets

(* Plain table *)
let releases = new_table doc
let _ = table_col_append doc releases "version"
let _ = table_col_append doc releases "date"
let release_row = new_row doc
let _ = row_set doc release_row "version" "1.0.0"
let _ = row_set doc release_row "date" "2025-01-01"
let _ = table_row_append doc releases release_row
let _ = map_set doc root_node "releases" releases

(* Keyed table *)
let deps = new_keyed_table doc
let _ = keyed_table_col_append doc deps "link"
let _ = keyed_table_col_append doc deps "version"
let plot = new_row doc
let _ = row_set doc plot "link" "github.com/zane-lang/plot"
let _ = row_set doc plot "version" "4.0.3"
let _ = keyed_table_row_set doc deps "plot" plot
let _ = map_set doc root_node "deps" deps
```

Check mutation results when you need to distinguish `BadKind`, `OutOfRange`, or `NotFound`:

```ocaml
match row_remove doc release_row "missing" with
| Ok -> ()
| NotFound -> prerr_endline "column did not exist"
| BadKind | OutOfRange | Err -> prerr_endline "row_remove failed"
```

---

## Sorting

The OCaml public API exposes sub-tree ordering. Apply it to `root doc` to sort the whole document.

```ocaml
node_order doc (root doc)

node_order_weighted doc (root doc) [
  ("name", 100.0);
  ("version", 90.0);
]
```

Default ordering groups scalar strings before containers and sorts alphabetically inside each group. Weighted ordering places higher weights first and uses alphabetical order for ties.

---

## Serialisation

```ocaml
match serialize doc None with
| Ok text -> print_string text
| Error e -> prerr_endline e.message

match serialize doc (Some "  ") with
| Ok text -> print_string text
| Error e -> prerr_endline e.message

match serialize_node doc compiler None with
| Ok text -> print_string text
| Error e -> prerr_endline e.message
```

`None` uses the default tab indent. `Some "  "` uses two spaces.

---

## Comments

Node comments are stored without the leading `#`; the serialiser adds it back. Arrays, tables, and keyed tables also have a header comment. Table rows use separate row-comment accessors.

```ocaml
let deps = Option.get (map_get doc (root doc) "deps")

let _ = node_comment_set doc deps "dependency table"
let comment = node_comment_get doc deps

let _ = node_header_comment_set doc deps "optional"
let header_comment = node_header_comment_get doc deps

let plot = Option.get (keyed_table_row_get doc deps "plot")
let _ = row_comment_set doc plot "main dependency"
let row_comment = row_comment_get doc plot
```

---

## Function reference

### Document

| Function | Description |
|---|---|
| `parse_file path` | Parse a file path; returns `(doc, error) result` |
| `parse src filename` | Parse UTF-8 text with optional filename; returns `(doc, error) result` |
| `serialize doc indent` | Serialise a document with optional indent string |
| `serialize_node doc node indent` | Serialise a single node with optional indent string |
| `root doc` | Return the root block node handle |

### Node inspection and comments

| Function | Description |
|---|---|
| `node_kind doc node` | Return `node_kind` |
| `node_is_container doc node` | `true` for block, array, table, or keyed table |
| `node_comment_get doc node` | Get pre-node comment |
| `node_comment_set doc node text` | Set pre-node comment; returns `status` |
| `node_header_comment_get doc node` | Get array/table header comment |
| `node_header_comment_set doc node text` | Set array/table header comment; returns `status` |
| `row_comment_get doc row` | Get row-level comment |
| `row_comment_set doc row text` | Set row-level comment; returns `status` |

### Strings and maps

| Function | Description |
|---|---|
| `string_get doc node` | Read a string node |
| `string_set doc node value` | Set a string node; returns `status` |
| `map_len doc block` | Number of entries in a block |
| `map_key_at doc block i` | Key at index `i` |
| `map_value_at doc block i` | Value node at index `i` |
| `map_get doc block key` | Lookup by key; returns `node option` |
| `map_get_or_insert doc block key` | Lookup or create an empty string node |
| `map_set doc block key node` | Insert or replace a block entry; returns `status` |
| `map_remove doc block key` | Remove a block entry; returns `status` |

### Arrays

| Function | Description |
|---|---|
| `array_len doc array` | Number of elements |
| `array_get doc array i` | Element at index `i` (`0` if absent/out of range) |
| `array_set doc array i node` | Replace an element; returns `status` |
| `array_push doc array node` | Append an element; returns `status` |
| `array_remove doc array i` | Remove an element; returns `status` |

### Plain tables

| Function | Description |
|---|---|
| `table_col_count doc table` | Column count |
| `table_col_name doc table i` | Column name at index `i` |
| `table_col_append doc table name` | Append a column; returns `status` |
| `table_row_count doc table` | Row count |
| `table_row_at doc table i` | Row node at index `i` |
| `table_row_append doc table row` | Append a row; returns `status` |
| `table_row_set doc table i row` | Replace a row; returns `status` |
| `table_row_remove doc table i` | Remove a row; returns `status` |

### Keyed tables

| Function | Description |
|---|---|
| `keyed_table_col_count doc table` | Non-key column count |
| `keyed_table_col_name doc table i` | Non-key column name at index `i` |
| `keyed_table_col_append doc table name` | Append a non-key column; returns `status` |
| `keyed_table_row_count doc table` | Row count |
| `keyed_table_row_key_at doc table i` | Row key at index `i` |
| `keyed_table_row_at doc table i` | Row node at index `i` |
| `keyed_table_row_get doc table key` | Lookup by row key; returns `node option` |
| `keyed_table_row_set doc table key row` | Insert or replace a row; returns `status` |
| `keyed_table_row_remove doc table key` | Remove a row; returns `status` |

### Rows

| Function | Description |
|---|---|
| `row_get doc row col` | Get a column value; returns `""` if absent |
| `row_set doc row col value` | Set a column value; returns `status` |
| `row_remove doc row col` | Remove a column; returns `status` |
| `row_col_count doc row` | Number of stored columns |
| `row_col_name_at doc row i` | Column name at index `i` |
| `row_col_value_at doc row i` | Column value at index `i` |

### Ordering

| Function | Description |
|---|---|
| `node_order doc node` | Sort a sub-tree with the default ordering |
| `node_order_weighted doc node weights` | Sort a sub-tree by `(key * weight)` list |

### Node creation

| Function | Description |
|---|---|
| `new_string doc value` | Create a string node |
| `new_block doc` | Create a block node |
| `new_array doc` | Create an array node |
| `new_table doc` | Create a plain table node |
| `new_keyed_table doc` | Create a keyed table node |
| `new_row doc` | Create a row node |
