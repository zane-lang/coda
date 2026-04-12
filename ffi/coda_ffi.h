#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// ─── Export / ABI helpers ────────────────────────────────────────────────────

#if defined(_WIN32)
#define CODA_FFI_EXPORT __declspec(dllexport)
#else
#define CODA_FFI_EXPORT __attribute__((visibility("default")))
#endif

// ─── Basic types ─────────────────────────────────────────────────────────────

typedef struct coda_doc coda_doc_t;

// 0 is reserved as "null/invalid".
typedef uint32_t coda_node_t;

typedef struct coda_str {
	const char* ptr;
	size_t      len;
} coda_str_t;

typedef struct coda_owned_str {
	char*  ptr;  // null-terminated for convenience
	size_t len;  // excludes the null terminator
} coda_owned_str_t;

typedef enum coda_node_kind {
	CODA_NODE_NULL         = 0,
	CODA_NODE_FILE         = 1,  // top-level block (serializes without braces)
	CODA_NODE_STRING       = 2,
	CODA_NODE_BLOCK        = 3,  // { key value ... }
	CODA_NODE_ARRAY        = 4,  // [ ... ] — homogeneous or nested values
	CODA_NODE_TABLE        = 5,  // anonymous-row table: header row + data rows
	CODA_NODE_KEYED_TABLE  = 6,  // keyed-row table: "key col..." header + keyed rows
	CODA_NODE_ROW          = 7,  // a single row inside TABLE or KEYED_TABLE
} coda_node_kind_t;

typedef enum coda_status {
	CODA_OK            = 0,
	CODA_ERR           = 1,
	CODA_NOT_FOUND     = 2,
	CODA_BAD_KIND      = 3,
	CODA_OUT_OF_RANGE  = 4,
} coda_status_t;

typedef struct coda_error {
	uint32_t         code;     // maps from coda::ParseErrorCode when available
	uint32_t         line;     // 1-based
	uint32_t         col;      // 1-based
	size_t           offset;   // byte offset in source
	coda_owned_str_t message;  // owned formatted error string
} coda_error_t;

typedef enum coda_parse_error_code {
	CODA_PARSE_UNEXPECTED_TOKEN    = 0,
	CODA_PARSE_UNEXPECTED_EOF      = 1,
	CODA_PARSE_DUPLICATE_KEY       = 2,
	CODA_PARSE_DUPLICATE_FIELD     = 3,
	CODA_PARSE_RAGGED_ROW          = 4,
	CODA_PARSE_INVALID_ESCAPE      = 5,
	CODA_PARSE_UNTERMINATED_STRING = 6,
	CODA_PARSE_NESTED_BLOCK        = 7,
	CODA_PARSE_CONTENT_AFTER_BRACE = 8,
	CODA_PARSE_KEY_IN_BLOCK        = 9,
} coda_parse_error_code_t;

CODA_FFI_EXPORT coda_str_t coda_parse_error_code_name(uint32_t code);

// ─── Memory management ───────────────────────────────────────────────────────

CODA_FFI_EXPORT void coda_free(void* p);

// Frees the message buffer inside err (does NOT free err itself).
CODA_FFI_EXPORT void coda_error_clear(coda_error_t* err);

CODA_FFI_EXPORT void coda_owned_str_free(coda_owned_str_t s);

// ─── ABI versioning ──────────────────────────────────────────────────────────

CODA_FFI_EXPORT uint32_t coda_ffi_abi_version(void);

// ─── Doc lifecycle ───────────────────────────────────────────────────────────

// Create an empty document with an empty root FILE node.
CODA_FFI_EXPORT coda_doc_t* coda_doc_new(void);
CODA_FFI_EXPORT void        coda_doc_free(coda_doc_t* doc);

// Parse UTF-8 text. Returns NULL on failure and fills *err (if non-NULL).
CODA_FFI_EXPORT coda_doc_t* coda_doc_parse(
	const char*   src,
	size_t        len,
	const char*   filename,  // optional, may be NULL
	coda_error_t* err        // optional, may be NULL
);

CODA_FFI_EXPORT coda_doc_t* coda_doc_parse_file(
	const char*   path,
	coda_error_t* err
);

CODA_FFI_EXPORT coda_doc_t* coda_doc_parse_fp(
	FILE*         fp,
	const char*   filename,
	coda_error_t* err
);

