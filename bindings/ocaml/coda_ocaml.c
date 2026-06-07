#include <caml/mlvalues.h>
#include <caml/memory.h>
#include <caml/alloc.h>
#include <caml/callback.h>
#include <caml/unix.h>
#include "coda_ffi.h"
#include <string.h>
#include <stdlib.h>

/* --- Types --- */

static VALUE wrap_doc(coda_doc_t* doc) {
    if (!doc) return Val_int(0);
    return Box_custom(doc, (void *)coda_doc_free);
}

static coda_doc_t* unwrap_doc(VALUE v) {
    if (caml_custom_is_custom(v)) {
        return (coda_doc_t *)caml_custom_data(v);
    }
    return NULL;
}

static VALUE wrap_node(coda_node_t node) {
    return Val_int(node);
}

static coda_node_t unwrap_node(VALUE v) {
    return (coda_node_t)Int_val(v);
}

static VALUE wrap_str(coda_str_t s) {
    return caml_copy_string(s.ptr, s.len);
}

static VALUE wrap_owned_str(coda_owned_str_t s) {
    VALUE v = caml_copy_string(s.ptr, s.len);
    coda_owned_str_free(s);
    return v;
}

static VALUE wrap_error(coda_error_t* err) {
    if (!err || err->code == 0 && err->message.ptr == NULL) return Val_unit;
    
    VALUE v = caml_alloc_tuple(5);
    caml_tuple_set(v, 0, Val_int(err->code));
    caml_tuple_set(v, 1, Val_int(err->line));
    caml_tuple_set(v, 2, Val_int(err->col));
    caml_tuple_set(v, 3, Val_uint(err->offset));
    caml_tuple_set(v, 4, wrap_owned_str(err->message));
    
    coda_error_clear(err);
    return v;
}

static const char* unwrap_opt_str(VALUE v, size_t* len) {
    if (v == Val_unit) {
        if (len) *len = 0;
        return NULL;
    }
    char* s = string_val(v);
    if (len) *len = caml_string_length(v);
    return s;
}

/* --- API Implementation --- */

VALUE coda_doc_new_ocaml(VALUE v) {
    return wrap_doc(coda_doc_new());
}

VALUE coda_doc_parse_ocaml(VALUE v_src, VALUE v_filename) {
    char* src = string_val(v_src);
    size_t len = caml_string_length(v_src);
    size_t fn_len;
    const char* filename = unwrap_opt_str(v_filename, &fn_len);
    
    coda_error_t err = {0};
    coda_doc_t* doc = coda_doc_parse(src, len, filename, &err);
    
    VALUE res = caml_alloc_tuple(3);
    caml_tuple_set(res, 0, wrap_doc(doc));
    caml_tuple_set(res, 1, Val_int(doc ? CODA_OK : CODA_ERR));
    caml_tuple_set(res, 2, wrap_error(&err));
    return res;
}

VALUE coda_doc_parse_file_ocaml(VALUE v_path) {
    const char* path = string_val(v_path);
    coda_error_t err = {0};
    coda_doc_t* doc = coda_doc_parse_file(path, &err);
    
    VALUE res = caml_alloc_tuple(3);
    caml_tuple_set(res, 0, wrap_doc(doc));
    caml_tuple_set(res, 1, Val_int(doc ? CODA_OK : CODA_ERR));
    caml_tuple_set(res, 2, wrap_error(&err));
    return res;
}

VALUE coda_doc_serialize_ocaml(VALUE v_doc, VALUE v_indent) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    size_t indent_len;
    const char* indent = unwrap_opt_str(v_indent, &indent_len);
    
    coda_error_t err = {0};
    coda_owned_str_t s = coda_doc_serialize(doc, indent, indent_len, &err);
    
    VALUE res = caml_alloc_tuple(3);
    caml_tuple_set(res, 0, wrap_owned_str(s));
    caml_tuple_set(res, 1, Val_int(s.ptr ? CODA_OK : CODA_ERR));
    caml_tuple_set(res, 2, wrap_error(&err));
    return res;
}

VALUE coda_doc_root_ocaml(VALUE v_doc) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return wrap_node(coda_doc_root(doc));
}

VALUE coda_node_kind_ocaml(VALUE v_doc, VALUE v_node) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return Val_int(coda_node_kind(doc, unwrap_node(v_node)));
}

