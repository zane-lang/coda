# C FFI API — `ffi/coda_ffi.h`

`libcoda_ffi` exposes a stable C ABI so you can embed the Coda parser in any language with a C FFI (Python via ctypes, Ruby, Lua, Zig, etc.).

## Setup

Build the shared library:

```bash
just build        # libcoda_ffi.so / .dylib / .dll for the host platform
just cross-all    # cross-compile for all supported targets (output in dist/)
```

Then include the header and link against the library:

```c
#include "ffi/coda_ffi.h"
// link: -lcoda_ffi
```

---

## Types

### Opaque handles

| Type | Underlying type | Description |
|---|---|---|
| `coda_doc_t*` | opaque pointer | Document handle; free with `coda_doc_free()` |
| `coda_node_t` | `uint32_t` | Node handle; `0` is reserved as null / invalid |

### String types

```c
typedef struct coda_str {
    const char* ptr;   // pointer into document-internal storage (borrowed)
    size_t      len;   // length in bytes, excluding any null terminator
} coda_str_t;

typedef struct coda_owned_str {
    char*  ptr;   // heap-allocated, null-terminated for convenience
    size_t len;   // length in bytes, excluding the null terminator
} coda_owned_str_t;
```

> ⚠️ **`coda_str_t` lifetime**: a *borrowed* view into the document's internal storage. The pointer is valid only as long as (a) the `coda_doc_t` that owns it is alive **and** (b) no mutation has been made to the document since the view was obtained. Always `memcpy` the bytes before calling any mutating function or `coda_doc_free()`. Do not store `coda_str_t` across calls that modify the document.
>
> Returned by: `coda_string_get`, `coda_map_key_at`, `coda_table_col_name`, `coda_keyed_table_col_name`, `coda_keyed_table_row_key_at`, `coda_row_get`, `coda_row_col_name_at`, `coda_row_col_value_at`, `coda_node_comment_get`, `coda_node_header_comment_get`, `coda_row_comment_get`, `coda_parse_error_code_name`.

### `coda_error_t`

```c
typedef struct coda_error {
    uint32_t         code;     // coda_parse_error_code_t value
    uint32_t         line;     // 1-based line number
    uint32_t         col;      // 1-based column number
    size_t           offset;   // byte offset into source
    coda_owned_str_t message;  // owned formatted error string
} coda_error_t;
```

Free the embedded message buffer with `coda_error_clear(&err)`; the struct itself is caller-allocated.

### `coda_node_kind_t`

| Value | Integer | Description |
|---|---|---|
| `CODA_NODE_NULL` | 0 | Null / invalid node |
| `CODA_NODE_STRING` | 2 | Scalar string value |
| `CODA_NODE_BLOCK` | 3 | Key-value map (also the root node) |
| `CODA_NODE_ARRAY` | 4 | Ordered list of values |
| `CODA_NODE_TABLE` | 5 | Anonymous-row table (header + data rows) |
| `CODA_NODE_KEYED_TABLE` | 6 | Keyed-row table (`key col…` header + keyed rows) |
| `CODA_NODE_ROW` | 7 | A single row inside a `TABLE` or `KEYED_TABLE` |

### `coda_status_t`

| Value | Integer | Meaning |
|---|---|---|
| `CODA_OK` | 0 | Success |
| `CODA_ERR` | 1 | Generic error |
| `CODA_NOT_FOUND` | 2 | Key or index not found |
| `CODA_BAD_KIND` | 3 | Node has the wrong kind for the operation |
| `CODA_OUT_OF_RANGE` | 4 | Index out of range |

### `coda_parse_error_code_t`

| Value | Integer |
|---|---|
| `CODA_PARSE_UNEXPECTED_TOKEN` | 0 |
| `CODA_PARSE_UNEXPECTED_EOF` | 1 |
| `CODA_PARSE_DUPLICATE_KEY` | 2 |
| `CODA_PARSE_DUPLICATE_FIELD` | 3 |
| `CODA_PARSE_RAGGED_ROW` | 4 |
| `CODA_PARSE_INVALID_ESCAPE` | 5 |
| `CODA_PARSE_UNTERMINATED_STRING` | 6 |
| `CODA_PARSE_NESTED_BLOCK` | 7 |
| `CODA_PARSE_CONTENT_AFTER_BRACE` | 8 |
| `CODA_PARSE_KEY_IN_BLOCK` | 9 |

