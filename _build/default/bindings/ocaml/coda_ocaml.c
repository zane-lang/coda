#include <caml/mlvalues.h>
#include <caml/memory.h>
#include <caml/alloc.h>
#include <caml/callback.h>
#include "coda_ffi.h"
#include <string.h>
#include <stdlib.h>

/* --- Types --- */

static value wrap_doc(coda_doc_t* doc) {
    return caml_copy_nativeint((intnat)doc);
}

static coda_doc_t* unwrap_doc(value v) {
    return (coda_doc_t*)Nativeint_val(v);
}

static value wrap_node(coda_node_t node) {
    return Val_int(node);
}

static coda_node_t unwrap_node(value v) {
    return (coda_node_t)Int_val(v);
}

static value wrap_str(coda_str_t s) {
    return s.ptr ? caml_copy_string(s.ptr) : caml_copy_string("");
}

static value wrap_owned_str(coda_owned_str_t s) {
    if (!s.ptr) return caml_copy_string("");
    value v = caml_alloc_string(s.len);
    memcpy(Bytes_val(v), s.ptr, s.len);
    coda_owned_str_free(s);
    return v;
}

static value wrap_error(coda_error_t* err) {
    if (!err || (err->code == 0 && err->message.ptr == NULL)) return Val_int(0);

    value tup = caml_alloc_tuple(5);
    Store_field(tup, 0, Val_int(err->code));
    Store_field(tup, 1, Val_int(err->line));
    Store_field(tup, 2, Val_int(err->col));
    Store_field(tup, 3, Val_int(err->offset));
    Store_field(tup, 4, wrap_owned_str(err->message));

    return caml_alloc_some(tup);
}

static const char* unwrap_opt_str(value v, size_t* len) {
    if (v == Val_int(0)) {
        if (len) *len = 0;
        return NULL;
    }
    value sv = Field(v, 0);
    const char* s = String_val(sv);
    if (len) *len = caml_string_length(sv);
    return s;
}

/* --- API Implementation --- */

value coda_doc_new_ocaml(value v) {
    return wrap_doc(coda_doc_new());
}

value coda_doc_parse_ocaml(value v_src, value v_filename) {
    const char* src = String_val(v_src);
    size_t len = caml_string_length(v_src);
    size_t fn_len;
    const char* filename = unwrap_opt_str(v_filename, &fn_len);
    
    coda_error_t err = {0};
    coda_doc_t* doc = coda_doc_parse(src, len, filename, &err);
    
    value res = caml_alloc_tuple(3);
    Store_field(res, 0, wrap_doc(doc));
    Store_field(res, 1, Val_int(doc ? CODA_OK : CODA_ERR));
    Store_field(res, 2, wrap_error(&err));
    return res;
}

value coda_doc_parse_file_ocaml(value v_path) {
    const char* path = String_val(v_path);
    coda_error_t err = {0};
    coda_doc_t* doc = coda_doc_parse_file(path, &err);
    
    value res = caml_alloc_tuple(3);
    Store_field(res, 0, wrap_doc(doc));
    Store_field(res, 1, Val_int(doc ? CODA_OK : CODA_ERR));
    Store_field(res, 2, wrap_error(&err));
    return res;
}

value coda_doc_serialize_ocaml(value v_doc, value v_indent) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    size_t indent_len;
    const char* indent = unwrap_opt_str(v_indent, &indent_len);
    
    coda_error_t err = {0};
    coda_owned_str_t s = coda_doc_serialize(doc, indent, indent_len, &err);
    
    value res = caml_alloc_tuple(3);
    Store_field(res, 0, wrap_owned_str(s));
    Store_field(res, 1, Val_int(s.ptr ? CODA_OK : CODA_ERR));
    Store_field(res, 2, wrap_error(&err));
    return res;
}

value coda_doc_root_ocaml(value v_doc) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return wrap_node(coda_doc_root(doc));
}