VALUE coda_node_is_container_ocaml(VALUE v_doc, VALUE v_node) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return Val_int(coda_node_is_container(doc, unwrap_node(v_node)));
}

VALUE coda_node_comment_get_ocaml(VALUE v_doc, VALUE v_node) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return wrap_str(coda_node_comment_get(doc, unwrap_node(v_node)));
}

VALUE coda_node_comment_set_ocaml(VALUE v_doc, VALUE v_node, VALUE v_s) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    char* s = string_val(v_s);
    size_t len = caml_string_length(v_s);
    coda_status_t status = coda_node_comment_set(doc, unwrap_node(v_node), s, len);
    return Val_int(status);
}

VALUE coda_node_header_comment_get_ocaml(VALUE v_doc, VALUE v_node) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return wrap_str(coda_node_header_comment_get(doc, unwrap_node(v_node)));
}

VALUE coda_node_header_comment_set_ocaml(VALUE v_doc, VALUE v_node, VALUE v_s) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    char* s = string_val(v_s);
    size_t len = caml_string_length(v_s);
    coda_status_t status = coda_node_header_comment_set(doc, unwrap_node(v_node), s, len);
    return Val_int(status);
}

VALUE coda_string_get_ocaml(VALUE v_doc, VALUE v_node) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return wrap_str(coda_string_get(doc, unwrap_node(v_node)));
}

VALUE coda_string_set_ocaml(VALUE v_doc, VALUE v_node, VALUE v_s) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    char* s = string_val(v_s);
    size_t len = caml_string_length(v_s);
    coda_status_t status = coda_string_set(doc, unwrap_node(v_node), s, len);
    return Val_int(status);
}

VALUE coda_array_len_ocaml(VALUE v_doc, VALUE v_node) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return Val_uint(coda_array_len(doc, unwrap_node(v_node)));
}

VALUE coda_array_get_ocaml(VALUE v_doc, VALUE v_node, VALUE v_idx) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return wrap_node(coda_array_get(doc, unwrap_node(v_node), (size_t)UInt_val(v_idx)));
}

VALUE coda_array_set_ocaml(VALUE v_doc, VALUE v_node, VALUE v_idx, VALUE v_val) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    coda_status_t status = coda_array_set(doc, unwrap_node(v_node), (size_t)UInt_val(v_idx), unwrap_node(v_val));
    return Val_int(status);
}

VALUE coda_array_push_ocaml(VALUE v_doc, VALUE v_node, VALUE v_val) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    coda_status_t status = coda_array_push(doc, unwrap_node(v_node), unwrap_node(v_val));
    return Val_int(status);
}

VALUE coda_array_remove_ocaml(VALUE v_doc, VALUE v_node, VALUE v_idx) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    coda_status_t status = coda_array_remove(doc, unwrap_node(v_node), (size_t)UInt_val(v_idx));
    return Val_int(status);
}

VALUE coda_map_len_ocaml(VALUE v_doc, VALUE v_node) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return Val_uint(coda_map_len(doc, unwrap_node(v_node)));
}

VALUE coda_map_key_at_ocaml(VALUE v_doc, VALUE v_node, VALUE v_idx) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return wrap_str(coda_map_key_at(doc, unwrap_node(v_node), (size_t)UInt_val(v_idx)));
}

VALUE coda_map_value_at_ocaml(VALUE v_doc, VALUE v_node, VALUE v_idx) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return wrap_node(coda_map_value_at(doc, unwrap_node(v_node), (size_t)UInt_val(v_idx)));
}

VALUE coda_map_get_ocaml(VALUE v_doc, VALUE v_node, VALUE v_key) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    char* key = string_val(v_key);
    size_t len = caml_string_length(v_key);
    return wrap_node(coda_map_get(doc, unwrap_node(v_node), key, len));
}

VALUE coda_map_get_or_insert_ocaml(VALUE v_doc, VALUE v_node, VALUE v_key) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    char* key = string_val(v_key);
    size_t len = caml_string_length(v_key);
    return wrap_node(coda_map_get_or_insert(doc, unwrap_node(v_node), key, len));
}

VALUE coda_map_set_ocaml(VALUE v_doc, VALUE v_node, VALUE v_key, VALUE v_val) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    char* key = string_val(v_key);
    size_t len = caml_string_length(v_key);
    coda_status_t status = coda_map_set(doc, unwrap_node(v_node), key, len, unwrap_node(v_val));
    return Val_int(status);
}