---

## Parsing

```c
coda_error_t err = {0};

// Parse from a byte buffer
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

// Free when done (also frees all node storage)
coda_doc_free(doc);
```

The `filename` and `err` parameters are optional — pass `NULL` to omit either.

---

## Reading values

```c
coda_node_t root = coda_doc_root(doc);  // CODA_NODE_BLOCK

// --- Scalar string ---
coda_node_t n    = coda_map_get(doc, root, "name", 4);
coda_str_t  name = coda_string_get(doc, n);
printf("%.*s\n", (int)name.len, name.ptr);

// --- Membership test ---
coda_node_t ver = coda_map_get(doc, root, "version", 7);
if (ver != 0) { /* key exists */ }

// --- Block (key-value map) ---
size_t nkeys = coda_map_len(doc, root);
for (size_t i = 0; i < nkeys; ++i) {
    coda_str_t  key = coda_map_key_at(doc, root, i);
    coda_node_t val = coda_map_value_at(doc, root, i);
    if (coda_node_kind(doc, val) == CODA_NODE_STRING) {
        coda_str_t s = coda_string_get(doc, val);
        printf("%.*s = %.*s\n",
               (int)key.len, key.ptr, (int)s.len, s.ptr);
    }
}

// --- Array (bare list) ---
coda_node_t arr  = coda_map_get(doc, root, "targets", 7);
size_t      alen = coda_array_len(doc, arr);
for (size_t i = 0; i < alen; ++i) {
    coda_str_t s = coda_string_get(doc, coda_array_get(doc, arr, i));
    printf("%.*s\n", (int)s.len, s.ptr);
}

// --- Plain table ---
coda_node_t t     = coda_map_get(doc, root, "releases", 8);
size_t      ncols = coda_table_col_count(doc, t);
size_t      nrows = coda_table_row_count(doc, t);
for (size_t r = 0; r < nrows; ++r) {
    coda_node_t row = coda_table_row_at(doc, t, r);
    coda_str_t  ver = coda_row_get(doc, row, "version", 7);
    coda_str_t  dat = coda_row_get(doc, row, "date", 4);
    printf("%.*s  %.*s\n",
           (int)ver.len, ver.ptr, (int)dat.len, dat.ptr);
}

// --- Keyed table ---
coda_node_t kt    = coda_map_get(doc, root, "deps", 4);
size_t      nkt   = coda_keyed_table_row_count(doc, kt);
for (size_t r = 0; r < nkt; ++r) {
    coda_str_t  key = coda_keyed_table_row_key_at(doc, kt, r);
    coda_node_t row = coda_keyed_table_row_at(doc, kt, r);
    coda_str_t  lnk = coda_row_get(doc, row, "link", 4);
    printf("%.*s → %.*s\n", (int)key.len, key.ptr, (int)lnk.len, lnk.ptr);
}

// Direct row lookup by key
coda_node_t plot = coda_keyed_table_row_get(doc, kt, "plot", 4);
```

---

## Creating and modifying

