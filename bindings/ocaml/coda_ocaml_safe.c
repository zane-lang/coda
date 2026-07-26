#include <caml/alloc.h>
#include <caml/custom.h>
#include <caml/fail.h>
#include <caml/memory.h>
#include <caml/mlvalues.h>

#include "coda_ffi.h"

#include <stdlib.h>
#include <string.h>

/* A document is an owning OCaml custom block. The finalizer is a backstop;
   callers can also release it deterministically through Coda.free. */

static void finalize_doc(value v) {
	coda_doc_t** slot = (coda_doc_t**)Data_custom_val(v);
	if (*slot) {
		coda_doc_free(*slot);
		*slot = NULL;
	}
}

static struct custom_operations doc_operations = {
	"zane.coda.document",
	finalize_doc,
	custom_compare_default,
	custom_hash_default,
	custom_serialize_default,
	custom_deserialize_default,
	custom_compare_ext_default,
	custom_fixed_length_default,
};

static value wrap_doc(coda_doc_t* doc) {
	CAMLparam0();
	CAMLlocal1(v);
	v = caml_alloc_custom(&doc_operations, sizeof(coda_doc_t*), 0, 1);
	*((coda_doc_t**)Data_custom_val(v)) = doc;
	CAMLreturn(v);
}

static coda_doc_t* unwrap_doc(value v) {
	coda_doc_t* doc = *((coda_doc_t**)Data_custom_val(v));
	if (!doc) caml_failwith("Coda document has been freed");
	return doc;
}

static value wrap_node(coda_node_t node) {
	return Val_long((intnat)node);
}

static coda_node_t unwrap_node(value v) {
	return (coda_node_t)Long_val(v);
}

static value wrap_str(coda_str_t s) {
	CAMLparam0();
	CAMLlocal1(v);
	v = caml_alloc_string(s.ptr ? s.len : 0);
	if (s.ptr && s.len) memcpy(Bytes_val(v), s.ptr, s.len);
	CAMLreturn(v);
}

static value wrap_owned_str(coda_owned_str_t s) {
	CAMLparam0();
	CAMLlocal1(v);
	v = caml_alloc_string(s.ptr ? s.len : 0);
	if (s.ptr && s.len) memcpy(Bytes_val(v), s.ptr, s.len);
	coda_owned_str_free(s);
	CAMLreturn(v);
}

static value wrap_error(coda_error_t* err) {
	CAMLparam0();
	CAMLlocal3(tuple, message, some);
	if (!err || (err->message.ptr == NULL && err->line == 0 && err->col == 0 && err->offset == 0))
		CAMLreturn(Val_none);

	message = wrap_owned_str(err->message);
	err->message = (coda_owned_str_t){0};
	tuple = caml_alloc_tuple(5);
	Store_field(tuple, 0, Val_long(err->code));
	Store_field(tuple, 1, Val_long(err->line));
	Store_field(tuple, 2, Val_long(err->col));
	Store_field(tuple, 3, Val_long((intnat)err->offset));
	Store_field(tuple, 4, message);
	some = caml_alloc(1, 0);
	Store_field(some, 0, tuple);
	CAMLreturn(some);
}

static const char* unwrap_opt_str(value v, size_t* len) {
	if (Is_long(v)) {
		*len = 0;
		return NULL;
	}
	value text = Field(v, 0);
	*len = caml_string_length(text);
	return String_val(text);
}

static value result_with_doc(coda_doc_t* doc, coda_error_t* err) {
	CAMLparam0();
	CAMLlocal3(result, wrapped_doc, wrapped_error);
	wrapped_doc = wrap_doc(doc);
	wrapped_error = wrap_error(err);
	result = caml_alloc_tuple(3);
	Store_field(result, 0, wrapped_doc);
	Store_field(result, 1, Val_long(doc ? CODA_OK : CODA_ERR));
	Store_field(result, 2, wrapped_error);
	CAMLreturn(result);
}

static value result_with_string(coda_owned_str_t string, coda_error_t* err) {
	CAMLparam0();
	CAMLlocal3(result, wrapped_string, wrapped_error);
	const int ok = string.ptr != NULL;
	wrapped_string = wrap_owned_str(string);
	wrapped_error = wrap_error(err);
	result = caml_alloc_tuple(3);
	Store_field(result, 0, wrapped_string);
	Store_field(result, 1, Val_long(ok ? CODA_OK : CODA_ERR));
	Store_field(result, 2, wrapped_error);
	CAMLreturn(result);
}

