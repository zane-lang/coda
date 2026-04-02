#include "coda_ffi.h"
#include "../include/coda.hpp"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct coda_doc {
	enum class Kind : uint8_t { Null=0, File=1, String=2, Block=3, Array=4, Table=5 };

	struct Node {
		Kind kind = Kind::Null;
		std::string comment;
		std::string header_comment;

		// String
		std::string s;

		// Array
		std::vector<uint32_t> arr;

		// Map-like (FILE/BLOCK/TABLE)
		std::vector<std::pair<std::string, uint32_t>> entries; // insertion order
		std::unordered_map<std::string, size_t> index;         // key -> entries index
	};

	// nodes[0] is reserved null
	std::vector<Node> nodes;
	uint32_t root = 0;

	coda_doc() {
		nodes.emplace_back(); // null
		root = new_node(Kind::File);
	}

	uint32_t new_node(Kind k) {
		Node n;
		n.kind = k;
		nodes.push_back(std::move(n));
		return static_cast<uint32_t>(nodes.size() - 1);
	}

	Node* get(uint32_t id) {
		if (id == 0 || id >= nodes.size()) return nullptr;
		return &nodes[id];
	}
	const Node* get(uint32_t id) const {
		if (id == 0 || id >= nodes.size()) return nullptr;
		return &nodes[id];
	}
};

static inline coda_str_t view_of(const std::string& s) {
	return { s.data(), s.size() };
}

static inline coda_owned_str_t owned_from_std(std::string s) {
	coda_owned_str_t out{nullptr, 0};
	out.len = s.size();
	out.ptr = static_cast<char*>(std::malloc(out.len + 1));
	if (!out.ptr) return {nullptr, 0};
	std::memcpy(out.ptr, s.data(), out.len);
	out.ptr[out.len] = '\0';
	return out;
}

extern "C" CODA_FFI_EXPORT void coda_free(void* p) {
	std::free(p);
}

extern "C" CODA_FFI_EXPORT void coda_owned_str_free(coda_owned_str_t s) {
	std::free(s.ptr);
}

extern "C" CODA_FFI_EXPORT void coda_error_clear(coda_error_t* err) {
	if (!err) return;
	std::free(err->message.ptr);
	err->message = {nullptr, 0};
	err->code = 0;
	err->line = 0;
	err->col = 0;
	err->offset = 0;
}

extern "C" CODA_FFI_EXPORT uint32_t coda_ffi_abi_version(void) {
	// bump when breaking changes occur
	return 1;
}

// ------------------------- AST <-> DOM conversion -------------------------

static uint32_t intern_value(coda_doc& d, const coda::CodaValue& v);

static uint32_t intern_value(coda_doc& d, const coda::CodaValue& v) {
	using K = coda_doc::Kind;

	uint32_t id = 0;

	if (std::holds_alternative<std::string>(v.content.value)) {
		id = d.new_node(K::String);
		auto* n = d.get(id);
		n->s = std::get<std::string>(v.content.value);
		n->comment = v.comment;
		return id;
	}

	if (std::holds_alternative<coda::CodaBlock>(v.content.value)) {
		const auto& b = std::get<coda::CodaBlock>(v.content.value);
		id = d.new_node(K::Block);
		// Don't cache the pointer - it might be invalidated by recursive calls
		d.get(id)->comment = v.comment;
		for (const auto& kv : b.content) {
			uint32_t child = intern_value(d, kv.second);
			// Re-get pointer after each recursive call
			auto* n = d.get(id);
			n->index[kv.first] = n->entries.size();
			n->entries.emplace_back(kv.first, child);
		}
		return id;
	}

	if (std::holds_alternative<coda::CodaArray>(v.content.value)) {
		const auto& a = std::get<coda::CodaArray>(v.content.value);
		id = d.new_node(K::Array);
		auto* n = d.get(id);
		n->comment = v.comment;
		n->header_comment = a.headerComment;
		n->arr.reserve(a.content.size());
		for (const auto& elem : a.content) {
			uint32_t child_id = intern_value(d, elem);
			// Re-get pointer after recursive call
			d.get(id)->arr.push_back(child_id);
		}
		return id;
	}

	// Table
	const auto& t = std::get<coda::CodaTable>(v.content.value);
	id = d.new_node(K::Table);
	d.get(id)->comment = v.comment;
	d.get(id)->header_comment = t.headerComment;
	for (const auto& kv : t.content) {
		uint32_t child = intern_value(d, kv.second);
		// Re-get pointer after each recursive call
		auto* n = d.get(id);
		n->index[kv.first] = n->entries.size();
		n->entries.emplace_back(kv.first, child);
	}
	return id;
}