```c
coda_doc_t* doc  = coda_doc_new();
coda_node_t root = coda_doc_root(doc);

// --- Scalar ---
coda_node_t n = coda_new_string(doc, "myproject", 9);
coda_map_set(doc, root, "name", 4, n);

// Shorter: get-or-insert then set
coda_node_t v = coda_map_get_or_insert(doc, root, "version", 7);
coda_string_set(doc, v, "1.0.0", 5);

// --- Block ---
coda_node_t blk = coda_new_block(doc);
coda_node_t dbg = coda_new_string(doc, "false", 5);
coda_map_set(doc, blk, "debug", 5, dbg);
coda_map_set(doc, root, "compiler", 8, blk);

// --- Array ---
coda_node_t arr = coda_new_array(doc);
coda_array_push(doc, arr, coda_new_string(doc, "x86_64-linux",  12));
coda_array_push(doc, arr, coda_new_string(doc, "x86_64-windows", 14));
coda_array_push(doc, arr, coda_new_string(doc, "aarch64-macos", 13));
coda_map_set(doc, root, "targets", 7, arr);

// --- Plain table ---
coda_node_t tbl = coda_new_table(doc);
coda_table_col_append(doc, tbl, "version", 7);
coda_table_col_append(doc, tbl, "date",    4);
coda_node_t row = coda_new_row(doc);
coda_row_set(doc, row, "version", 7, "1.0.0",      5);
coda_row_set(doc, row, "date",    4, "2025-01-01", 10);
coda_table_row_append(doc, tbl, row);
coda_map_set(doc, root, "releases", 8, tbl);

// --- Keyed table ---
coda_node_t kt = coda_new_keyed_table(doc);
coda_keyed_table_col_append(doc, kt, "link",    4);
coda_keyed_table_col_append(doc, kt, "version", 7);
coda_node_t plot_row = coda_new_row(doc);
coda_row_set(doc, plot_row, "link",    4, "github.com/zane-lang/plot", 25);
coda_row_set(doc, plot_row, "version", 7, "4.0.3", 5);
coda_keyed_table_row_set(doc, kt, "plot", 4, plot_row);
coda_map_set(doc, root, "deps", 4, kt);

// Modify an existing string value
coda_node_t existing = coda_map_get(doc, root, "name", 4);
coda_string_set(doc, existing, "renamed", 7);

// Remove a key
coda_map_remove(doc, root, "deprecated", 10);
```

---

## Sorting

```c
// Sort the entire document (scalars first, then containers; alphabetical within each group)
coda_doc_order(doc);

// Sort by weight table (higher weight → closer to the top)
const char*  keys[]    = { "name", "version" };
const float  weights[] = { 100.0f, 90.0f };
coda_doc_order_weighted(doc, keys, weights, 2);

// Sort a sub-tree only
coda_node_t blk = coda_map_get(doc, root, "compiler", 8);
coda_node_order(doc, blk);
coda_node_order_weighted(doc, blk, keys, weights, 2);
```

> **Warning**: after any `order` call, all previously obtained `coda_node_t` handles become **invalid**. Re-acquire them via `coda_doc_root()` and the traversal functions.

---

## Serialisation

```c
// Serialise the whole document
coda_owned_str_t out = coda_doc_serialize(doc, "\t", 1, NULL);
printf("%.*s", (int)out.len, out.ptr);
coda_owned_str_free(out);

// Serialise with 2-space indentation
coda_owned_str_t out = coda_doc_serialize(doc, "  ", 2, NULL);
coda_owned_str_free(out);

// Serialise a single node sub-tree
coda_node_t blk = coda_map_get(doc, root, "compiler", 8);
coda_owned_str_t out = coda_node_serialize(doc, blk, "\t", 1, NULL);
coda_owned_str_free(out);
```

Pass `NULL` for the `err` parameter to ignore serialisation errors.

---

## Error handling

Parse functions return `NULL` (or `0` for node handles) on failure and optionally fill a caller-provided `coda_error_t`:

```c
coda_error_t err = {0};
coda_doc_t*  doc = coda_doc_parse(src, len, "file.coda", &err);
if (!doc) {
    // err.line / err.col are 1-based
    fprintf(stderr, "%u:%u: %.*s\n",
            err.line, err.col,
            (int)err.message.len, err.message.ptr);
    // get the human-readable error code name
    coda_str_t codeName = coda_parse_error_code_name(err.code);
    fprintf(stderr, "code: %.*s\n", (int)codeName.len, codeName.ptr);
    coda_error_clear(&err);  // free the message buffer
    return 1;
}
```

`coda_error_t` fields:

| Field | Type | Description |
|---|---|---|
| `code` | `uint32_t` | `coda_parse_error_code_t` value |
| `line` | `uint32_t` | 1-based line number |
| `col` | `uint32_t` | 1-based column number |
| `offset` | `size_t` | Byte offset into the source |
| `message` | `coda_owned_str_t` | Formatted error string — free with `coda_error_clear()` |

