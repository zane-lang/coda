# C FFI API — `ffi/coda_ffi.h`

The C FFI is the shared native layer used by the Python and OCaml bindings. It exposes an owned document arena and opaque node handles.

## Build

```bash
devbox run -- just build
```

This produces `build/libcoda_ffi.so` on Linux. Cross-platform artifacts are produced by:

```bash
devbox run -- just cross-all
```

Include:

```c
#include "ffi/coda_ffi.h"
```

## Ownership and handle safety

A `coda_doc_t*` owns every node created in that document. A `coda_node_t` is an opaque, document-scoped handle; `0` is null/invalid.

Handles have these guarantees:

- They remain valid across `coda_doc_order`, `coda_doc_order_weighted`, `coda_node_order`, and `coda_node_order_weighted`.
- Removing or replacing a node invalidates handles to that node and its descendants.
- A stale handle never aliases a later node, even if the internal arena slot is reused.
- A handle from one document cannot be passed into another document.
- A node can have only one parent. Reusing an attached node or creating a cycle returns `CODA_ERR`.
- Attaching a detached node transfers it to the destination container.

```c
coda_doc_t* doc = coda_doc_new();
coda_node_t root = coda_doc_root(doc);
coda_node_t value = coda_new_string(doc, "hello", 5);

if (coda_map_set(doc, root, "name", 4, value) != CODA_OK) {
    /* invalid ownership, kind, or allocation failure */
}
```

Always free the document:

```c
coda_doc_free(doc);
```

## Borrowed and owned strings

`coda_str_t` is borrowed from document storage. Copy it before any document mutation or before freeing the document.

```c
coda_str_t view = coda_string_get(doc, node);
```

`coda_owned_str_t` owns an allocated buffer and must be released with `coda_owned_str_free`:

```c
coda_error_t err = {0};
coda_owned_str_t text = coda_doc_serialize(doc, NULL, 0, &err);
if (text.ptr) {
    fwrite(text.ptr, 1, text.len, stdout);
    coda_owned_str_free(text);
} else {
    fwrite(err.message.ptr, 1, err.message.len, stderr);
    coda_error_clear(&err);
}
```

## Parsing

```c
coda_error_t err = {0};
const char* source = "name myproject\n";

coda_doc_t* doc = coda_doc_parse(
    source,
    strlen(source),
    "project.coda",
    &err
);

if (!doc) {
    fprintf(stderr, "%.*s\n", (int)err.message.len, err.message.ptr);
    coda_error_clear(&err);
}
```

Available entry points:

```c
coda_doc_parse(src, len, filename, err);
coda_doc_parse_file(path, err);
coda_doc_parse_fp(fp, filename, err);
```

For parse failures, `coda_error_t.code` maps to `coda_parse_error_code_t`. Use `coda_parse_error_code_name(code)` for its stable name. Non-parse failures use the documented `coda_error_code_t` sentinels.

## Node kinds

```c
switch (coda_node_kind(doc, node)) {
case CODA_NODE_STRING:
case CODA_NODE_BLOCK:
case CODA_NODE_ARRAY:
case CODA_NODE_TABLE:
case CODA_NODE_KEYED_TABLE:
case CODA_NODE_ROW:
case CODA_NODE_NULL:
    break;
}
```

`coda_node_is_container` returns true for blocks, arrays, tables, and keyed tables.

## Blocks

```c
coda_node_t root = coda_doc_root(doc);
coda_node_t name = coda_map_get(doc, root, "name", 4);

size_t count = coda_map_len(doc, root);
for (size_t i = 0; i < count; ++i) {
    coda_str_t key = coda_map_key_at(doc, root, i);
    coda_node_t value = coda_map_value_at(doc, root, i);
}
```

Mutation:

```c
coda_map_get_or_insert(doc, block, key, key_len);
coda_map_set(doc, block, key, key_len, value);
coda_map_remove(doc, block, key, key_len);
```

Normal `coda_map_get` never inserts.

## Strings

```c
coda_str_t value = coda_string_get(doc, node);
coda_status_t status = coda_string_set(doc, node, text, text_len);
```

## Arrays