static coda::CodaValue emit_value(const coda_doc& d, uint32_t id) {
	const auto* n = d.get(id);
	if (!n) return coda::CodaValue{std::string("")};

	coda::CodaValue out;

	switch (n->kind) {
		case coda_doc::Kind::String: {
			out.content = n->s;
			out.comment = n->comment;
			return out;
		}
		case coda_doc::Kind::Block: {
			coda::CodaBlock b;
			for (const auto& [k, child] : n->entries) {
				b.content[k] = emit_value(d, child);
			}
			out.content = std::move(b);
			out.comment = n->comment;
			return out;
		}
		case coda_doc::Kind::Array: {
			coda::CodaArray a;
			a.headerComment = n->header_comment;
			a.content.reserve(n->arr.size());
			for (uint32_t child : n->arr) {
				a.content.push_back(emit_value(d, child));
			}
			out.content = std::move(a);
			out.comment = n->comment;
			return out;
		}
		case coda_doc::Kind::Table: {
			coda::CodaTable t;
			t.headerComment = n->header_comment;
			for (const auto& [k, child] : n->entries) {
				t.content[k] = emit_value(d, child);
			}
			out.content = std::move(t);
			out.comment = n->comment;
			return out;
		}
		default:
			out.content = std::string("");
			out.comment = n->comment;
			return out;
	}
}

static coda::CodaFile emit_file(const coda_doc& d) {
	coda::CodaFile f;
	const auto* root = d.get(d.root);
	if (!root) return f;

	for (const auto& [k, child] : root->entries) {
		f.statements[k] = emit_value(d, child);
	}
	return f;
}

// ------------------------- Doc API -------------------------
extern "C" CODA_FFI_EXPORT coda_str_t coda_parse_error_code_name(uint32_t code) {
	switch ((coda_parse_error_code_t)code) {
		case CODA_PARSE_UNEXPECTED_TOKEN:      return { "UnexpectedToken",      sizeof("UnexpectedToken") - 1 };
		case CODA_PARSE_UNEXPECTED_EOF:        return { "UnexpectedEOF",        sizeof("UnexpectedEOF") - 1 };
		case CODA_PARSE_DUPLICATE_KEY:         return { "DuplicateKey",         sizeof("DuplicateKey") - 1 };
		case CODA_PARSE_DUPLICATE_FIELD:       return { "DuplicateField",       sizeof("DuplicateField") - 1 };
		case CODA_PARSE_RAGGED_ROW:            return { "RaggedRow",            sizeof("RaggedRow") - 1 };
		case CODA_PARSE_INVALID_ESCAPE:        return { "InvalidEscape",        sizeof("InvalidEscape") - 1 };
		case CODA_PARSE_UNTERMINATED_STRING:   return { "UnterminatedString",   sizeof("UnterminatedString") - 1 };
		case CODA_PARSE_NESTED_BLOCK:          return { "NestedBlock",          sizeof("NestedBlock") - 1 };
		case CODA_PARSE_CONTENT_AFTER_BRACE:   return { "ContentAfterBrace",    sizeof("ContentAfterBrace") - 1 };
		case CODA_PARSE_KEY_IN_BLOCK:          return { "KeyInBlock",           sizeof("KeyInBlock") - 1 };
		default:                               return { "", 0 };
	}
}
static_assert((uint32_t)coda::ParseErrorCode::UnexpectedToken == CODA_PARSE_UNEXPECTED_TOKEN);
static_assert((uint32_t)coda::ParseErrorCode::KeyInBlock      == CODA_PARSE_KEY_IN_BLOCK);

