#pragma once

#include <stddef.h>   // size_t
#include <stdint.h>   // uint32_t, uint8_t, etc.
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// ------------------------- Export / ABI helpers -------------------------

#if defined(_WIN32)
#define CODA_FFI_EXPORT __declspec(dllexport)
#else
#define CODA_FFI_EXPORT __attribute__((visibility("default")))
#endif

// ------------------------- Basic types -------------------------

typedef struct coda_doc coda_doc_t;

// 0 is reserved as "null/invalid".
typedef uint32_t coda_node_t;

typedef struct coda_str {
	const char* ptr;
	size_t len;
} coda_str_t;

typedef struct coda_owned_str {
	char* ptr;   // null-terminated for convenience
	size_t len;  // excludes the null terminator
} coda_owned_str_t;

typedef enum coda_node_kind {
	CODA_NODE_NULL  = 0,
	CODA_NODE_FILE  = 1, // top-level statements (serializes without braces)
	CODA_NODE_STRING= 2,
	CODA_NODE_BLOCK = 3, // {...}
	CODA_NODE_ARRAY = 4, // [...]
	CODA_NODE_TABLE = 5  // internal "table/map" node (used for rows & keyed tables)
} coda_node_kind_t;

typedef enum coda_status {
	CODA_OK = 0,
	CODA_ERR = 1,
	CODA_NOT_FOUND = 2,
	CODA_BAD_KIND = 3,
	CODA_OUT_OF_RANGE = 4
} coda_status_t;

typedef struct coda_error {
	uint32_t code;     // maps from coda::ParseErrorCode when available
	uint32_t line;     // 1-based
	uint32_t col;      // 1-based
	size_t   offset;   // byte offset in source when available
	coda_owned_str_t message; // owned formatted error message
} coda_error_t;

typedef enum coda_parse_error_code {
	CODA_PARSE_UNEXPECTED_TOKEN     = 0,
	CODA_PARSE_UNEXPECTED_EOF       = 1,
	CODA_PARSE_DUPLICATE_KEY        = 2,
	CODA_PARSE_DUPLICATE_FIELD      = 3,
	CODA_PARSE_RAGGED_ROW           = 4,
	CODA_PARSE_INVALID_ESCAPE       = 5,
	CODA_PARSE_UNTERMINATED_STRING  = 6,
	CODA_PARSE_NESTED_BLOCK         = 7,
	CODA_PARSE_CONTENT_AFTER_BRACE  = 8,
	CODA_PARSE_KEY_IN_BLOCK         = 9,
} coda_parse_error_code_t;

CODA_FFI_EXPORT coda_str_t coda_parse_error_code_name(uint32_t code);

// ------------------------- Memory management -------------------------

CODA_FFI_EXPORT void coda_free(void* p);

// Frees message buffer inside err (does NOT free err itself).
CODA_FFI_EXPORT void coda_error_clear(coda_error_t* err);

CODA_FFI_EXPORT void coda_owned_str_free(coda_owned_str_t s);

// ------------------------- ABI versioning -------------------------

CODA_FFI_EXPORT uint32_t coda_ffi_abi_version(void);

// ------------------------- Doc lifecycle -------------------------

CODA_FFI_EXPORT coda_doc_t* coda_doc_parse_fp(FILE* fp, const char* filename, coda_error_t* err);
CODA_FFI_EXPORT coda_doc_t* coda_doc_new(void);
CODA_FFI_EXPORT void coda_doc_free(coda_doc_t* doc);

// Parse UTF-8 bytes. On failure returns NULL and fills err->message (owned).
CODA_FFI_EXPORT coda_doc_t* coda_doc_parse(
	const char* src,
	size_t len,
	const char* filename,  // optional; may be NULL
	coda_error_t* err       // optional; may be NULL
);

CODA_FFI_EXPORT coda_doc_t* coda_doc_parse_file(
	const char* path,
	coda_error_t* err
);