value coda_node_kind_ocaml(value v_doc, value v_node) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return Val_int(coda_node_kind(doc, unwrap_node(v_node)));
}

value coda_node_is_container_ocaml(value v_doc, value v_node) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return Val_int(coda_node_is_container(doc, unwrap_node(v_node)));
}

value coda_node_comment_get_ocaml(value v_doc, value v_node) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return wrap_str(coda_node_comment_get(doc, unwrap_node(v_node)));
}

value coda_node_comment_set_ocaml(value v_doc, value v_node, value v_s) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    const char* s = String_val(v_s);
    size_t len = caml_string_length(v_s);
    coda_status_t status = coda_node_comment_set(doc, unwrap_node(v_node), s, len);
    return Val_int(status);
}

value coda_node_header_comment_get_ocaml(value v_doc, value v_node) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return wrap_str(coda_node_header_comment_get(doc, unwrap_node(v_node)));
}

value coda_node_header_comment_set_ocaml(value v_doc, value v_node, value v_s) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    const char* s = String_val(v_s);
    size_t len = caml_string_length(v_s);
    coda_status_t status = coda_node_header_comment_set(doc, unwrap_node(v_node), s, len);
    return Val_int(status);
}

value coda_string_get_ocaml(value v_doc, value v_node) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return wrap_str(coda_string_get(doc, unwrap_node(v_node)));
}

value coda_string_set_ocaml(value v_doc, value v_node, value v_s) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    const char* s = String_val(v_s);
    size_t len = caml_string_length(v_s);
    coda_status_t status = coda_string_set(doc, unwrap_node(v_node), s, len);
    return Val_int(status);
}

value coda_array_len_ocaml(value v_doc, value v_node) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return Val_int(coda_array_len(doc, unwrap_node(v_node)));
}

value coda_array_get_ocaml(value v_doc, value v_node, value v_idx) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return wrap_node(coda_array_get(doc, unwrap_node(v_node), (size_t)Long_val(v_idx)));
}

value coda_array_set_ocaml(value v_doc, value v_node, value v_idx, value v_val) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    coda_status_t status = coda_array_set(doc, unwrap_node(v_node), (size_t)Long_val(v_idx), unwrap_node(v_val));
    return Val_int(status);
}

value coda_array_push_ocaml(value v_doc, value v_node, value v_val) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    coda_status_t status = coda_array_push(doc, unwrap_node(v_node), unwrap_node(v_val));
    return Val_int(status);
}

value coda_array_remove_ocaml(value v_doc, value v_node, value v_idx) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    coda_status_t status = coda_array_remove(doc, unwrap_node(v_node), (size_t)Long_val(v_idx));
    return Val_int(status);
}

value coda_map_len_ocaml(value v_doc, value v_node) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return Val_int(coda_map_len(doc, unwrap_node(v_node)));
}

value coda_map_key_at_ocaml(value v_doc, value v_node, value v_idx) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return wrap_str(coda_map_key_at(doc, unwrap_node(v_node), (size_t)Long_val(v_idx)));
}

value coda_map_value_at_ocaml(value v_doc, value v_node, value v_idx) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return wrap_node(coda_map_value_at(doc, unwrap_node(v_node), (size_t)Long_val(v_idx)));
}

value coda_map_get_ocaml(value v_doc, value v_node, value v_key) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    const char* key = String_val(v_key);
    size_t len = caml_string_length(v_key);
    return wrap_node(coda_map_get(doc, unwrap_node(v_node), key, len));
}

value coda_map_get_or_insert_ocaml(value v_doc, value v_node, value v_key) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    const char* key = String_val(v_key);
    size_t len = caml_string_length(v_key);
    return wrap_node(coda_map_get_or_insert(doc, unwrap_node(v_node), key, len));
}

value coda_map_set_ocaml(value v_doc, value v_node, value v_key, value v_val) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    const char* key = String_val(v_key);
    size_t len = caml_string_length(v_key);
    coda_status_t status = coda_map_set(doc, unwrap_node(v_node), key, len, unwrap_node(v_val));
    return Val_int(status);
}