// Serialize the document to Coda text.
// indent_unit: e.g. "\t" or "  " (pass NULL for default "\t")
// Returns {NULL,0} on error and fills *err.
CODA_FFI_EXPORT coda_owned_str_t coda_doc_serialize(
	const coda_doc_t* doc,
	const char*       indent_unit,
	size_t            indent_unit_len,
	coda_error_t*     err
);

// Reorder all keys using default ordering (scalars first, then containers;
// alphabetical within each group). Array and table row order is preserved.
//
// WARNING: All previously obtained coda_node_t handles become INVALID after
//          this call. Re-acquire handles via coda_doc_root() and the
//          traversal APIs.
CODA_FFI_EXPORT void coda_doc_order(coda_doc_t* doc);

// Reorder all keys by weight (higher weight → closer to top).
// Keys absent from the table get weight 0.0; equal weights sort alphabetically.
//
// WARNING: Same handle invalidation caveat as coda_doc_order().
CODA_FFI_EXPORT void coda_doc_order_weighted(
	coda_doc_t*  doc,
	const char** keys,
	const float* weights,
	size_t       count
);

// Returns the root FILE node of the document.
CODA_FFI_EXPORT coda_node_t coda_doc_root(const coda_doc_t* doc);

// ─── Node inspection ─────────────────────────────────────────────────────────

CODA_FFI_EXPORT coda_node_kind_t coda_node_kind(
	const coda_doc_t* doc, coda_node_t n
);

// Returns 1 if the node is a container (BLOCK, ARRAY, TABLE, KEYED_TABLE),
// 0 otherwise (STRING, ROW, FILE, NULL).
CODA_FFI_EXPORT int coda_node_is_container(const coda_doc_t* doc, coda_node_t n);

// Pre-node comment (the # lines above a key/row).
CODA_FFI_EXPORT coda_str_t coda_node_comment_get(
	const coda_doc_t* doc, coda_node_t n
);
CODA_FFI_EXPORT coda_status_t coda_node_comment_set(
	coda_doc_t* doc, coda_node_t n,
	const char* s, size_t len
);

// Header comment: the # lines before the header row inside a TABLE or
// KEYED_TABLE, or before the first item inside an ARRAY.
// Returns empty string if not applicable or absent.
CODA_FFI_EXPORT coda_str_t coda_node_header_comment_get(
	const coda_doc_t* doc, coda_node_t n
);
CODA_FFI_EXPORT coda_status_t coda_node_header_comment_set(
	coda_doc_t* doc, coda_node_t n,
	const char* s, size_t len
);

// ─── String nodes ────────────────────────────────────────────────────────────

CODA_FFI_EXPORT coda_str_t coda_string_get(
	const coda_doc_t* doc, coda_node_t n
);
CODA_FFI_EXPORT coda_status_t coda_string_set(
	coda_doc_t* doc, coda_node_t n,
	const char* s, size_t len
);

// ─── Array nodes ─────────────────────────────────────────────────────────────

// Valid for: CODA_NODE_ARRAY
// For TABLE, use coda_table_*. For KEYED_TABLE, use coda_keyed_table_*.

CODA_FFI_EXPORT size_t      coda_array_len(const coda_doc_t* doc, coda_node_t a);
CODA_FFI_EXPORT coda_node_t coda_array_get(const coda_doc_t* doc, coda_node_t a, size_t idx);

CODA_FFI_EXPORT coda_status_t coda_array_set(
	coda_doc_t* doc, coda_node_t a, size_t idx, coda_node_t value
);
CODA_FFI_EXPORT coda_status_t coda_array_push(
	coda_doc_t* doc, coda_node_t a, coda_node_t value
);
CODA_FFI_EXPORT coda_status_t coda_array_remove(
	coda_doc_t* doc, coda_node_t a, size_t idx
);

// ─── Map-like nodes (FILE / BLOCK) ───────────────────────────────────────────

// Valid for: CODA_NODE_FILE, CODA_NODE_BLOCK

CODA_FFI_EXPORT size_t      coda_map_len(const coda_doc_t* doc, coda_node_t m);
CODA_FFI_EXPORT coda_str_t  coda_map_key_at(const coda_doc_t* doc, coda_node_t m, size_t idx);
CODA_FFI_EXPORT coda_node_t coda_map_value_at(const coda_doc_t* doc, coda_node_t m, size_t idx);