VALUE coda_map_remove_ocaml(VALUE v_doc, VALUE v_node, VALUE v_key) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    char* key = string_val(v_key);
    size_t len = caml_string_length(v_key);
    coda_status_t status = coda_map_remove(doc, unwrap_node(v_node), key, len);
    return Val_int(status);
}

VALUE coda_table_col_count_ocaml(VALUE v_doc, VALUE v_node) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return Val_uint(coda_table_col_count(doc, unwrap_node(v_node)));
}

VALUE coda_table_col_name_ocaml(VALUE v_doc, VALUE v_node, VALUE v_idx) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return wrap_str(coda_table_col_name(doc, unwrap_node(v_node), (size_t)UInt_val(v_idx)));
}

VALUE coda_table_col_append_ocaml(VALUE v_doc, VALUE v_node, VALUE v_name) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    char* name = string_val(v_name);
    size_t len = caml_string_length(v_name);
    coda_status_t status = coda_table_col_append(doc, unwrap_node(v_node), name, len);
    return Val_int(status);
}

VALUE coda_table_row_count_ocaml(VALUE v_doc, VALUE v_node) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return Val_uint(coda_table_row_count(doc, unwrap_node(v_node)));
}

VALUE coda_table_row_at_ocaml(VALUE v_doc, VALUE v_node, VALUE v_idx) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return wrap_node(coda_table_row_at(doc, unwrap_node(v_node), (size_t)UInt_val(v_idx)));
}

VALUE coda_table_row_append_ocaml(VALUE v_doc, VALUE v_node, VALUE v_row) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    coda_status_t status = coda_table_row_append(doc, unwrap_node(v_node), unwrap_node(v_row));
    return Val_int(status);
}

VALUE coda_table_row_set_ocaml(VALUE v_doc, VALUE v_node, VALUE v_idx, VALUE v_row) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    coda_status_t status = coda_table_row_set(doc, unwrap_node(v_node), (size_t)UInt_val(v_idx), unwrap_node(v_row));
    return Val_int(status);
}

VALUE coda_table_row_remove_ocaml(VALUE v_doc, VALUE v_node, VALUE v_idx) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    coda_status_t status = coda_table_row_remove(doc, unwrap_node(v_node), (size_t)UInt_val(v_idx));
    return Val_int(status);
}

VALUE coda_keyed_table_col_count_ocaml(VALUE v_doc, VALUE v_node) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return Val_uint(coda_keyed_table_col_count(doc, unwrap_node(v_node)));
}

VALUE coda_keyed_table_col_name_ocaml(VALUE v_doc, VALUE v_node, VALUE v_idx) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return wrap_str(coda_keyed_table_col_name(doc, unwrap_node(v_node), (size_t)UInt_val(v_idx)));
}

VALUE coda_keyed_table_col_append_ocaml(VALUE v_doc, VALUE v_node, VALUE v_name) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    char* name = string_val(v_name);
    size_t len = caml_string_length(v_name);
    coda_status_t status = coda_keyed_table_col_append(doc, unwrap_node(v_node), name, len);
    return Val_int(status);
}

VALUE coda_keyed_table_row_count_ocaml(VALUE v_doc, VALUE v_node) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return Val_uint(coda_keyed_table_row_count(doc, unwrap_node(v_node)));
}

VALUE coda_keyed_table_row_key_at_ocaml(VALUE v_doc, VALUE v_node, VALUE v_idx) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return wrap_str(coda_keyed_table_row_key_at(doc, unwrap_node(v_node), (size_t)UInt_val(v_idx)));
}

VALUE coda_keyed_table_row_at_ocaml(VALUE v_doc, VALUE v_node, VALUE v_idx) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return wrap_node(coda_keyed_table_row_at(doc, unwrap_node(v_node), (size_t)UInt_val(v_idx)));
}

VALUE coda_keyed_table_row_get_ocaml(VALUE v_doc, VALUE v_node, VALUE v_key) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    char* key = string_val(v_key);
    size_t len = caml_string_length(v_key);
    return wrap_node(coda_keyed_table_row_get(doc, unwrap_node(v_node), key, len));
}

VALUE coda_keyed_table_row_set_ocaml(VALUE v_doc, VALUE v_node, VALUE v_key, VALUE v_row) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    char* key = string_val(v_key);
    size_t len = caml_string_length(v_key);
    coda_status_t status = coda_keyed_table_row_set(doc, unwrap_node(v_node), key, len, unwrap_node(v_row));
    return Val_int(status);
}