Parse error codes: `CODA_PARSE_UNEXPECTED_TOKEN`, `CODA_PARSE_UNEXPECTED_EOF`, `CODA_PARSE_DUPLICATE_KEY`, `CODA_PARSE_DUPLICATE_FIELD`, `CODA_PARSE_RAGGED_ROW`, `CODA_PARSE_INVALID_ESCAPE`, `CODA_PARSE_UNTERMINATED_STRING`, `CODA_PARSE_NESTED_BLOCK`, `CODA_PARSE_CONTENT_AFTER_BRACE`, `CODA_PARSE_KEY_IN_BLOCK`.

---

## Comments

```c
// Pre-node comment (# lines above a key/row)
coda_str_t c = coda_node_comment_get(doc, node);
coda_node_comment_set(doc, node, "dependency table", 16);

// Header comment (# lines before the first element in an array/table)
coda_str_t h = coda_node_header_comment_get(doc, node);
coda_node_header_comment_set(doc, node, "optional", 8);

// Row-level comment (inside a table)
coda_str_t rc = coda_row_comment_get(doc, row);
coda_row_comment_set(doc, row, "main dep", 8);
```

Comments are stored and serialised without the leading `#` — the serialiser adds it automatically.

---

## Memory management

| Resource | Free function |
|---|---|
| `coda_doc_t*` | `coda_doc_free(doc)` |
| `coda_owned_str_t` | `coda_owned_str_free(s)` |
| `coda_error_t` message buffer | `coda_error_clear(&err)` (struct itself is caller-allocated) |

Do **not** mix these — calling the wrong free on a pointer is undefined behaviour and will crash on Windows when the library and caller use different CRT instances.

---

## Full function reference

### Document

| Function | Description |
|---|---|
| `coda_doc_new()` | Create an empty document |
| `coda_doc_parse(src, len, file?, err?)` | Parse UTF-8 bytes |
| `coda_doc_parse_file(path, err?)` | Parse from a file path |
| `coda_doc_parse_fp(fp, file?, err?)` | Parse from an open `FILE*` |
| `coda_doc_free(doc)` | Free the document and all its nodes |
| `coda_doc_root(doc)` | Get the root `CODA_NODE_BLOCK` handle |
| `coda_doc_serialize(doc, indent, ilen, err?)` | Serialise to `coda_owned_str_t` |
| `coda_doc_order(doc)` | Sort all keys |
| `coda_doc_order_weighted(doc, keys, weights, count)` | Sort by weight |
| `coda_ffi_abi_version()` | ABI version integer |

### Node inspection

| Function | Description |
|---|---|
| `coda_node_kind(doc, n)` | Returns `coda_node_kind_t` |
| `coda_node_is_container(doc, n)` | `1` if `BLOCK`, `ARRAY`, `TABLE`, or `KEYED_TABLE` |
| `coda_node_comment_get(doc, n)` | Pre-node comment (`coda_str_t`) |
| `coda_node_comment_set(doc, n, s, len)` | Set pre-node comment; returns `coda_status_t` |
| `coda_node_header_comment_get(doc, n)` | Header comment (`coda_str_t`) |
| `coda_node_header_comment_set(doc, n, s, len)` | Set header comment; returns `coda_status_t` |
| `coda_node_serialize(doc, n, indent, ilen, err?)` | Serialise sub-tree to `coda_owned_str_t` |
| `coda_node_order(doc, n)` | Sort sub-tree alphabetically |
| `coda_node_order_weighted(doc, n, keys, weights, count)` | Sort sub-tree by weight |

### Node creation

| Function | Description |
|---|---|
| `coda_new_string(doc, s, len)` | Create a new string node |
| `coda_new_block(doc)` | Create a new block node |
| `coda_new_array(doc)` | Create a new array node |
| `coda_new_table(doc)` | Create a new plain table node |
| `coda_new_keyed_table(doc)` | Create a new keyed table node |
| `coda_new_row(doc)` | Create a new row node |

### String nodes (`CODA_NODE_STRING`)

| Function | Description |
|---|---|
| `coda_string_get(doc, n)` | Read value (`coda_str_t`) |
| `coda_string_set(doc, n, s, len)` | Write value |