// Returns 0 if key is not found.
CODA_FFI_EXPORT coda_node_t coda_map_get(
	const coda_doc_t* doc, coda_node_t m,
	const char* key, size_t key_len
);

// Like coda_map_get but inserts an empty STRING node if the key is absent.
CODA_FFI_EXPORT coda_node_t coda_map_get_or_insert(
	coda_doc_t* doc, coda_node_t m,
	const char* key, size_t key_len
);

CODA_FFI_EXPORT coda_status_t coda_map_set(
	coda_doc_t* doc, coda_node_t m,
	const char* key, size_t key_len,
	coda_node_t value
);

CODA_FFI_EXPORT coda_status_t coda_map_remove(
	coda_doc_t* doc, coda_node_t m,
	const char* key, size_t key_len
);

// ─── Plain table nodes (TABLE) ───────────────────────────────────────────────
//
// A TABLE has:
//   - An ordered list of column names (the header row).
//   - An ordered list of ROW nodes, each of which is a map of column→string.
//
// Iteration pattern:
//   size_t ncols = coda_table_col_count(doc, t);
//   for (size_t c = 0; c < ncols; ++c)  coda_table_col_name(doc, t, c);
//   size_t nrows = coda_table_row_count(doc, t);
//   for (size_t r = 0; r < nrows; ++r) {
//       coda_node_t row = coda_table_row_at(doc, t, r);
//       coda_str_t  val = coda_row_get(doc, row, "col", 3);
//   }

CODA_FFI_EXPORT size_t     coda_table_col_count(const coda_doc_t* doc, coda_node_t t);
CODA_FFI_EXPORT coda_str_t coda_table_col_name(const coda_doc_t* doc, coda_node_t t, size_t col_idx);

// Appends a new column name.  Existing rows are NOT automatically extended.
CODA_FFI_EXPORT coda_status_t coda_table_col_append(
	coda_doc_t* doc, coda_node_t t,
	const char* name, size_t name_len
);

CODA_FFI_EXPORT size_t      coda_table_row_count(const coda_doc_t* doc, coda_node_t t);
CODA_FFI_EXPORT coda_node_t coda_table_row_at(const coda_doc_t* doc, coda_node_t t, size_t row_idx);

// Appends an existing ROW node to the table and returns CODA_OK.
// The node must have been created with coda_new_row().
CODA_FFI_EXPORT coda_status_t coda_table_row_append(
	coda_doc_t* doc, coda_node_t t, coda_node_t row
);

CODA_FFI_EXPORT coda_status_t coda_table_row_set(
	coda_doc_t* doc, coda_node_t t, size_t row_idx, coda_node_t row
);

CODA_FFI_EXPORT coda_status_t coda_table_row_remove(
	coda_doc_t* doc, coda_node_t t, size_t row_idx
);

// ─── Keyed table nodes (KEYED_TABLE) ─────────────────────────────────────────
//
// A KEYED_TABLE has:
//   - An ordered list of column names (NOT including the key column itself).
//   - An ordered map of key→ROW, where each ROW maps column→string.
//
// Iteration pattern:
//   size_t ncols = coda_keyed_table_col_count(doc, kt);
//   for (size_t c = 0; c < ncols; ++c)  coda_keyed_table_col_name(doc, kt, c);
//   size_t nrows = coda_keyed_table_row_count(doc, kt);
//   for (size_t r = 0; r < nrows; ++r) {
//       coda_str_t  key = coda_keyed_table_row_key_at(doc, kt, r);
//       coda_node_t row = coda_keyed_table_row_at(doc, kt, r);
//       coda_str_t  val = coda_row_get(doc, row, "col", 3);
//   }

CODA_FFI_EXPORT size_t     coda_keyed_table_col_count(const coda_doc_t* doc, coda_node_t kt);
CODA_FFI_EXPORT coda_str_t coda_keyed_table_col_name(const coda_doc_t* doc, coda_node_t kt, size_t col_idx);

CODA_FFI_EXPORT coda_status_t coda_keyed_table_col_append(
	coda_doc_t* doc, coda_node_t kt,
	const char* name, size_t name_len
);

CODA_FFI_EXPORT size_t      coda_keyed_table_row_count(const coda_doc_t* doc, coda_node_t kt);
CODA_FFI_EXPORT coda_str_t  coda_keyed_table_row_key_at(const coda_doc_t* doc, coda_node_t kt, size_t row_idx);
CODA_FFI_EXPORT coda_node_t coda_keyed_table_row_at(const coda_doc_t* doc, coda_node_t kt, size_t row_idx);

