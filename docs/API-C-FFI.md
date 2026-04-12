# C FFI API — `ffi/coda_ffi.h`

`libcoda_ffi` exposes a stable C ABI so you can embed the Coda parser in any language with a C FFI (Ruby, Python via ctypes, Lua, Zig, etc.).

## Setup

Build the shared library:

```bash
rake build        # libcoda_ffi.so for the host platform
rake cross-all    # cross-compile for all supported targets (output in dist/)
```

Then include the header and link against the library:

```c
#include "ffi/coda_ffi.h"
// link: -lcoda_ffi
```

---

## Types

| Type | Description |
|---|---|
| `coda_doc_t*` | Opaque document handle; free with `coda_doc_free()` |
| `coda_node_t` (`uint32_t`) | Opaque node handle; `0` = null/invalid |
| `coda_str_t` | **Borrowed** `{ const char* ptr; size_t len; }` view — see lifetime warning below |
| `coda_owned_str_t` | **Owned** `{ char* ptr; size_t len; }` — free with `coda_owned_str_free()` |
| `coda_error_t` | Parse error details — call `coda_error_clear()` to release the message buffer |
| `coda_node_kind_t` | Enum: `CODA_NODE_NULL`, `STRING`, `BLOCK`, `ARRAY`, `TABLE`, `KEYED_TABLE`, `ROW` |
| `coda_status_t` | Return code: `CODA_OK`, `CODA_ERR`, `CODA_NOT_FOUND`, `CODA_BAD_KIND`, `CODA_OUT_OF_RANGE` |

> **`coda_str_t` lifetime**: `coda_str_t` is a *borrowed* view into the document's internal storage. The pointer is valid only as long as the document is alive **and** no mutation has been made since the view was obtained. Always copy the bytes (e.g. `memcpy`) before calling any mutating function or `coda_doc_free()`.

---

## Document lifecycle

```c
// Parse from a byte buffer
coda_error_t err = {0};
coda_doc_t* doc = coda_doc_parse(src, len, "filename.coda", &err);
if (!doc) {
    fprintf(stderr, "parse error at %u:%u — %.*s\n",
            err.line, err.col, (int)err.message.len, err.message.ptr);
    coda_error_clear(&err);
    return 1;
}

// Parse from a file path
coda_doc_t* doc = coda_doc_parse_file("project.coda", &err);

// Parse from an open FILE*
coda_doc_t* doc = coda_doc_parse_fp(fp, "project.coda", &err);

// Create an empty document
coda_doc_t* doc = coda_doc_new();

// Free the document (also frees all node storage)
coda_doc_free(doc);
```

---

## Reading values

```c
coda_node_t root = coda_doc_root(doc);    // CODA_NODE_BLOCK

// --- String ---
coda_node_t n    = coda_map_get(doc, root, "name", 4);
coda_str_t  name = coda_string_get(doc, n);
printf("%.*s\n", (int)name.len, name.ptr);

// --- Block (map) ---
size_t len = coda_map_len(doc, root);
for (size_t i = 0; i < len; ++i) {
    coda_str_t  key = coda_map_key_at(doc, root, i);
    coda_node_t val = coda_map_value_at(doc, root, i);
    // inspect val...
}

// --- Bare-list array ---
coda_node_t arr  = coda_map_get(doc, root, "targets", 7);
size_t      alen = coda_array_len(doc, arr);
for (size_t i = 0; i < alen; ++i) {
    coda_node_t item = coda_array_get(doc, arr, i);
    coda_str_t  s    = coda_string_get(doc, item);
    printf("%.*s\n", (int)s.len, s.ptr);
}

// --- Keyed table ---
coda_node_t kt    = coda_map_get(doc, root, "deps", 4);
size_t      nrows = coda_keyed_table_row_count(doc, kt);
for (size_t r = 0; r < nrows; ++r) {
    coda_str_t  key = coda_keyed_table_row_key_at(doc, kt, r);
    coda_node_t row = coda_keyed_table_row_at(doc, kt, r);
    coda_str_t  lnk = coda_row_get(doc, row, "link", 4);
    printf("%.*s → %.*s\n", (int)key.len, key.ptr, (int)lnk.len, lnk.ptr);
}

// Direct row lookup by key
coda_node_t plot = coda_keyed_table_row_get(doc, kt, "plot", 4);
```