value coda_map_remove_ocaml(value v_doc, value v_node, value v_key) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    const char* key = String_val(v_key);
    size_t len = caml_string_length(v_key);
    coda_status_t status = coda_map_remove(doc, unwrap_node(v_node), key, len);
    return Val_int(status);
}

value coda_table_col_count_ocaml(value v_doc, value v_node) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return Val_int(coda_table_col_count(doc, unwrap_node(v_node)));
}

value coda_table_col_name_ocaml(value v_doc, value v_node, value v_idx) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return wrap_str(coda_table_col_name(doc, unwrap_node(v_node), (size_t)Long_val(v_idx)));
}

value coda_table_col_append_ocaml(value v_doc, value v_node, value v_name) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    const char* name = String_val(v_name);
    size_t len = caml_string_length(v_name);
    coda_status_t status = coda_table_col_append(doc, unwrap_node(v_node), name, len);
    return Val_int(status);
}

value coda_table_row_count_ocaml(value v_doc, value v_node) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return Val_int(coda_table_row_count(doc, unwrap_node(v_node)));
}

value coda_table_row_at_ocaml(value v_doc, value v_node, value v_idx) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return wrap_node(coda_table_row_at(doc, unwrap_node(v_node), (size_t)Long_val(v_idx)));
}

value coda_table_row_append_ocaml(value v_doc, value v_node, value v_row) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    coda_status_t status = coda_table_row_append(doc, unwrap_node(v_node), unwrap_node(v_row));
    return Val_int(status);
}

value coda_table_row_set_ocaml(value v_doc, value v_node, value v_idx, value v_row) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    coda_status_t status = coda_table_row_set(doc, unwrap_node(v_node), (size_t)Long_val(v_idx), unwrap_node(v_row));
    return Val_int(status);
}

value coda_table_row_remove_ocaml(value v_doc, value v_node, value v_idx) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    coda_status_t status = coda_table_row_remove(doc, unwrap_node(v_node), (size_t)Long_val(v_idx));
    return Val_int(status);
}

value coda_keyed_table_col_count_ocaml(value v_doc, value v_node) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return Val_int(coda_keyed_table_col_count(doc, unwrap_node(v_node)));
}

value coda_keyed_table_col_name_ocaml(value v_doc, value v_node, value v_idx) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return wrap_str(coda_keyed_table_col_name(doc, unwrap_node(v_node), (size_t)Long_val(v_idx)));
}

value coda_keyed_table_col_append_ocaml(value v_doc, value v_node, value v_name) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    const char* name = String_val(v_name);
    size_t len = caml_string_length(v_name);
    coda_status_t status = coda_keyed_table_col_append(doc, unwrap_node(v_node), name, len);
    return Val_int(status);
}

value coda_keyed_table_row_count_ocaml(value v_doc, value v_node) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return Val_int(coda_keyed_table_row_count(doc, unwrap_node(v_node)));
}

value coda_keyed_table_row_key_at_ocaml(value v_doc, value v_node, value v_idx) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return wrap_str(coda_keyed_table_row_key_at(doc, unwrap_node(v_node), (size_t)Long_val(v_idx)));
}

value coda_keyed_table_row_at_ocaml(value v_doc, value v_node, value v_idx) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return wrap_node(coda_keyed_table_row_at(doc, unwrap_node(v_node), (size_t)Long_val(v_idx)));
}

value coda_keyed_table_row_get_ocaml(value v_doc, value v_node, value v_key) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    const char* key = String_val(v_key);
    size_t len = caml_string_length(v_key);
    return wrap_node(coda_keyed_table_row_get(doc, unwrap_node(v_node), key, len));
}

value coda_keyed_table_row_set_ocaml(value v_doc, value v_node, value v_key, value v_row) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    const char* key = String_val(v_key);
    size_t len = caml_string_length(v_key);
    coda_status_t status = coda_keyed_table_row_set(doc, unwrap_node(v_node), key, len, unwrap_node(v_row));
    return Val_int(status);
}