CAMLprim value coda_doc_new_ocaml(value unit) {
	CAMLparam1(unit);
	CAMLreturn(wrap_doc(coda_doc_new()));
}

CAMLprim value coda_doc_free_ocaml(value v_doc) {
	CAMLparam1(v_doc);
	finalize_doc(v_doc);
	CAMLreturn(Val_unit);
}

CAMLprim value coda_doc_parse_ocaml(value v_src, value v_filename) {
	CAMLparam2(v_src, v_filename);
	size_t filename_len = 0;
	const char* filename = unwrap_opt_str(v_filename, &filename_len);
	(void)filename_len;
	coda_error_t error = {0};
	coda_doc_t* doc = coda_doc_parse(
		String_val(v_src), caml_string_length(v_src), filename, &error);
	CAMLreturn(result_with_doc(doc, &error));
}

CAMLprim value coda_doc_parse_file_ocaml(value v_path) {
	CAMLparam1(v_path);
	coda_error_t error = {0};
	coda_doc_t* doc = coda_doc_parse_file(String_val(v_path), &error);
	CAMLreturn(result_with_doc(doc, &error));
}

CAMLprim value coda_doc_serialize_ocaml(value v_doc, value v_indent) {
	CAMLparam2(v_doc, v_indent);
	size_t indent_len = 0;
	const char* indent = unwrap_opt_str(v_indent, &indent_len);
	coda_error_t error = {0};
	coda_owned_str_t string = coda_doc_serialize(unwrap_doc(v_doc), indent, indent_len, &error);
	CAMLreturn(result_with_string(string, &error));
}

CAMLprim value coda_doc_root_ocaml(value v_doc) {
	CAMLparam1(v_doc);
	CAMLreturn(wrap_node(coda_doc_root(unwrap_doc(v_doc))));
}

#define DOC_NODE_IMMEDIATE_STUB(name, c_name, expression) \
CAMLprim value name(value v_doc, value v_node) { \
	CAMLparam2(v_doc, v_node); \
	coda_doc_t* doc = unwrap_doc(v_doc); \
	coda_node_t node = unwrap_node(v_node); \
	CAMLreturn(expression); \
}

#define DOC_NODE_STRING_GETTER(name, c_name) \
CAMLprim value name(value v_doc, value v_node) { \
	CAMLparam2(v_doc, v_node); \
	CAMLreturn(wrap_str(c_name(unwrap_doc(v_doc), unwrap_node(v_node)))); \
}

#define DOC_NODE_STRING_SETTER(name, c_name) \
CAMLprim value name(value v_doc, value v_node, value v_text) { \
	CAMLparam3(v_doc, v_node, v_text); \
	CAMLreturn(Val_long(c_name(unwrap_doc(v_doc), unwrap_node(v_node), \
		String_val(v_text), caml_string_length(v_text)))); \
}

DOC_NODE_IMMEDIATE_STUB(coda_node_kind_ocaml, coda_node_kind, Val_long(coda_node_kind(doc, node)))
DOC_NODE_IMMEDIATE_STUB(coda_node_is_container_ocaml, coda_node_is_container, Val_bool(coda_node_is_container(doc, node)))
DOC_NODE_STRING_GETTER(coda_node_comment_get_ocaml, coda_node_comment_get)
DOC_NODE_STRING_SETTER(coda_node_comment_set_ocaml, coda_node_comment_set)
DOC_NODE_STRING_GETTER(coda_node_header_comment_get_ocaml, coda_node_header_comment_get)
DOC_NODE_STRING_SETTER(coda_node_header_comment_set_ocaml, coda_node_header_comment_set)
DOC_NODE_STRING_GETTER(coda_string_get_ocaml, coda_string_get)
DOC_NODE_STRING_SETTER(coda_string_set_ocaml, coda_string_set)
DOC_NODE_IMMEDIATE_STUB(coda_array_len_ocaml, coda_array_len, Val_long((intnat)coda_array_len(doc, node)))
DOC_NODE_IMMEDIATE_STUB(coda_map_len_ocaml, coda_map_len, Val_long((intnat)coda_map_len(doc, node)))
DOC_NODE_IMMEDIATE_STUB(coda_table_col_count_ocaml, coda_table_col_count, Val_long((intnat)coda_table_col_count(doc, node)))
DOC_NODE_IMMEDIATE_STUB(coda_table_row_count_ocaml, coda_table_row_count, Val_long((intnat)coda_table_row_count(doc, node)))
DOC_NODE_IMMEDIATE_STUB(coda_keyed_table_col_count_ocaml, coda_keyed_table_col_count, Val_long((intnat)coda_keyed_table_col_count(doc, node)))
DOC_NODE_IMMEDIATE_STUB(coda_keyed_table_row_count_ocaml, coda_keyed_table_row_count, Val_long((intnat)coda_keyed_table_row_count(doc, node)))
DOC_NODE_IMMEDIATE_STUB(coda_row_col_count_ocaml, coda_row_col_count, Val_long((intnat)coda_row_col_count(doc, node)))
DOC_NODE_STRING_GETTER(coda_row_comment_get_ocaml, coda_row_comment_get)
DOC_NODE_STRING_SETTER(coda_row_comment_set_ocaml, coda_row_comment_set)