VALUE coda_keyed_table_row_remove_ocaml(VALUE v_doc, VALUE v_node, VALUE v_key) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    char* key = string_val(v_key);
    size_t len = caml_string_length(v_key);
    coda_status_t status = coda_keyed_table_row_remove(doc, unwrap_node(v_node), key, len);
    return Val_int(status);
}

VALUE coda_row_get_ocaml(VALUE v_doc, VALUE v_node, VALUE v_col) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    char* col = string_val(v_col);
    size_t len = caml_string_length(v_col);
    return wrap_str(coda_row_get(doc, unwrap_node(v_node), col, len));
}

VALUE coda_row_set_ocaml(VALUE v_doc, VALUE v_node, VALUE v_col, VALUE v_val) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    char* col = string_val(v_col);
    size_t col_len = caml_string_length(v_col);
    char* val = string_val(v_val);
    size_t val_len = caml_string_length(v_val);
    coda_status_t status = coda_row_set(doc, unwrap_node(v_node), col, col_len, val, val_len);
    return Val_int(status);
}

VALUE coda_row_remove_ocaml(VALUE v_doc, VALUE v_node, VALUE v_col) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    char* col = string_val(v_col);
    size_t len = caml_string_length(v_col);
    coda_status_t status = coda_row_remove(doc, unwrap_node(v_node), col, len);
    return Val_int(status);
}

VALUE coda_row_col_count_ocaml(VALUE v_doc, VALUE v_node) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return Val_uint(coda_row_col_count(doc, unwrap_node(v_node)));
}

VALUE coda_row_col_name_at_ocaml(VALUE v_doc, VALUE v_node, VALUE v_idx) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return wrap_str(coda_row_col_name_at(doc, unwrap_node(v_node), (size_t)UInt_val(v_idx)));
}

VALUE coda_row_col_value_at_ocaml(VALUE v_doc, VALUE v_node, VALUE v_idx) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return wrap_str(coda_row_col_value_at(doc, unwrap_node(v_node), (size_t)UInt_val(v_idx)));
}

VALUE coda_row_comment_get_ocaml(VALUE v_doc, VALUE v_node) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return wrap_str(coda_row_comment_get(doc, unwrap_node(v_node)));
}

VALUE coda_row_comment_set_ocaml(VALUE v_doc, VALUE v_node, VALUE v_s) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    char* s = string_val(v_s);
    size_t len = caml_string_length(v_s);
    coda_status_t status = coda_row_comment_set(doc, unwrap_node(v_node), s, len);
    return Val_int(status);
}

VALUE coda_node_serialize_ocaml(VALUE v_doc, VALUE v_node, VALUE v_indent) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    size_t indent_len;
    const char* indent = unwrap_opt_str(v_indent, &indent_len);
    
    coda_error_t err = {0};
    coda_owned_str_t s = coda_node_serialize(doc, unwrap_node(v_node), indent, indent_len, &err);
    
    VALUE res = caml_alloc_tuple(3);
    caml_tuple_set(res, 0, wrap_owned_str(s));
    caml_tuple_set(res, 1, Val_int(s.ptr ? CODA_OK : CODA_ERR));
    caml_tuple_set(res, 2, wrap_error(&err));
    return res;
}

VALUE coda_node_order_ocaml(VALUE v_doc, VALUE v_node) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    coda_node_order(doc, unwrap_node(v_node));
    return Val_unit;
}

VALUE coda_new_string_ocaml(VALUE v_doc, VALUE v_s) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    char* s = string_val(v_s);
    size_t len = caml_string_length(v_s);
    return wrap_node(coda_new_string(doc, s, len));
}

VALUE coda_new_block_ocaml(VALUE v_doc) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return wrap_node(coda_new_block(doc));
}

VALUE coda_new_array_ocaml(VALUE v_doc) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return wrap_node(coda_new_array(doc));
}

VALUE coda_new_table_ocaml(VALUE v_doc) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return wrap_node(coda_new_table(doc));
}

VALUE coda_new_keyed_table_ocaml(VALUE v_doc) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return wrap_node(coda_new_keyed_table(doc));
}

VALUE coda_new_row_ocaml(VALUE v_doc) {
    coda_doc_t* doc = unwrap_doc(v_doc);
    return wrap_node(coda_new_row(doc));
}