// Serialize the doc to Coda text.
// indent_unit: e.g. "\t" or "  "
// On error: returns {NULL,0} and fills err.
CODA_FFI_EXPORT coda_owned_str_t coda_doc_serialize(
	const coda_doc_t* doc,
	const char* indent_unit,
	size_t indent_unit_len,
	coda_error_t* err
);

// Reorder keys and nested containers using default ordering rules.
CODA_FFI_EXPORT void coda_doc_order(coda_doc_t* doc);

// Reorder keys using a weight table (higher weight earlier).
CODA_FFI_EXPORT void coda_doc_order_weighted(
	coda_doc_t* doc,
	const char** keys,
	const float* weights,
	size_t count
);

CODA_FFI_EXPORT coda_node_t coda_doc_root(const coda_doc_t* doc);

// ------------------------- Node inspection -------------------------

CODA_FFI_EXPORT coda_node_kind_t coda_node_kind(
	const coda_doc_t* doc, coda_node_t n
);

CODA_FFI_EXPORT coda_str_t coda_node_comment_get(
	const coda_doc_t* doc, coda_node_t n
);

CODA_FFI_EXPORT coda_status_t coda_node_comment_set(
	coda_doc_t* doc, coda_node_t n,
	const char* s, size_t len
);

// Header comment for table-like containers.
// - For a plain table: the comment block directly before the header row inside the array.
// - For a keyed table: the comment block directly before the `key ...` header line.
// Returns empty string if none (or if doc/node is invalid).
CODA_FFI_EXPORT coda_str_t coda_node_header_comment_get(const coda_doc_t* doc, coda_node_t n);

CODA_FFI_EXPORT coda_status_t coda_node_header_comment_set(
	coda_doc_t* doc, coda_node_t n,
	const char* s, size_t len
);

// ------------------------- String nodes -------------------------

CODA_FFI_EXPORT coda_str_t coda_string_get(
	const coda_doc_t* doc, coda_node_t n
);

CODA_FFI_EXPORT coda_status_t coda_string_set(
	coda_doc_t* doc, coda_node_t n,
	const char* s, size_t len
);

// ------------------------- Array nodes -------------------------

CODA_FFI_EXPORT size_t coda_array_len(
	const coda_doc_t* doc, coda_node_t a
);

CODA_FFI_EXPORT coda_node_t coda_array_get(
	const coda_doc_t* doc, coda_node_t a, size_t idx
);

CODA_FFI_EXPORT coda_status_t coda_array_set(
	coda_doc_t* doc, coda_node_t a, size_t idx, coda_node_t value
);

CODA_FFI_EXPORT coda_status_t coda_array_push(
	coda_doc_t* doc, coda_node_t a, coda_node_t value
);

// ------------------------- Map-like nodes (FILE / BLOCK / TABLE) -------------------------

CODA_FFI_EXPORT size_t coda_map_len(
	const coda_doc_t* doc, coda_node_t m
);

CODA_FFI_EXPORT coda_str_t coda_map_key_at(
	const coda_doc_t* doc, coda_node_t m, size_t idx
);

CODA_FFI_EXPORT coda_node_t coda_map_value_at(
	const coda_doc_t* doc, coda_node_t m, size_t idx
);

// returns 0 if not found
CODA_FFI_EXPORT coda_node_t coda_map_get(
	const coda_doc_t* doc, coda_node_t m,
	const char* key, size_t key_len
);

// Like coda_map_get, but inserts an empty string node if missing.
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

// ------------------------- Node creation (in a doc arena) -------------------------

CODA_FFI_EXPORT coda_node_t coda_new_string(
	coda_doc_t* doc, const char* s, size_t len
);

CODA_FFI_EXPORT coda_node_t coda_new_block(coda_doc_t* doc);
CODA_FFI_EXPORT coda_node_t coda_new_array(coda_doc_t* doc);
CODA_FFI_EXPORT coda_node_t coda_new_table(coda_doc_t* doc);

#ifdef __cplusplus
} // extern "C"
#endif