extern "C" CODA_FFI_EXPORT coda_doc_t* coda_doc_parse_fp(FILE* fp, const char* filename, coda_error_t* err) {
	if (err) coda_error_clear(err);
	if (!fp) {
		if (err) err->message = owned_from_std("fp is null");
		return nullptr;
	}

	std::string data;
	char buf[4096];

	while (true) {
		size_t n = std::fread(buf, 1, sizeof(buf), fp);
		if (n > 0) data.append(buf, n);

		if (n < sizeof(buf)) {
			if (std::ferror(fp)) {
				if (err) err->message = owned_from_std("file read error");
				return nullptr;
			}
			break; // EOF
		}
	}

	return coda_doc_parse(data.data(), data.size(), filename, err);
}

extern "C" CODA_FFI_EXPORT coda_doc_t* coda_doc_new(void) {
	try {
		return new coda_doc();
	} catch (...) {
		return nullptr;
	}
}

extern "C" CODA_FFI_EXPORT void coda_doc_free(coda_doc_t* doc) {
	delete doc;
}

static void fill_parse_error(coda_error_t* err, const coda::ParseError& e) {
	if (!err) return;
	coda_error_clear(err);
	err->code = static_cast<uint32_t>(e.code);
	err->line = static_cast<uint32_t>(e.loc.line);
	err->col = static_cast<uint32_t>(e.loc.col);
	err->offset = e.loc.offset;
	err->message = owned_from_std(std::string(e.what()));
}

extern "C" CODA_FFI_EXPORT coda_doc_t* coda_doc_parse(
	const char* src, size_t len,
	const char* filename,
	coda_error_t* err
) {
	if (err) coda_error_clear(err);

	try {
		if (!src) {
			if (len != 0) {
				if (err) {
					coda_error_clear(err);
					err->code = 0;
					err->message = owned_from_std("src is null but len is nonzero");
				}
				return nullptr;
			}
			src = "";
		}
		std::string text(src, len);

		// Parser is part of the header-only implementation and throws ParseError on fatal issues. <!--citation:2-->
		coda::detail::Parser p(std::move(text), filename ? std::string(filename) : std::string(""));
		coda::CodaFile file = p.parse();

		auto* d = new coda_doc();
		// replace default empty root with parsed contents
		d->nodes.clear();
		d->nodes.emplace_back(); // null

		d->root = d->new_node(coda_doc::Kind::File);

		for (const auto& [k, v] : file.statements) {
			uint32_t child = intern_value(*d, v);
			// Re-get root pointer after each recursive call that might reallocate
			auto* root = d->get(d->root);
			root->index[k] = root->entries.size();
			root->entries.emplace_back(k, child);
		}

		return d;
	} catch (const coda::ParseError& e) {
		fill_parse_error(err, e);
		return nullptr;
	} catch (const std::exception& e) {
		if (err) {
			coda_error_clear(err);
			err->code = 0;
			err->message = owned_from_std(std::string("exception: ") + e.what());
		}
		return nullptr;
	} catch (...) {
		if (err) {
			coda_error_clear(err);
			err->code = 0;
			err->message = owned_from_std("unknown exception");
		}
		return nullptr;
	}
}

extern "C" CODA_FFI_EXPORT coda_doc_t* coda_doc_parse_file(
	const char* path,
	coda_error_t* err
) {
	if (err) coda_error_clear(err);

	try {
		if (!path) {
			if (err) err->message = owned_from_std("path is null");
			return nullptr;
		}

		std::ifstream f(path, std::ios::binary);
		if (!f) {
			if (err) err->message = owned_from_std(std::string("could not open: ") + path);
			return nullptr;
		}

		std::ostringstream ss;
		ss << f.rdbuf();
		std::string text = ss.str();
		return coda_doc_parse(text.data(), text.size(), path, err);
	} catch (const std::exception& e) {
		if (err) err->message = owned_from_std(std::string("exception: ") + e.what());
		return nullptr;
	} catch (...) {
		if (err) err->message = owned_from_std("unknown exception");
		return nullptr;
	}
}

extern "C" CODA_FFI_EXPORT coda_owned_str_t coda_doc_serialize(
	const coda_doc_t* doc,
	const char* indent_unit,
	size_t indent_unit_len,
	coda_error_t* err
) {
	if (!indent_unit && indent_unit_len != 0) return { nullptr, 0 };
	if (err) coda_error_clear(err);
	if (!doc) {
		if (err) err->message = owned_from_std("doc is null");
		return {nullptr, 0};
	}

	try {
		std::string unit = "\t";
		if (indent_unit) unit.assign(indent_unit, indent_unit_len);

		// CodaFile::serialize uses the same top-level “statements map” rendering as the project README describes. <!--citation:3-->
		coda::CodaFile f = emit_file(*doc);
		std::string out = f.serialize(unit);
		return owned_from_std(std::move(out));
	} catch (const std::exception& e) {
		if (err) err->message = owned_from_std(std::string("exception: ") + e.what());
		return {nullptr, 0};
	} catch (...) {
		if (err) err->message = owned_from_std("unknown exception");
		return {nullptr, 0};
	}
}