#undef DOC_NODE_IMMEDIATE_STUB
#undef DOC_NODE_STRING_GETTER
#undef DOC_NODE_STRING_SETTER

#define DOC_NODE_INDEX_NODE(name, c_name) \
CAMLprim value name(value v_doc, value v_node, value v_index) { \
	CAMLparam3(v_doc, v_node, v_index); \
	CAMLreturn(wrap_node(c_name(unwrap_doc(v_doc), unwrap_node(v_node), \
		(size_t)Long_val(v_index)))); \
}

#define DOC_NODE_INDEX_STRING(name, c_name) \
CAMLprim value name(value v_doc, value v_node, value v_index) { \
	CAMLparam3(v_doc, v_node, v_index); \
	CAMLreturn(wrap_str(c_name(unwrap_doc(v_doc), unwrap_node(v_node), \
		(size_t)Long_val(v_index)))); \
}

DOC_NODE_INDEX_NODE(coda_array_get_ocaml, coda_array_get)
DOC_NODE_INDEX_STRING(coda_map_key_at_ocaml, coda_map_key_at)
DOC_NODE_INDEX_NODE(coda_map_value_at_ocaml, coda_map_value_at)
DOC_NODE_INDEX_STRING(coda_table_col_name_ocaml, coda_table_col_name)
DOC_NODE_INDEX_NODE(coda_table_row_at_ocaml, coda_table_row_at)
DOC_NODE_INDEX_STRING(coda_keyed_table_col_name_ocaml, coda_keyed_table_col_name)
DOC_NODE_INDEX_STRING(coda_keyed_table_row_key_at_ocaml, coda_keyed_table_row_key_at)
DOC_NODE_INDEX_NODE(coda_keyed_table_row_at_ocaml, coda_keyed_table_row_at)
DOC_NODE_INDEX_STRING(coda_row_col_name_at_ocaml, coda_row_col_name_at)
DOC_NODE_INDEX_STRING(coda_row_col_value_at_ocaml, coda_row_col_value_at)

#undef DOC_NODE_INDEX_NODE
#undef DOC_NODE_INDEX_STRING

CAMLprim value coda_array_set_ocaml(value v_doc, value v_node, value v_index, value v_value) {
	CAMLparam4(v_doc, v_node, v_index, v_value);
	CAMLreturn(Val_long(coda_array_set(unwrap_doc(v_doc), unwrap_node(v_node),
		(size_t)Long_val(v_index), unwrap_node(v_value))));
}

CAMLprim value coda_array_push_ocaml(value v_doc, value v_node, value v_value) {
	CAMLparam3(v_doc, v_node, v_value);
	CAMLreturn(Val_long(coda_array_push(unwrap_doc(v_doc), unwrap_node(v_node), unwrap_node(v_value))));
}

CAMLprim value coda_array_remove_ocaml(value v_doc, value v_node, value v_index) {
	CAMLparam3(v_doc, v_node, v_index);
	CAMLreturn(Val_long(coda_array_remove(unwrap_doc(v_doc), unwrap_node(v_node),
		(size_t)Long_val(v_index))));
}

