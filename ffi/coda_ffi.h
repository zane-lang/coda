#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
#define CODA_FFI_EXPORT __declspec(dllexport)
#else
#define CODA_FFI_EXPORT __attribute__((visibility("default")))
#endif

typedef struct coda_doc coda_doc_t;

// Opaque, document-scoped node handle. Zero is always null/invalid.
//
// Handles remain stable across ordering operations. They become invalid when
// their node is removed/replaced or when the owning document is freed. A stale
// handle never aliases a later node, even when the implementation recycles its
// internal arena slot.
typedef uint32_t coda_node_t;

// Borrowed string view into document storage. Copy it before mutating/freeing
// the document. Any document mutation may invalidate previously returned views.
typedef struct coda_str {
	const char* ptr;
	size_t      len;
} coda_str_t;

typedef struct coda_owned_str {
	char*  ptr;
	size_t len;
} coda_owned_str_t;

typedef enum coda_node_kind {
	CODA_NODE_NULL         = 0,
	CODA_NODE_STRING       = 2,
	CODA_NODE_BLOCK        = 3,
	CODA_NODE_ARRAY        = 4,
	CODA_NODE_TABLE        = 5,
	CODA_NODE_KEYED_TABLE  = 6,
	CODA_NODE_ROW          = 7,
} coda_node_kind_t;

typedef enum coda_status {
	CODA_OK            = 0,
	CODA_ERR           = 1,
	CODA_NOT_FOUND     = 2,
	CODA_BAD_KIND      = 3,
	CODA_OUT_OF_RANGE  = 4,
} coda_status_t;