---

## Mutating values

```c
// Set a string value
coda_node_t n = coda_map_get(doc, root, "name", 4);
coda_string_set(doc, n, "newproject", 10);

// Insert a new key with get-or-insert
coda_node_t n = coda_map_get_or_insert(doc, root, "version", 7);
coda_string_set(doc, n, "1.0.0", 5);

// Remove a key
coda_map_remove(doc, root, "deprecated", 10);

// Set a row column value
coda_row_set(doc, row, "link", 4, "example.com", 11);
```

---

## Serialisation

```c
// Serialise the whole document
coda_owned_str_t out = coda_doc_serialize(doc, "\t", 1, NULL);
printf("%.*s", (int)out.len, out.ptr);
coda_owned_str_free(out);

// Serialise a single node
coda_owned_str_t out = coda_node_serialize(doc, node, "  ", 2, NULL);
coda_owned_str_free(out);
```

---

## Sorting

```c
// Sort the entire document (scalars first, then containers; alphabetical)
coda_doc_order(doc);

// Sort by weight table (higher weight → closer to top)
const char*  keys[]    = { "name", "type" };
const float  weights[] = { 100.0f, 90.0f };
coda_doc_order_weighted(doc, keys, weights, 2);

// Sort a sub-tree
coda_node_order(doc, node);
coda_node_order_weighted(doc, node, keys, weights, 2);
```

> **Warning**: after any `order` call, all previously obtained `coda_node_t` handles become **invalid**. Re-acquire them via `coda_doc_root()` and the traversal functions.

---

## Comments

```c
// Pre-node comment (# lines above a key/row)
coda_str_t c = coda_node_comment_get(doc, node);
coda_node_comment_set(doc, node, "my comment", 10);

// Header comment (# lines inside an array/table before the first item/header)
coda_str_t h = coda_node_header_comment_get(doc, node);
coda_node_header_comment_set(doc, node, "columns:", 8);

// Row-level comment inside a table
coda_str_t rc = coda_row_comment_get(doc, row);
coda_row_comment_set(doc, row, "optional dep", 12);
```

---

## Memory management

| Resource | Free function |
|---|---|
| `coda_doc_t*` | `coda_doc_free(doc)` |
| `coda_owned_str_t` | `coda_owned_str_free(s)` |
| `coda_error_t` message buffer | `coda_error_clear(&err)` (struct itself is caller-allocated) |

Do **not** mix these — calling the wrong free on a pointer is undefined behaviour (crashes on Windows when the library and caller use different CRT instances).

---

## Full function reference

### Document

| Function | Description |
|---|---|
| `coda_doc_new()` | Create empty document |
| `coda_doc_parse(src, len, file?, err?)` | Parse UTF-8 bytes |
| `coda_doc_parse_file(path, err?)` | Parse from file path |
| `coda_doc_parse_fp(fp, file?, err?)` | Parse from `FILE*` |
| `coda_doc_free(doc)` | Free the document |
| `coda_doc_root(doc)` | Get root `CODA_NODE_BLOCK` handle |
| `coda_doc_serialize(doc, indent, ilen, err?)` | Serialise to `coda_owned_str_t` |
| `coda_doc_order(doc)` | Sort all keys |
| `coda_doc_order_weighted(doc, keys, weights, n)` | Sort by weight |
| `coda_ffi_abi_version()` | ABI version integer |

### Node inspection

| Function | Description |
|---|---|
| `coda_node_kind(doc, n)` | Returns `coda_node_kind_t` |
| `coda_node_is_container(doc, n)` | 1 if BLOCK/ARRAY/TABLE/KEYED_TABLE |
| `coda_node_comment_get/set` | Pre-node comment |
| `coda_node_header_comment_get/set` | Header comment |
| `coda_node_serialize(doc, n, indent, ilen, err?)` | Serialise sub-tree |
| `coda_node_order(doc, n)` | Sort sub-tree |
| `coda_node_order_weighted(doc, n, keys, weights, n)` | Sort sub-tree by weight |

### String nodes (`CODA_NODE_STRING`)

