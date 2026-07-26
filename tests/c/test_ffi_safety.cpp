#include "../../ffi/coda_ffi.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include <string>

static std::string to_string(coda_str_t value) {
	return value.ptr ? std::string(value.ptr, value.len) : std::string();
}

static coda_node_t make_row(coda_doc_t* doc, const char* column, const char* value) {
	coda_node_t row = coda_new_row(doc);
	assert(row != 0);
	assert(coda_row_set(doc, row, column, std::strlen(column), value, std::strlen(value)) == CODA_OK);
	return row;
}

int main() {
	{
		coda_doc_t* left = coda_doc_new();
		coda_doc_t* right = coda_doc_new();
		coda_node_t foreign = coda_new_string(left, "left", 4);
		assert(coda_map_set(right, coda_doc_root(right), "x", 1, foreign) != CODA_OK);
		coda_doc_free(left);
		coda_doc_free(right);
	}

	{
		coda_doc_t* doc = coda_doc_new();
		coda_node_t root = coda_doc_root(doc);
		coda_node_t value = coda_new_string(doc, "value", 5);
		assert(coda_map_set(doc, root, "first", 5, value) == CODA_OK);
		assert(coda_map_set(doc, root, "second", 6, value) != CODA_OK);

		coda_node_t block = coda_new_block(doc);
		assert(coda_map_set(doc, root, "block", 5, block) == CODA_OK);
		assert(coda_map_set(doc, block, "self", 4, block) != CODA_OK);
		coda_doc_free(doc);
	}

	{
		coda_doc_t* doc = coda_doc_new();
		coda_node_t root = coda_doc_root(doc);
		coda_node_t old = coda_new_string(doc, "old", 3);
		assert(coda_map_set(doc, root, "value", 5, old) == CODA_OK);
		assert(coda_map_remove(doc, root, "value", 5) == CODA_OK);
		assert(coda_node_kind(doc, old) == CODA_NODE_NULL);

		coda_node_t replacement = coda_new_string(doc, "new", 3);
		assert(replacement != old);
		assert(coda_node_kind(doc, old) == CODA_NODE_NULL);

		coda_error_t error = {};
		coda_owned_str_t serialized = coda_node_serialize(doc, old, nullptr, 0, &error);
		assert(serialized.ptr == nullptr);
		assert(error.code == CODA_ERROR_INVALID_HANDLE);
		assert(error.message.ptr != nullptr);
		coda_error_clear(&error);
		coda_doc_free(doc);
	}

	{
		coda_doc_t* doc = coda_doc_new();
		coda_node_t root = coda_doc_root(doc);
		coda_node_t z = coda_new_string(doc, "z", 1);
		coda_node_t a = coda_new_string(doc, "a", 1);
		assert(coda_map_set(doc, root, "z", 1, z) == CODA_OK);
		assert(coda_map_set(doc, root, "a", 1, a) == CODA_OK);
		coda_doc_order(doc);
		assert(coda_node_kind(doc, z) == CODA_NODE_STRING);
		assert(to_string(coda_string_get(doc, z)) == "z");
		assert(to_string(coda_map_key_at(doc, root, 0)) == "a");
		coda_doc_free(doc);
	}

	{
		coda_doc_t* doc = coda_doc_new();
		coda_node_t root = coda_doc_root(doc);
		coda_node_t table = coda_new_keyed_table(doc);
		assert(coda_keyed_table_col_append(doc, table, "value", 5) == CODA_OK);
		assert(coda_keyed_table_col_append(doc, table, "value", 5) != CODA_OK);

		coda_node_t z = make_row(doc, "value", "z");
		coda_node_t a = make_row(doc, "value", "a");
		assert(coda_keyed_table_row_set(doc, table, "z", 1, z) == CODA_OK);
		assert(coda_keyed_table_row_set(doc, table, "a", 1, a) == CODA_OK);
		assert(coda_map_set(doc, root, "table", 5, table) == CODA_OK);

		assert(coda_keyed_table_col_append(doc, table, "later", 5) != CODA_OK);
		assert(coda_row_remove(doc, z, "value", 5) != CODA_OK);
		assert(coda_row_set(doc, z, "unknown", 7, "x", 1) != CODA_OK);
		assert(coda_row_set(doc, z, "value", 5, "updated", 7) == CODA_OK);

		coda_node_order(doc, table);
		assert(to_string(coda_keyed_table_row_key_at(doc, table, 0)) == "a");
		assert(to_string(coda_keyed_table_row_key_at(doc, table, 1)) == "z");
		coda_doc_free(doc);
	}

	std::cout << "FFI safety tests passed\n";
	return 0;
}