CAMLprim value coda_map_get_ocaml(value v_doc, value v_node, value v_key) {
	CAMLparam3(v_doc, v_node, v_key);
	CAMLreturn(wrap_node(coda_map_get(unwrap_doc(v_doc), unwrap_node(v_node),
		String_val(v_key), caml_string_length(v_key))));
}

CAMLprim value coda_map_get_or_insert_ocaml(value v_doc, value v_node, value v_key) {
	CAMLparam3(v_doc, v_node, v_key);
	CAMLreturn(wrap_node(coda_map_get_or_insert(unwrap_doc(v_doc), unwrap_node(v_node),
		String_val(v_key), caml_string_length(v_key))));
}

CAMLprim value coda_map_set_ocaml(value v_doc, value v_node, value v_key, value v_value) {
	CAMLparam4(v_doc, v_node, v_key, v_value);
	CAMLreturn(Val_long(coda_map_set(unwrap_doc(v_doc), unwrap_node(v_node),
		String_val(v_key), caml_string_length(v_key), unwrap_node(v_value))));
}

CAMLprim value coda_map_remove_ocaml(value v_doc, value v_node, value v_key) {
	CAMLparam3(v_doc, v_node, v_key);
	CAMLreturn(Val_long(coda_map_remove(unwrap_doc(v_doc), unwrap_node(v_node),
		String_val(v_key), caml_string_length(v_key))));
}

#define DOC_NODE_APPEND_STRING(name, c_name) \
CAMLprim value name(value v_doc, value v_node, value v_text) { \
	CAMLparam3(v_doc, v_node, v_text); \
	CAMLreturn(Val_long(c_name(unwrap_doc(v_doc), unwrap_node(v_node), \
		String_val(v_text), caml_string_length(v_text)))); \
}

DOC_NODE_APPEND_STRING(coda_table_col_append_ocaml, coda_table_col_append)
DOC_NODE_APPEND_STRING(coda_keyed_table_col_append_ocaml, coda_keyed_table_col_append)
#undef DOC_NODE_APPEND_STRING

CAMLprim value coda_table_row_append_ocaml(value v_doc, value v_node, value v_row) {
	CAMLparam3(v_doc, v_node, v_row);
	CAMLreturn(Val_long(coda_table_row_append(unwrap_doc(v_doc), unwrap_node(v_node), unwrap_node(v_row))));
}

CAMLprim value coda_table_row_set_ocaml(value v_doc, value v_node, value v_index, value v_row) {
	CAMLparam4(v_doc, v_node, v_index, v_row);
	CAMLreturn(Val_long(coda_table_row_set(unwrap_doc(v_doc), unwrap_node(v_node),
		(size_t)Long_val(v_index), unwrap_node(v_row))));
}

CAMLprim value coda_table_row_remove_ocaml(value v_doc, value v_node, value v_index) {
	CAMLparam3(v_doc, v_node, v_index);
	CAMLreturn(Val_long(coda_table_row_remove(unwrap_doc(v_doc), unwrap_node(v_node),
		(size_t)Long_val(v_index))));
}

CAMLprim value coda_keyed_table_row_get_ocaml(value v_doc, value v_node, value v_key) {
	CAMLparam3(v_doc, v_node, v_key);
	CAMLreturn(wrap_node(coda_keyed_table_row_get(unwrap_doc(v_doc), unwrap_node(v_node),
		String_val(v_key), caml_string_length(v_key))));
}

CAMLprim value coda_keyed_table_row_set_ocaml(
		value v_doc, value v_node, value v_key, value v_row) {
	CAMLparam4(v_doc, v_node, v_key, v_row);
	CAMLreturn(Val_long(coda_keyed_table_row_set(unwrap_doc(v_doc), unwrap_node(v_node),
		String_val(v_key), caml_string_length(v_key), unwrap_node(v_row))));
}

CAMLprim value coda_keyed_table_row_remove_ocaml(value v_doc, value v_node, value v_key) {
	CAMLparam3(v_doc, v_node, v_key);
	CAMLreturn(Val_long(coda_keyed_table_row_remove(unwrap_doc(v_doc), unwrap_node(v_node),
		String_val(v_key), caml_string_length(v_key))));
}