extern "C" CODA_FFI_EXPORT void coda_doc_order(coda_doc_t* doc) {
	if (!doc) return;
	try {
		coda::CodaFile f = emit_file(*doc);
		f.order();

		// Rebuild nodes from ordered file
		doc->nodes.clear();
		doc->nodes.emplace_back(); // null
		doc->root = doc->new_node(coda_doc::Kind::File);
		for (const auto& [k, v] : f.statements) {
			uint32_t child = intern_value(*doc, v);
			auto* root = doc->get(doc->root);
			root->index[k] = root->entries.size();
			root->entries.emplace_back(k, child);
		}
	} catch (...) {
		return;
	}
}

extern "C" CODA_FFI_EXPORT void coda_doc_order_weighted(
	coda_doc_t* doc,
	const char** keys,
	const float* weights,
	size_t count
) {
	if (!doc) return;
	try {
		std::unordered_map<std::string, float> weight_map;
		weight_map.reserve(count);
		for (size_t i = 0; i < count; ++i) {
			const char* key = keys ? keys[i] : nullptr;
			float w = weights ? weights[i] : 0.0f;
			weight_map[key ? key : ""] = w;
		}

		coda::CodaFile f = emit_file(*doc);
		f.order([&](const std::string& field) -> float {
			auto it = weight_map.find(field);
			if (it == weight_map.end()) return 0.0f;
			return it->second;
		});

		// Rebuild nodes from ordered file
		doc->nodes.clear();
		doc->nodes.emplace_back(); // null
		doc->root = doc->new_node(coda_doc::Kind::File);
		for (const auto& [k, v] : f.statements) {
			uint32_t child = intern_value(*doc, v);
			auto* root = doc->get(doc->root);
			root->index[k] = root->entries.size();
			root->entries.emplace_back(k, child);
		}
	} catch (...) {
		return;
	}
}

extern "C" CODA_FFI_EXPORT coda_node_t coda_doc_root(const coda_doc_t* doc) {
	if (!doc) return 0;
	return doc->root;
}

// ------------------------- Node inspection -------------------------

extern "C" CODA_FFI_EXPORT coda_node_kind_t coda_node_kind(const coda_doc_t* doc, coda_node_t n) {
	if (!doc) return CODA_NODE_NULL;
	const auto* node = doc->get(n);
	if (!node) return CODA_NODE_NULL;

	switch (node->kind) {
		case coda_doc::Kind::File:   return CODA_NODE_FILE;
		case coda_doc::Kind::String: return CODA_NODE_STRING;
		case coda_doc::Kind::Block:  return CODA_NODE_BLOCK;
		case coda_doc::Kind::Array:  return CODA_NODE_ARRAY;
		case coda_doc::Kind::Table:  return CODA_NODE_TABLE;
		default: return CODA_NODE_NULL;
	}
}

extern "C" CODA_FFI_EXPORT coda_str_t coda_node_comment_get(const coda_doc_t* doc, coda_node_t n) {
	static const std::string empty;
	if (!doc) return view_of(empty);
	const auto* node = doc->get(n);
	if (!node) return view_of(empty);
	return view_of(node->comment);
}

extern "C" CODA_FFI_EXPORT coda_status_t coda_node_comment_set(
	coda_doc_t* doc, coda_node_t n, const char* s, size_t len
) {
	if (!doc) return CODA_ERR;
	auto* node = doc->get(n);
	if (!node) return CODA_ERR;
	try {
		node->comment.assign(s ? s : "", len);
		return CODA_OK;
	} catch (...) {
		return CODA_ERR;
	}
}

extern "C" CODA_FFI_EXPORT coda_str_t
coda_node_header_comment_get(const coda_doc_t* doc, coda_node_t n) {
	static const std::string empty;
	if (!doc) return view_of(empty);

	const auto* node = doc->get(n);
	if (!node) return view_of(empty);

	return view_of(node->header_comment);
}