value coda_keyed_table_row_remove_ocaml(value v_doc, value v_node, value v_key) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    const char* key = String_val(v_key);
    size_t len = caml_string_length(v_key);
    coda_status_t status = coda_keyed_table_row_remove(doc, unwrap_node(v_node), key, len);
    return Val_int(status);
}

value coda_row_get_ocaml(value v_doc, value v_node, value v_col) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    const char* col = String_val(v_col);
    size_t len = caml_string_length(v_col);
    return wrap_str(coda_row_get(doc, unwrap_node(v_node), col, len));
}

value coda_row_set_ocaml(value v_doc, value v_node, value v_col, value v_val) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    const char* col = String_val(v_col);
    size_t col_len = caml_string_length(v_col);
    const char* val = String_val(v_val);
    size_t val_len = caml_string_length(v_val);
    coda_status_t status = coda_row_set(doc, unwrap_node(v_node), col, col_len, val, val_len);
    return Val_int(status);
}

value coda_row_remove_ocaml(value v_doc, value v_node, value v_col) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    const char* col = String_val(v_col);
    size_t len = caml_string_length(v_col);
    coda_status_t status = coda_row_remove(doc, unwrap_node(v_node), col, len);
    return Val_int(status);
}

value coda_row_col_count_ocaml(value v_doc, value v_node) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return Val_int(coda_row_col_count(doc, unwrap_node(v_node)));
}

value coda_row_col_name_at_ocaml(value v_doc, value v_node, value v_idx) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return wrap_str(coda_row_col_name_at(doc, unwrap_node(v_node), (size_t)Long_val(v_idx)));
}

value coda_row_col_value_at_ocaml(value v_doc, value v_node, value v_idx) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return wrap_str(coda_row_col_value_at(doc, unwrap_node(v_node), (size_t)Long_val(v_idx)));
}

value coda_row_comment_get_ocaml(value v_doc, value v_node) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return wrap_str(coda_row_comment_get(doc, unwrap_node(v_node)));
}

value coda_row_comment_set_ocaml(value v_doc, value v_node, value v_s) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    const char* s = String_val(v_s);
    size_t len = caml_string_length(v_s);
    coda_status_t status = coda_row_comment_set(doc, unwrap_node(v_node), s, len);
    return Val_int(status);
}

value coda_node_serialize_ocaml(value v_doc, value v_node, value v_indent) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    size_t indent_len;
    const char* indent = unwrap_opt_str(v_indent, &indent_len);
    
    coda_error_t err = {0};
    coda_owned_str_t s = coda_node_serialize(doc, unwrap_node(v_node), indent, indent_len, &err);
    
    value res = caml_alloc_tuple(3);
    Store_field(res, 0, wrap_owned_str(s));
    Store_field(res, 1, Val_int(s.ptr ? CODA_OK : CODA_ERR));
    Store_field(res, 2, wrap_error(&err));
    return res;
}

value coda_node_order_ocaml(value v_doc, value v_node) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    coda_node_order(doc, unwrap_node(v_node));
    return Val_unit;
}

value coda_new_string_ocaml(value v_doc, value v_s) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    const char* s = String_val(v_s);
    size_t len = caml_string_length(v_s);
    return wrap_node(coda_new_string(doc, s, len));
}

value coda_new_block_ocaml(value v_doc) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return wrap_node(coda_new_block(doc));
}

value coda_new_array_ocaml(value v_doc) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return wrap_node(coda_new_array(doc));
}

value coda_new_table_ocaml(value v_doc) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return wrap_node(coda_new_table(doc));
}

value coda_new_keyed_table_ocaml(value v_doc) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return wrap_node(coda_new_keyed_table(doc));
}

value coda_new_row_ocaml(value v_doc) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return wrap_node(coda_new_row(doc));
}