CAMLprim value coda_row_get_ocaml(value v_doc, value v_node, value v_column) {
	CAMLparam3(v_doc, v_node, v_column);
	CAMLreturn(wrap_str(coda_row_get(unwrap_doc(v_doc), unwrap_node(v_node),
		String_val(v_column), caml_string_length(v_column))));
}

CAMLprim value coda_row_set_ocaml(
		value v_doc, value v_node, value v_column, value v_value) {
	CAMLparam4(v_doc, v_node, v_column, v_value);
	CAMLreturn(Val_long(coda_row_set(unwrap_doc(v_doc), unwrap_node(v_node),
		String_val(v_column), caml_string_length(v_column),
		String_val(v_value), caml_string_length(v_value))));
}

CAMLprim value coda_row_remove_ocaml(value v_doc, value v_node, value v_column) {
	CAMLparam3(v_doc, v_node, v_column);
	CAMLreturn(Val_long(coda_row_remove(unwrap_doc(v_doc), unwrap_node(v_node),
		String_val(v_column), caml_string_length(v_column))));
}

CAMLprim value coda_node_serialize_ocaml(value v_doc, value v_node, value v_indent) {
	CAMLparam3(v_doc, v_node, v_indent);
	size_t indent_len = 0;
	const char* indent = unwrap_opt_str(v_indent, &indent_len);
	coda_error_t error = {0};
	coda_owned_str_t string = coda_node_serialize(
		unwrap_doc(v_doc), unwrap_node(v_node), indent, indent_len, &error);
	CAMLreturn(result_with_string(string, &error));
}

CAMLprim value coda_node_order_ocaml(value v_doc, value v_node) {
	CAMLparam2(v_doc, v_node);
	coda_node_order(unwrap_doc(v_doc), unwrap_node(v_node));
	CAMLreturn(Val_unit);
}

CAMLprim value coda_node_order_weighted_ocaml(
		value v_doc, value v_node, value v_weights) {
	CAMLparam3(v_doc, v_node, v_weights);
	size_t count = 0;
	for (value cursor = v_weights; cursor != Val_emptylist; cursor = Field(cursor, 1)) ++count;

	char** keys = count ? (char**)calloc(count, sizeof(char*)) : NULL;
	float* weights = count ? (float*)calloc(count, sizeof(float)) : NULL;
	if (count && (!keys || !weights)) {
		free(keys);
		free(weights);
		caml_raise_out_of_memory();
	}

	size_t index = 0;
	for (value cursor = v_weights; cursor != Val_emptylist; cursor = Field(cursor, 1)) {
		value pair = Field(cursor, 0);
		value key = Field(pair, 0);
		const size_t length = caml_string_length(key);
		keys[index] = (char*)malloc(length + 1);
		if (!keys[index]) {
			for (size_t i = 0; i < index; ++i) free(keys[i]);
			free(keys);
			free(weights);
			caml_raise_out_of_memory();
		}
		memcpy(keys[index], String_val(key), length);
		keys[index][length] = '\0';
		weights[index] = (float)Double_val(Field(pair, 1));
		++index;
	}

	coda_node_order_weighted(unwrap_doc(v_doc), unwrap_node(v_node),
		(const char**)keys, weights, count);
	for (size_t i = 0; i < count; ++i) free(keys[i]);
	free(keys);
	free(weights);
	CAMLreturn(Val_unit);
}

CAMLprim value coda_new_string_ocaml(value v_doc, value v_text) {
	CAMLparam2(v_doc, v_text);
	CAMLreturn(wrap_node(coda_new_string(unwrap_doc(v_doc), String_val(v_text),
		caml_string_length(v_text))));
}

#define DOC_NEW_NODE(name, c_name) \
CAMLprim value name(value v_doc) { \
	CAMLparam1(v_doc); \
	CAMLreturn(wrap_node(c_name(unwrap_doc(v_doc)))); \
}

DOC_NEW_NODE(coda_new_block_ocaml, coda_new_block)
DOC_NEW_NODE(coda_new_array_ocaml, coda_new_array)
DOC_NEW_NODE(coda_new_table_ocaml, coda_new_table)
DOC_NEW_NODE(coda_new_keyed_table_ocaml, coda_new_keyed_table)
DOC_NEW_NODE(coda_new_row_ocaml, coda_new_row)
#undef DOC_NEW_NODE