extern "C" CODA_FFI_EXPORT coda_status_t
coda_node_header_comment_set(coda_doc_t* doc, coda_node_t n, const char* s, size_t len) {
	if (!doc) return CODA_ERR;
	auto* node = doc->get(n);
	if (!node) return CODA_ERR;

	if (node->kind != coda_doc::Kind::Table && node->kind != coda_doc::Kind::Array)
		return CODA_BAD_KIND;

	try {
		node->header_comment.assign(s ? s : "", len);
		return CODA_OK;
	} catch (...) {
		return CODA_ERR;
	}
}

// ------------------------- String nodes -------------------------

extern "C" CODA_FFI_EXPORT coda_str_t coda_string_get(const coda_doc_t* doc, coda_node_t n) {
	static const std::string empty;
	if (!doc) return view_of(empty);
	const auto* node = doc->get(n);
	if (!node || node->kind != coda_doc::Kind::String) return view_of(empty);
	return view_of(node->s);
}

extern "C" CODA_FFI_EXPORT coda_status_t coda_string_set(
	coda_doc_t* doc, coda_node_t n, const char* s, size_t len
) {
	if (!s && len != 0) return CODA_ERR;
	if (!doc) return CODA_ERR;
	auto* node = doc->get(n);
	if (!node) return CODA_ERR;
	if (node->kind != coda_doc::Kind::String) return CODA_BAD_KIND;
	try {
		node->s.assign(s ? s : "", len);
		return CODA_OK;
	} catch (...) {
		return CODA_ERR;
	}
}

// ------------------------- Array nodes -------------------------

extern "C" CODA_FFI_EXPORT size_t coda_array_len(const coda_doc_t* doc, coda_node_t a) {
	if (!doc) return 0;
	const auto* node = doc->get(a);
	if (!node || node->kind != coda_doc::Kind::Array) return 0;
	return node->arr.size();
}

extern "C" CODA_FFI_EXPORT coda_node_t coda_array_get(const coda_doc_t* doc, coda_node_t a, size_t idx) {
	if (!doc) return 0;
	const auto* node = doc->get(a);
	if (!node || node->kind != coda_doc::Kind::Array) return 0;
	if (idx >= node->arr.size()) return 0;
	return node->arr[idx];
}

extern "C" CODA_FFI_EXPORT coda_status_t coda_array_set(coda_doc_t* doc, coda_node_t a, size_t idx, coda_node_t value) {
	if (!doc) return CODA_ERR;
	auto* node = doc->get(a);
	if (!node) return CODA_ERR;
	if (node->kind != coda_doc::Kind::Array) return CODA_BAD_KIND;
	if (idx >= node->arr.size()) return CODA_OUT_OF_RANGE;
	node->arr[idx] = value;
	return CODA_OK;
}

extern "C" CODA_FFI_EXPORT coda_status_t coda_array_push(coda_doc_t* doc, coda_node_t a, coda_node_t value) {
	if (!doc) return CODA_ERR;
	auto* node = doc->get(a);
	if (!node) return CODA_ERR;
	if (node->kind != coda_doc::Kind::Array) return CODA_BAD_KIND;
	try {
		node->arr.push_back(value);
		return CODA_OK;
	} catch (...) {
		return CODA_ERR;
	}
}

// ------------------------- Map-like nodes -------------------------

static inline bool is_map_kind(const coda_doc::Node* n) {
	if (!n) return false;
	return n->kind == coda_doc::Kind::File || n->kind == coda_doc::Kind::Block || n->kind == coda_doc::Kind::Table;
}

extern "C" CODA_FFI_EXPORT size_t coda_map_len(const coda_doc_t* doc, coda_node_t m) {
	if (!doc) return 0;
	const auto* node = doc->get(m);
	if (!is_map_kind(node)) return 0;
	return node->entries.size();
}

extern "C" CODA_FFI_EXPORT coda_str_t coda_map_key_at(const coda_doc_t* doc, coda_node_t m, size_t idx) {
	static const std::string empty;
	if (!doc) return view_of(empty);
	const auto* node = doc->get(m);
	if (!is_map_kind(node)) return view_of(empty);
	if (idx >= node->entries.size()) return view_of(empty);
	return view_of(node->entries[idx].first);
}