```c
size_t n = coda_array_len(doc, array);
coda_node_t item = coda_array_get(doc, array, index);

coda_array_push(doc, array, value);
coda_array_set(doc, array, index, value);
coda_array_remove(doc, array, index);
```

Negative indices are a binding-level feature; the C API takes `size_t`.

## Tables

Tables preserve their declared column order. Duplicate columns are rejected. Columns may only be appended before the first row is attached.

```c
coda_node_t table = coda_new_table(doc);
coda_table_col_append(doc, table, "version", 7);
coda_table_col_append(doc, table, "date", 4);

coda_node_t row = coda_new_row(doc);
coda_row_set(doc, row, "version", 7, "1.0.0", 5);
coda_row_set(doc, row, "date", 4, "2026-01-01", 10);
coda_table_row_append(doc, table, row);
```

Rows must contain exactly the declared fields when attached. Once attached, existing values may be changed, but required fields cannot be removed and unknown fields cannot be added.

Plain-table access:

```c
size_t columns = coda_table_col_count(doc, table);
coda_str_t column = coda_table_col_name(doc, table, column_index);
size_t rows = coda_table_row_count(doc, table);
coda_node_t row = coda_table_row_at(doc, table, row_index);
```

## Keyed tables

Keyed tables use the same column and row-schema rules:

```c
coda_node_t table = coda_new_keyed_table(doc);
coda_keyed_table_col_append(doc, table, "link", 4);

coda_node_t row = coda_new_row(doc);
coda_row_set(doc, row, "link", 4, "example.com", 11);
coda_keyed_table_row_set(doc, table, "example", 7, row);
```

Access:

```c
coda_keyed_table_row_get(doc, table, key, key_len);
coda_keyed_table_row_count(doc, table);
coda_keyed_table_row_key_at(doc, table, index);
coda_keyed_table_row_at(doc, table, index);
```

`coda_node_order` sorts keyed-table rows alphabetically by key. Weighted ordering uses row keys.

## Rows

```c
coda_str_t value = coda_row_get(doc, row, column, column_len);
coda_row_set(doc, row, column, column_len, value, value_len);
coda_row_remove(doc, row, column, column_len);
```

Iteration:

```c
size_t n = coda_row_col_count(doc, row);
for (size_t i = 0; i < n; ++i) {
    coda_str_t name = coda_row_col_name_at(doc, row, i);
    coda_str_t value = coda_row_col_value_at(doc, row, i);
}
```

## Comments

```c
coda_node_comment_get(doc, node);
coda_node_comment_set(doc, node, text, len);

coda_node_header_comment_get(doc, container);
coda_node_header_comment_set(doc, container, text, len);

coda_row_comment_get(doc, row);
coda_row_comment_set(doc, row, text, len);
```

Header comments apply to arrays, tables, and keyed tables.

## Ordering

```c
coda_doc_order(doc);
coda_node_order(doc, node);

const char* keys[] = {"name", "version"};
float weights[] = {100.0f, 90.0f};
coda_doc_order_weighted(doc, keys, weights, 2);
```

Ordering is in-place. Handles remain valid, but cached iteration indices become stale. Arrays and plain-table rows retain their order; keyed-table rows are sorted by key or weight.

## Node serialization

```c
coda_error_t err = {0};
coda_owned_str_t text = coda_node_serialize(doc, node, "  ", 2, &err);
```

The root block serializes without braces. Nested blocks include braces. Invalid or stale handles return `{NULL, 0}`, set `err.code` to `CODA_ERROR_INVALID_HANDLE`, and populate `err.message`; they are never interpreted as the document root.

## Status codes

| Status | Meaning |
|---|---|
| `CODA_OK` | Success |
| `CODA_ERR` | Invalid document/handle/ownership/schema or allocation failure |
| `CODA_NOT_FOUND` | Missing map key, keyed row, or row column |
| `CODA_BAD_KIND` | Operation does not apply to this node kind |
| `CODA_OUT_OF_RANGE` | Index is outside the container |

## ABI version

```c
uint32_t version = coda_ffi_abi_version();
```

Bindings should verify the expected ABI before making other calls.