typedef struct coda_error {
	uint32_t         code;
	uint32_t         line;
	uint32_t         col;
	size_t           offset;
	coda_owned_str_t message;
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

// Allocation families must be released by their matching functions.
CODA_FFI_EXPORT void coda_free(void* p);
CODA_FFI_EXPORT void coda_error_clear(coda_error_t* err);
CODA_FFI_EXPORT void coda_owned_str_free(coda_owned_str_t s);
CODA_FFI_EXPORT uint32_t coda_ffi_abi_version(void);

// Document lifecycle.
CODA_FFI_EXPORT coda_doc_t* coda_doc_new(void);
CODA_FFI_EXPORT void        coda_doc_free(coda_doc_t* doc);
CODA_FFI_EXPORT coda_doc_t* coda_doc_parse(
	const char* src, size_t len, const char* filename, coda_error_t* err);
CODA_FFI_EXPORT coda_doc_t* coda_doc_parse_file(
	const char* path, coda_error_t* err);
CODA_FFI_EXPORT coda_doc_t* coda_doc_parse_fp(
	FILE* fp, const char* filename, coda_error_t* err);

CODA_FFI_EXPORT coda_owned_str_t coda_doc_serialize(
	const coda_doc_t* doc,
	const char* indent_unit,
	size_t indent_unit_len,
	coda_error_t* err);

// Ordering is in-place. Node handles remain valid; index-based iteration order
// changes and must be read again. Arrays and plain-table rows keep their order;
// keyed-table rows are ordered by key (or by the supplied key weights).
CODA_FFI_EXPORT void coda_doc_order(coda_doc_t* doc);
CODA_FFI_EXPORT void coda_doc_order_weighted(
	coda_doc_t* doc,
	const char** keys,
	const float* weights,
	size_t count);
CODA_FFI_EXPORT coda_node_t coda_doc_root(const coda_doc_t* doc);

// General node inspection and comments.
CODA_FFI_EXPORT coda_node_kind_t coda_node_kind(
	const coda_doc_t* doc, coda_node_t n);
CODA_FFI_EXPORT int coda_node_is_container(
	const coda_doc_t* doc, coda_node_t n);
CODA_FFI_EXPORT coda_str_t coda_node_comment_get(
	const coda_doc_t* doc, coda_node_t n);
CODA_FFI_EXPORT coda_status_t coda_node_comment_set(
	coda_doc_t* doc, coda_node_t n, const char* s, size_t len);
CODA_FFI_EXPORT coda_str_t coda_node_header_comment_get(
	const coda_doc_t* doc, coda_node_t n);
CODA_FFI_EXPORT coda_status_t coda_node_header_comment_set(
	coda_doc_t* doc, coda_node_t n, const char* s, size_t len);

// String nodes.
CODA_FFI_EXPORT coda_str_t coda_string_get(
	const coda_doc_t* doc, coda_node_t n);
CODA_FFI_EXPORT coda_status_t coda_string_set(
	coda_doc_t* doc, coda_node_t n, const char* s, size_t len);

// Ownership rule for all container attachment APIs below:
// - the value/row must have been created in the same document;
// - it must currently be detached (except replacing a slot with itself);
// - attaching transfers it to exactly one parent;
// - cycles, cross-document handles and multiple parents return CODA_ERR.
// Removal/replacement recursively destroys the old subtree and invalidates all
// handles to it.

// Arrays.
CODA_FFI_EXPORT size_t coda_array_len(
	const coda_doc_t* doc, coda_node_t a);
CODA_FFI_EXPORT coda_node_t coda_array_get(
	const coda_doc_t* doc, coda_node_t a, size_t idx);
CODA_FFI_EXPORT coda_status_t coda_array_set(
	coda_doc_t* doc, coda_node_t a, size_t idx, coda_node_t value);
CODA_FFI_EXPORT coda_status_t coda_array_push(
	coda_doc_t* doc, coda_node_t a, coda_node_t value);
CODA_FFI_EXPORT coda_status_t coda_array_remove(
	coda_doc_t* doc, coda_node_t a, size_t idx);

// Blocks/maps.
CODA_FFI_EXPORT size_t coda_map_len(
	const coda_doc_t* doc, coda_node_t m);
CODA_FFI_EXPORT coda_str_t coda_map_key_at(
	const coda_doc_t* doc, coda_node_t m, size_t idx);
CODA_FFI_EXPORT coda_node_t coda_map_value_at(
	const coda_doc_t* doc, coda_node_t m, size_t idx);
CODA_FFI_EXPORT coda_node_t coda_map_get(
	const coda_doc_t* doc, coda_node_t m,
	const char* key, size_t key_len);
CODA_FFI_EXPORT coda_node_t coda_map_get_or_insert(
	coda_doc_t* doc, coda_node_t m,
	const char* key, size_t key_len);
CODA_FFI_EXPORT coda_status_t coda_map_set(
	coda_doc_t* doc, coda_node_t m,
	const char* key, size_t key_len,
	coda_node_t value);
CODA_FFI_EXPORT coda_status_t coda_map_remove(
	coda_doc_t* doc, coda_node_t m,
	const char* key, size_t key_len);

// Plain tables. Column declaration order is preserved. Duplicate columns are
// rejected. Columns may only be appended before the first row is attached.
CODA_FFI_EXPORT size_t coda_table_col_count(
	const coda_doc_t* doc, coda_node_t t);
CODA_FFI_EXPORT coda_str_t coda_table_col_name(
	const coda_doc_t* doc, coda_node_t t, size_t col_idx);
CODA_FFI_EXPORT coda_status_t coda_table_col_append(
	coda_doc_t* doc, coda_node_t t,
	const char* name, size_t name_len);
CODA_FFI_EXPORT size_t coda_table_row_count(
	const coda_doc_t* doc, coda_node_t t);
CODA_FFI_EXPORT coda_node_t coda_table_row_at(
	const coda_doc_t* doc, coda_node_t t, size_t row_idx);
CODA_FFI_EXPORT coda_status_t coda_table_row_append(
	coda_doc_t* doc, coda_node_t t, coda_node_t row);
CODA_FFI_EXPORT coda_status_t coda_table_row_set(
	coda_doc_t* doc, coda_node_t t, size_t row_idx, coda_node_t row);
CODA_FFI_EXPORT coda_status_t coda_table_row_remove(
	coda_doc_t* doc, coda_node_t t, size_t row_idx);

// Keyed tables. Column rules match plain tables.
CODA_FFI_EXPORT size_t coda_keyed_table_col_count(
	const coda_doc_t* doc, coda_node_t kt);
CODA_FFI_EXPORT coda_str_t coda_keyed_table_col_name(
	const coda_doc_t* doc, coda_node_t kt, size_t col_idx);
CODA_FFI_EXPORT coda_status_t coda_keyed_table_col_append(
	coda_doc_t* doc, coda_node_t kt,
	const char* name, size_t name_len);
CODA_FFI_EXPORT size_t coda_keyed_table_row_count(
	const coda_doc_t* doc, coda_node_t kt);
CODA_FFI_EXPORT coda_str_t coda_keyed_table_row_key_at(
	const coda_doc_t* doc, coda_node_t kt, size_t row_idx);
CODA_FFI_EXPORT coda_node_t coda_keyed_table_row_at(
	const coda_doc_t* doc, coda_node_t kt, size_t row_idx);
CODA_FFI_EXPORT coda_node_t coda_keyed_table_row_get(
	const coda_doc_t* doc, coda_node_t kt,
	const char* key, size_t key_len);
CODA_FFI_EXPORT coda_status_t coda_keyed_table_row_set(
	coda_doc_t* doc, coda_node_t kt,
	const char* key, size_t key_len,
	coda_node_t row);
CODA_FFI_EXPORT coda_status_t coda_keyed_table_row_remove(
	coda_doc_t* doc, coda_node_t kt,
	const char* key, size_t key_len);

// Rows are flat column->string maps. A detached row may be built freely. Once
// attached to a table, its schema is fixed: existing values may be changed, but
// required columns cannot be removed and unknown columns cannot be added.
CODA_FFI_EXPORT coda_str_t coda_row_get(
	const coda_doc_t* doc, coda_node_t row,
	const char* col, size_t col_len);
CODA_FFI_EXPORT coda_status_t coda_row_set(
	coda_doc_t* doc, coda_node_t row,
	const char* col, size_t col_len,
	const char* val, size_t val_len);
CODA_FFI_EXPORT coda_status_t coda_row_remove(
	coda_doc_t* doc, coda_node_t row,
	const char* col, size_t col_len);
CODA_FFI_EXPORT size_t coda_row_col_count(
	const coda_doc_t* doc, coda_node_t row);
CODA_FFI_EXPORT coda_str_t coda_row_col_name_at(
	const coda_doc_t* doc, coda_node_t row, size_t idx);
CODA_FFI_EXPORT coda_str_t coda_row_col_value_at(
	const coda_doc_t* doc, coda_node_t row, size_t idx);
CODA_FFI_EXPORT coda_str_t coda_row_comment_get(
	const coda_doc_t* doc, coda_node_t row);
CODA_FFI_EXPORT coda_status_t coda_row_comment_set(
	coda_doc_t* doc, coda_node_t row,
	const char* s, size_t len);

// Invalid/stale handles return {NULL,0} and populate err; they are never treated
// as the document root.
CODA_FFI_EXPORT coda_owned_str_t coda_node_serialize(
	const coda_doc_t* doc,
	coda_node_t n,
	const char* indent_unit,
	size_t indent_unit_len,
	coda_error_t* err);
CODA_FFI_EXPORT void coda_node_order(
	coda_doc_t* doc, coda_node_t n);
CODA_FFI_EXPORT void coda_node_order_weighted(
	coda_doc_t* doc,
	coda_node_t n,
	const char** keys,
	const float* weights,
	size_t count);

// New nodes start detached and must be attached exactly once.
CODA_FFI_EXPORT coda_node_t coda_new_string(
	coda_doc_t* doc, const char* s, size_t len);
CODA_FFI_EXPORT coda_node_t coda_new_block(coda_doc_t* doc);
CODA_FFI_EXPORT coda_node_t coda_new_array(coda_doc_t* doc);
CODA_FFI_EXPORT coda_node_t coda_new_table(coda_doc_t* doc);
CODA_FFI_EXPORT coda_node_t coda_new_keyed_table(coda_doc_t* doc);
CODA_FFI_EXPORT coda_node_t coda_new_row(coda_doc_t* doc);

#ifdef __cplusplus
} // extern "C"
#endif