extern "C" CODA_FFI_EXPORT coda_node_t coda_map_value_at(const coda_doc_t* doc, coda_node_t m, size_t idx) {
	if (!doc) return 0;
	const auto* node = doc->get(m);
	if (!is_map_kind(node)) return 0;
	if (idx >= node->entries.size()) return 0;
	return node->entries[idx].second;
}

extern "C" CODA_FFI_EXPORT coda_node_t coda_map_get(
	const coda_doc_t* doc, coda_node_t m,
	const char* key, size_t key_len
) {
	if (!key && key_len != 0) return 0;
	if (!doc) return 0;
	const auto* node = doc->get(m);
	if (!is_map_kind(node)) return 0;

	std::string k(key ? key : "", key_len);
	auto it = node->index.find(k);
	if (it == node->index.end()) return 0;
	return node->entries[it->second].second;
}

extern "C" CODA_FFI_EXPORT coda_node_t coda_map_get_or_insert(
	coda_doc_t* doc, coda_node_t m,
	const char* key, size_t key_len
) {
	if (!key && key_len != 0) return 0;
	if (!doc) return 0;
	auto* node = doc->get(m);
	if (!is_map_kind(node)) return 0;

	std::string k(key ? key : "", key_len);
	auto it = node->index.find(k);
	if (it != node->index.end()) return node->entries[it->second].second;

	uint32_t child = doc->new_node(coda_doc::Kind::String);
	auto* child_node = doc->get(child);
	child_node->s.clear();

	node = doc->get(m);
	node->index[k] = node->entries.size();
	node->entries.emplace_back(std::move(k), child);
	return child;
}

extern "C" CODA_FFI_EXPORT coda_status_t coda_map_set(
	coda_doc_t* doc, coda_node_t m,
	const char* key, size_t key_len,
	coda_node_t value
) {
	if (!key && key_len != 0) return CODA_ERR;
	if (!doc) return CODA_ERR;
	auto* node = doc->get(m);
	if (!is_map_kind(node)) return CODA_BAD_KIND;

	try {
		std::string k(key ? key : "", key_len);
		auto it = node->index.find(k);
		if (it == node->index.end()) {
			node->index[k] = node->entries.size();
			node->entries.emplace_back(std::move(k), value);
		} else {
			node->entries[it->second].second = value;
		}
		return CODA_OK;
	} catch (...) {
		return CODA_ERR;
	}
}

extern "C" CODA_FFI_EXPORT coda_status_t coda_map_remove(
	coda_doc_t* doc, coda_node_t m,
	const char* key, size_t key_len
) {
	if (!doc) return CODA_ERR;
	auto* node = doc->get(m);
	if (!is_map_kind(node)) return CODA_BAD_KIND;

	std::string k(key ? key : "", key_len);
	auto it = node->index.find(k);
	if (it == node->index.end()) return CODA_NOT_FOUND;

	size_t idx = it->second;
	node->entries.erase(node->entries.begin() + idx);
	node->index.erase(it);

	// reindex following entries (keeps implementation simple; fine for config sizes)
	for (size_t i = idx; i < node->entries.size(); ++i) {
		node->index[node->entries[i].first] = i;
	}
	return CODA_OK;
}

// ------------------------- Node creation -------------------------

extern "C" CODA_FFI_EXPORT coda_node_t coda_new_string(coda_doc_t* doc, const char* s, size_t len) {
	if (!s && len != 0) return 0;
	if (!doc) return 0;
	try {
		uint32_t id = doc->new_node(coda_doc::Kind::String);
		auto* n = doc->get(id);
		n->s.assign(s ? s : "", len);
		return id;
	} catch (...) {
		return 0;
	}
}

extern "C" CODA_FFI_EXPORT coda_node_t coda_new_block(coda_doc_t* doc) {
	if (!doc) return 0;
	return doc->new_node(coda_doc::Kind::Block);
}

extern "C" CODA_FFI_EXPORT coda_node_t coda_new_array(coda_doc_t* doc) {
	if (!doc) return 0;
	return doc->new_node(coda_doc::Kind::Array);
}

extern "C" CODA_FFI_EXPORT coda_node_t coda_new_table(coda_doc_t* doc) {
	if (!doc) return 0;
	return doc->new_node(coda_doc::Kind::Table);
}