### Block / map nodes (`CODA_NODE_BLOCK`)

| Function | Description |
|---|---|
| `coda_map_len(doc, m)` | Entry count |
| `coda_map_key_at(doc, m, i)` | Key at index `i` |
| `coda_map_value_at(doc, m, i)` | Value node at index `i` |
| `coda_map_get(doc, m, key, klen)` | Lookup by key; returns `0` if absent |
| `coda_map_get_or_insert(doc, m, key, klen)` | Lookup or create an empty string node |
| `coda_map_set(doc, m, key, klen, value)` | Insert or replace |
| `coda_map_remove(doc, m, key, klen)` | Remove key |

### Array nodes (`CODA_NODE_ARRAY`)

| Function | Description |
|---|---|
| `coda_array_len(doc, a)` | Element count |
| `coda_array_get(doc, a, i)` | Get element at index |
| `coda_array_set(doc, a, i, value)` | Replace element |
| `coda_array_push(doc, a, value)` | Append element |
| `coda_array_remove(doc, a, i)` | Remove element |

### Plain table nodes (`CODA_NODE_TABLE`)

| Function | Description |
|---|---|
| `coda_table_col_count(doc, t)` | Column count |
| `coda_table_col_name(doc, t, i)` | Column name at index `i` |
| `coda_table_col_append(doc, t, name, nlen)` | Append a column name |
| `coda_table_row_count(doc, t)` | Row count |
| `coda_table_row_at(doc, t, i)` | Row node at index `i` |
| `coda_table_row_append(doc, t, row)` | Append a row |
| `coda_table_row_set(doc, t, i, row)` | Replace row at index `i` |
| `coda_table_row_remove(doc, t, i)` | Remove row at index `i` |

### Keyed table nodes (`CODA_NODE_KEYED_TABLE`)

| Function | Description |
|---|---|
| `coda_keyed_table_col_count(doc, kt)` | Column count (excludes the key column) |
| `coda_keyed_table_col_name(doc, kt, i)` | Column name at index `i` |
| `coda_keyed_table_col_append(doc, kt, name, nlen)` | Append a column name |
| `coda_keyed_table_row_count(doc, kt)` | Row count |
| `coda_keyed_table_row_key_at(doc, kt, i)` | Key string at index `i` |
| `coda_keyed_table_row_at(doc, kt, i)` | Row node at index `i` |
| `coda_keyed_table_row_get(doc, kt, key, klen)` | Lookup row by key; returns `0` if absent |
| `coda_keyed_table_row_set(doc, kt, key, klen, row)` | Insert or replace row |
| `coda_keyed_table_row_remove(doc, kt, key, klen)` | Remove row |

### Row nodes (`CODA_NODE_ROW`)

| Function | Description |
|---|---|
| `coda_row_get(doc, row, col, clen)` | Get column value; returns empty string if absent |
| `coda_row_set(doc, row, col, clen, val, vlen)` | Set column value |
| `coda_row_remove(doc, row, col, clen)` | Remove column; returns `CODA_NOT_FOUND` if absent |
| `coda_row_col_count(doc, row)` | Column count |
| `coda_row_col_name_at(doc, row, i)` | Column name at index `i` |
| `coda_row_col_value_at(doc, row, i)` | Column value at index `i` |
| `coda_row_comment_get(doc, row)` | Row-level comment |
| `coda_row_comment_set(doc, row, s, len)` | Set row-level comment |

### Memory management

| Function | Description |
|---|---|
| `coda_doc_free(doc)` | Free a `coda_doc_t*` and all its nodes |
| `coda_owned_str_free(s)` | Free a `coda_owned_str_t` returned by the library |
| `coda_error_clear(&err)` | Free the message buffer inside a `coda_error_t` |
| `coda_free(p)` | Generic free for future API functions — do **not** use on `coda_doc_t*` or `coda_owned_str_t` |

### Utilities

| Function | Description |
|---|---|
| `coda_ffi_abi_version()` | Returns the ABI version integer |
| `coda_parse_error_code_name(code)` | Human-readable name for a `coda_parse_error_code_t` value |
