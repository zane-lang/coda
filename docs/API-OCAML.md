# OCaml API — `bindings/ocaml/`

The OCaml binding is a thin, typed layer over the hardened C FFI.

## Build and test

```bash
devbox run -- just test-ocaml
```

The Dune library embeds the native implementation; no separately installed shared library is required.

```ocaml
open Coda
```

## Documents and lifetime

```ocaml
match parse_file "project.coda" with
| Ok doc ->
    let root_node = root doc in
    (* use document *)
    free doc
| Error error ->
    prerr_endline error.message
```

A `doc` is an owning OCaml custom block. Native memory is released automatically by its finalizer, and `free` provides deterministic release.

Using a document after `free` raises `Failure`. Calling `free` more than once is safe.

```ocaml
val free : doc -> unit
```

A `node` is an opaque integer handle scoped to its document. `0` represents null/invalid.

Handles remain stable across ordering. Removing or replacing a subtree invalidates its handles; stale handles never alias newly created nodes. Node handles cannot be moved between documents, attached to multiple parents, or used to create cycles; such mutations return `Err`.

## Parsing and serialization

```ocaml
val parse_file : string -> (doc, error) result
val parse : string -> string option -> (doc, error) result
val serialize : doc -> string option -> (string, error) result
val serialize_node : doc -> node -> string option -> (string, error) result
```

Examples:

```ocaml
match parse source (Some "project.coda") with
| Error error ->
    Printf.eprintf "%d:%d: %s\n" error.line error.col error.message
| Ok doc ->
    match serialize doc (Some "  ") with
    | Ok text -> print_string text
    | Error error -> prerr_endline error.message
```

Serializing an invalid or stale node returns `Error`; it is never interpreted as the document root.

## Errors and status values

```ocaml
type status =
  | Ok
  | Err
  | NotFound
  | BadKind
  | OutOfRange

type error = {
  code : int;
  line : int;
  col : int;
  offset : int;
  message : string;
}
```

Lookups that naturally have absence use `option`; mutations use `status`.

## Node inspection

```ocaml
type node_kind =
  | NodeNull
  | NodeString
  | NodeBlock
  | NodeArray
  | NodeTable
  | NodeKeyedTable
  | NodeRow

val root : doc -> node
val node_kind : doc -> node -> node_kind
val node_is_container : doc -> node -> bool
```

## Strings

```ocaml
val string_get : doc -> node -> string
val string_set : doc -> node -> string -> status
```

## Blocks

```ocaml
val map_len : doc -> node -> int
val map_key_at : doc -> node -> int -> string
val map_value_at : doc -> node -> int -> node
val map_get : doc -> node -> string -> node option
val map_get_or_insert : doc -> node -> string -> node
val map_set : doc -> node -> string -> node -> status
val map_remove : doc -> node -> string -> status
```

Normal `map_get` does not insert:

```ocaml
match map_get doc (root doc) "name" with
| Some value -> print_endline (string_get doc value)
| None -> ()
```

## Arrays

```ocaml
val array_len : doc -> node -> int
val array_get : doc -> node -> int -> node
val array_set : doc -> node -> int -> node -> status
val array_push : doc -> node -> node -> status
val array_remove : doc -> node -> int -> status
```

The OCaml API uses non-negative integer indices. Out-of-range accessors return the C ABI sentinel or an `OutOfRange` status as appropriate.

## Plain tables

```ocaml
val table_col_count : doc -> node -> int
val table_col_name : doc -> node -> int -> string
val table_col_append : doc -> node -> string -> status
val table_row_count : doc -> node -> int
val table_row_at : doc -> node -> int -> node
val table_row_append : doc -> node -> node -> status
val table_row_set : doc -> node -> int -> node -> status
val table_row_remove : doc -> node -> int -> status
```

Column declaration order is preserved. Duplicate columns are rejected, and columns can only be added before the first row is attached.

Rows must contain exactly the declared fields at attachment time. Once attached, existing values may be changed, but required fields cannot be removed and unknown fields cannot be added.

## Keyed tables

```ocaml
val keyed_table_col_count : doc -> node -> int
val keyed_table_col_name : doc -> node -> int -> string
val keyed_table_col_append : doc -> node -> string -> status
val keyed_table_row_count : doc -> node -> int
val keyed_table_row_key_at : doc -> node -> int -> string
val keyed_table_row_at : doc -> node -> int -> node
val keyed_table_row_get : doc -> node -> string -> node option
val keyed_table_row_set : doc -> node -> string -> node -> status
val keyed_table_row_remove : doc -> node -> string -> status
```

```ocaml
match keyed_table_row_get doc table "plot" with
| Some row -> print_endline (row_get doc row "link")
| None -> ()
```

## Rows

```ocaml
val row_get : doc -> node -> string -> string
val row_set : doc -> node -> string -> string -> status
val row_remove : doc -> node -> string -> status
val row_col_count : doc -> node -> int
val row_col_name_at : doc -> node -> int -> string
val row_col_value_at : doc -> node -> int -> string
```

Detached rows may be built freely. Attached rows obey the schema of their containing table.

## Comments

```ocaml
val node_comment_get : doc -> node -> string
val node_comment_set : doc -> node -> string -> status
val node_header_comment_get : doc -> node -> string
val node_header_comment_set : doc -> node -> string -> status
val row_comment_get : doc -> node -> string
val row_comment_set : doc -> node -> string -> status
```

Header comments apply to arrays, plain tables, and keyed tables.

## Ordering

```ocaml
val node_order : doc -> node -> unit
val node_order_weighted : doc -> node -> (string * float) list -> unit
```

```ocaml
node_order doc (root doc);
node_order_weighted doc (root doc) [
  ("name", 100.0);
  ("version", 90.0);
]
```

Block keys are ordered with scalars first, then containers, alphabetically within each group. Weighted ordering uses descending weight with alphabetical ties. Keyed-table rows are ordered by row key or row-key weight. Arrays and plain-table row order are preserved.

Ordering changes index-based iteration order but does not invalidate handles.

## Creating nodes

New nodes are detached and must be attached exactly once to a container in the same document.

```ocaml
val new_string : doc -> string -> node
val new_block : doc -> node
val new_array : doc -> node
val new_table : doc -> node
val new_keyed_table : doc -> node
val new_row : doc -> node
```

Example:

```ocaml
match parse "" None with
| Error error -> prerr_endline error.message
| Ok doc ->
    let value = new_string doc "myproject" in
    ignore (map_set doc (root doc) "name" value);
    free doc
```