| Function | Description |
|---|---|
| `coda_new_string(doc, s, len)` | Create a new string node |
| `coda_string_get(doc, n)` | Read value (`coda_str_t`) |
| `coda_string_set(doc, n, s, len)` | Write value |

### Block / map nodes (`CODA_NODE_BLOCK`)

| Function | Description |
|---|---|
| `coda_new_block(doc)` | Create new block node |
| `coda_map_len(doc, m)` | Entry count |
| `coda_map_key_at(doc, m, i)` | Key at index i |
| `coda_map_value_at(doc, m, i)` | Value node at index i |
| `coda_map_get(doc, m, key, klen)` | Lookup by key (0 if missing) |
| `coda_map_get_or_insert(doc, m, key, klen)` | Get or create empty string node |
| `coda_map_set(doc, m, key, klen, value)` | Insert or replace |
| `coda_map_remove(doc, m, key, klen)` | Remove key |

### Array nodes (`CODA_NODE_ARRAY`)

| Function | Description |
|---|---|
| `coda_new_array(doc)` | Create new array node |
| `coda_array_len(doc, a)` | Element count |
| `coda_array_get(doc, a, i)` | Get element at index |
| `coda_array_set(doc, a, i, value)` | Replace element |
| `coda_array_push(doc, a, value)` | Append element |
| `coda_array_remove(doc, a, i)` | Remove element |

### Plain table nodes (`CODA_NODE_TABLE`)

| Function | Description |
|---|---|
| `coda_new_table(doc)` | Create new table node |
| `coda_table_col_count(doc, t)` | Column count |
| `coda_table_col_name(doc, t, i)` | Column name at index |
| `coda_table_col_append(doc, t, name, nlen)` | Append column |
| `coda_table_row_count(doc, t)` | Row count |
| `coda_table_row_at(doc, t, i)` | Row node at index |
| `coda_table_row_append(doc, t, row)` | Append row |
| `coda_table_row_set(doc, t, i, row)` | Replace row |
| `coda_table_row_remove(doc, t, i)` | Remove row |

### Keyed table nodes (`CODA_NODE_KEYED_TABLE`)

| Function | Description |
|---|---|
| `coda_new_keyed_table(doc)` | Create new keyed table |
| `coda_keyed_table_col_count(doc, kt)` | Column count (excludes key column) |
| `coda_keyed_table_col_name(doc, kt, i)` | Column name at index |
| `coda_keyed_table_col_append(doc, kt, name, nlen)` | Append column |
| `coda_keyed_table_row_count(doc, kt)` | Row count |
| `coda_keyed_table_row_key_at(doc, kt, i)` | Key string at index |
| `coda_keyed_table_row_at(doc, kt, i)` | Row node at index |
| `coda_keyed_table_row_get(doc, kt, key, klen)` | Lookup row by key |
| `coda_keyed_table_row_set(doc, kt, key, klen, row)` | Insert/replace row |
| `coda_keyed_table_row_remove(doc, kt, key, klen)` | Remove row |

### Row nodes (`CODA_NODE_ROW`)

| Function | Description |
|---|---|
| `coda_new_row(doc)` | Create new row node |
| `coda_row_get(doc, row, col, clen)` | Get column value |
| `coda_row_set(doc, row, col, clen, val, vlen)` | Set column value |
| `coda_row_remove(doc, row, col, clen)` | Remove column |
| `coda_row_col_count(doc, row)` | Column count |
| `coda_row_col_name_at(doc, row, i)` | Column name at index |
| `coda_row_col_value_at(doc, row, i)` | Column value at index |
| `coda_row_comment_get/set` | Row-level comment |

### Error / parse codes

| Function | Description |
|---|---|
| `coda_parse_error_code_name(code)` | Human-readable name for a parse error code |
| `coda_error_clear(&err)` | Free the error message buffer |

Parse error codes: `CODA_PARSE_UNEXPECTED_TOKEN`, `UNEXPECTED_EOF`, `DUPLICATE_KEY`, `DUPLICATE_FIELD`, `RAGGED_ROW`, `INVALID_ESCAPE`, `UNTERMINATED_STRING`, `NESTED_BLOCK`, `CONTENT_AFTER_BRACE`, `KEY_IN_BLOCK`.