// Returns 0 if key not found.
CODA_FFI_EXPORT coda_node_t coda_keyed_table_row_get(
	const coda_doc_t* doc, coda_node_t kt,
	const char* key, size_t key_len
);

// Inserts or replaces the row for the given key.
CODA_FFI_EXPORT coda_status_t coda_keyed_table_row_set(
	coda_doc_t* doc, coda_node_t kt,
	const char* key, size_t key_len,
	coda_node_t row
);

CODA_FFI_EXPORT coda_status_t coda_keyed_table_row_remove(
	coda_doc_t* doc, coda_node_t kt,
	const char* key, size_t key_len
);

// ─── Row nodes ───────────────────────────────────────────────────────────────
//
// A ROW is a flat map of column-name → string value.
// Both TABLE rows and KEYED_TABLE rows use this API.

// Returns empty string if column is not present.
CODA_FFI_EXPORT coda_str_t coda_row_get(
	const coda_doc_t* doc, coda_node_t row,
	const char* col, size_t col_len
);

// Sets (or inserts) a column value in the row.
CODA_FFI_EXPORT coda_status_t coda_row_set(
	coda_doc_t* doc, coda_node_t row,
	const char* col, size_t col_len,
	const char* val, size_t val_len
);

// Removes a column from the row; returns CODA_NOT_FOUND if absent.
CODA_FFI_EXPORT coda_status_t coda_row_remove(
	coda_doc_t* doc, coda_node_t row,
	const char* col, size_t col_len
);

// Iterate over a row's columns in insertion order.
CODA_FFI_EXPORT size_t     coda_row_col_count(const coda_doc_t* doc, coda_node_t row);
CODA_FFI_EXPORT coda_str_t coda_row_col_name_at(const coda_doc_t* doc, coda_node_t row, size_t idx);
CODA_FFI_EXPORT coda_str_t coda_row_col_value_at(const coda_doc_t* doc, coda_node_t row, size_t idx);

// Row-level comment (the # lines above this row inside the table).
CODA_FFI_EXPORT coda_str_t coda_row_comment_get(
	const coda_doc_t* doc, coda_node_t row
);
CODA_FFI_EXPORT coda_status_t coda_row_comment_set(
	coda_doc_t* doc, coda_node_t row,
	const char* s, size_t len
);

// Serialize a single node to Coda text.
// For FILE nodes this is the same as coda_doc_serialize().
// For BLOCK nodes the output is wrapped in { }.
// indent_unit: e.g. "\t" or "  " (pass NULL for default "\t")
// Returns {NULL,0} on error and fills *err.
CODA_FFI_EXPORT coda_owned_str_t coda_node_serialize(
	const coda_doc_t* doc,
	coda_node_t       n,
	const char*       indent_unit,
	size_t            indent_unit_len,
	coda_error_t*     err
);

// Reorder a sub-tree rooted at n using default ordering.
// Has no effect on non-container nodes.
//
// WARNING: All previously obtained coda_node_t handles become INVALID after
//          this call. Re-acquire handles via traversal APIs.
CODA_FFI_EXPORT void coda_node_order(coda_doc_t* doc, coda_node_t n);

// Reorder a sub-tree rooted at n by weight.
//
// WARNING: Same handle invalidation caveat as coda_node_order().
CODA_FFI_EXPORT void coda_node_order_weighted(
	coda_doc_t*  doc,
	coda_node_t  n,
	const char** keys,
	const float* weights,
	size_t       count
);

// ─── Node creation ───────────────────────────────────────────────────────────

CODA_FFI_EXPORT coda_node_t coda_new_string(
	coda_doc_t* doc, const char* s, size_t len
);
CODA_FFI_EXPORT coda_node_t coda_new_block(coda_doc_t* doc);
CODA_FFI_EXPORT coda_node_t coda_new_array(coda_doc_t* doc);
CODA_FFI_EXPORT coda_node_t coda_new_table(coda_doc_t* doc);
CODA_FFI_EXPORT coda_node_t coda_new_keyed_table(coda_doc_t* doc);
CODA_FFI_EXPORT coda_node_t coda_new_row(coda_doc_t* doc);

#ifdef __cplusplus
} // extern "C"
#endif
