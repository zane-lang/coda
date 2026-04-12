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

// ─── Internal DOM ─────────────────────────────────────────────────────────────
//
// The DOM is a flat node arena indexed by uint32_t handles.
// nodes[0] is permanently reserved as the null/invalid node.
//
// Node kinds map 1-to-1 onto the new AST types:
//
//   FILE / BLOCK   → entries (OrderedMap<string, node_id>)
//   ARRAY          → arr (vector<node_id>)
//   TABLE          → cols (column names) + arr (row node_ids, in order)
//   KEYED_TABLE    → cols (column names) + entries (key → row node_id)
//   ROW            → fields (OrderedMap<string, string>)  ← flat strings, no sub-nodes
//   STRING         → s

struct coda_doc {
	enum class Kind : uint8_t {
		Null        = 0,
		File        = 1,
		String      = 2,
		Block       = 3,
		Array       = 4,
		Table       = 5,
		KeyedTable  = 6,
		Row         = 7,
	};

	struct Node {
		Kind kind = Kind::Null;

		// Comments
		std::string comment;
		std::string header_comment;  // TABLE / KEYED_TABLE / ARRAY

		// STRING
		std::string s;

		// FILE / BLOCK: ordered key→node map
		// KEYED_TABLE:  ordered key→row-node map
		std::vector<std::pair<std::string, uint32_t>> entries;
		std::unordered_map<std::string, size_t>       index;   // key → entries index

		// ARRAY / TABLE: ordered list of child node IDs
		std::vector<uint32_t> arr;

		// TABLE / KEYED_TABLE: column names (in declaration order)
		std::vector<std::string> cols;

		// ROW: flat column→value map (strings, not sub-nodes)
		std::vector<std::pair<std::string, std::string>> fields;
		std::unordered_map<std::string, size_t>          field_index;
	};

	std::vector<Node> nodes;
	uint32_t root = 0;

	coda_doc() {
		nodes.emplace_back();            // nodes[0] = null sentinel
		root = new_node(Kind::File);     // nodes[1] = root FILE
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

// ─── Small helpers ────────────────────────────────────────────────────────────

static inline coda_str_t view_of(const std::string& s) {
	return { s.data(), s.size() };
}

static const std::string g_empty;

static inline coda_owned_str_t owned_from_std(std::string s) {
	coda_owned_str_t out{ nullptr, 0 };
	out.len = s.size();
	out.ptr = static_cast<char*>(std::malloc(out.len + 1));
	if (!out.ptr) return { nullptr, 0 };
	std::memcpy(out.ptr, s.data(), out.len);
	out.ptr[out.len] = '\0';
	return out;
}

static inline bool is_map_kind(const coda_doc::Node* n) {
	return n && (n->kind == coda_doc::Kind::File ||
	             n->kind == coda_doc::Kind::Block);
}

// ─── AST → DOM (intern) ───────────────────────────────────────────────────────

// Forward declaration — intern_value and intern_block are mutually recursive.
static uint32_t intern_value(coda_doc& d, const coda::detail::Value& v);

static void intern_block_into(coda_doc& d, uint32_t id, const coda::Block& b) {
	for (const auto& [k, vp] : b.getContent()) {
		uint32_t child = intern_value(d, *vp);
		auto* n = d.get(id);          // re-fetch: intern_value may reallocate
		n->index[k] = n->entries.size();
		n->entries.emplace_back(k, child);
	}
}

static uint32_t intern_value(coda_doc& d, const coda::detail::Value& v) {
	using K = coda_doc::Kind;

	// ── String ──────────────────────────────────────────────────────────────
	if (!v.isContainer()) {
		uint32_t id = d.new_node(K::String);
		auto* n = d.get(id);
		n->s       = v.asString();
		n->comment = v.getComment();
		return id;
	}

	// Probe each container type in turn via the AST's as* accessors,
	// which throw on mismatch. Block?
	try {
		const coda::Block& b = v.asBlock();
		uint32_t id = d.new_node(K::Block);
		d.get(id)->comment = v.getComment();
		intern_block_into(d, id, b);
		return id;
	} catch (...) {}

	// Array?
	try {
		const coda::Array& a = v.asArray();
		uint32_t id = d.new_node(K::Array);
		d.get(id)->comment        = v.getComment();
		d.get(id)->header_comment = a.getHeaderComment();
		for (const auto& vp : a) {
			uint32_t child = intern_value(d, *vp);
			d.get(id)->arr.push_back(child);
		}
		return id;
	} catch (...) {}

	// Table?
	try {
		const coda::Table& t = v.asTable();
		uint32_t id = d.new_node(K::Table);
		d.get(id)->comment = v.getComment();
		d.get(id)->header_comment = t.getHeaderComment();

		// Build column list from the first row (if any)
		bool cols_built = false;
		for (const auto& row : t) {
			if (!cols_built) {
				for (const auto& [col, _] : row)
					d.get(id)->cols.push_back(col);
				cols_built = true;
			}
			// Intern the row
			uint32_t rid = d.new_node(coda_doc::Kind::Row);
			d.get(rid)->comment = row.getComment();
			for (const auto& [col, val] : row) {
				auto* rn = d.get(rid);
				rn->field_index[col] = rn->fields.size();
				rn->fields.emplace_back(col, val);
			}
			d.get(id)->arr.push_back(rid);
		}
		return id;
	} catch (...) {}

	// KeyedTable?
	try {
		const coda::KeyedTable& kt = v.asKeyedTable();
		uint32_t id = d.new_node(K::KeyedTable);
		d.get(id)->comment = v.getComment();
		d.get(id)->header_comment = kt.getHeaderComment();

		// Build column list from the first row
		bool cols_built = false;
		for (const auto& [rowKey, row] : kt) {
			if (!cols_built) {
				for (const auto& [col, _] : row)
					d.get(id)->cols.push_back(col);
				cols_built = true;
			}
			// Intern the row
			uint32_t rid = d.new_node(coda_doc::Kind::Row);
			d.get(rid)->comment = row.getComment();
			for (const auto& [col, val] : row) {
				auto* rn = d.get(rid);
				rn->field_index[col] = rn->fields.size();
				rn->fields.emplace_back(col, val);
			}
			// Insert into keyed table
			auto* ktn = d.get(id);
			ktn->index[rowKey] = ktn->entries.size();
			ktn->entries.emplace_back(rowKey, rid);
		}
		return id;
	} catch (...) {}

	// Fallback — should be unreachable with a well-formed AST
	uint32_t id = d.new_node(K::String);
	d.get(id)->s       = "";
	d.get(id)->comment = v.getComment();
	return id;
}

// ─── DOM → AST (emit) ─────────────────────────────────────────────────────────

static coda::detail::Value emit_value(const coda_doc& d, uint32_t id);

static coda::detail::Value emit_value(const coda_doc& d, uint32_t id) {
	const auto* n = d.get(id);
	if (!n) {
		coda::detail::Value v(std::string(""));
		return v;
	}

	switch (n->kind) {

		case coda_doc::Kind::String: {
			coda::detail::Value v(n->s);
			v.setComment(n->comment);
			return v;
		}

		case coda_doc::Kind::File:
		case coda_doc::Kind::Block: {
			coda::Block b;
			for (const auto& [k, child] : n->entries)
				b.getContent()[k] = std::make_unique<coda::detail::Value>(emit_value(d, child));
			coda::detail::Value v(std::move(b));
			v.setComment(n->comment);
			return v;
		}

		case coda_doc::Kind::Array: {
			coda::Array a;
			a.setHeaderComment(n->header_comment);
			for (uint32_t child : n->arr)
				a.append(emit_value(d, child));
			coda::detail::Value v(std::move(a));
			v.setComment(n->comment);
			return v;
		}

		case coda_doc::Kind::Table: {
			// Reconstruct a Table with headers derived from the stored col list.
			std::set<std::string> hdrs(n->cols.begin(), n->cols.end());
			coda::Table t(hdrs);
			t.setHeaderComment(n->header_comment);
			for (uint32_t rid : n->arr) {
				const auto* rn = d.get(rid);
				if (!rn || rn->kind != coda_doc::Kind::Row) continue;
				coda::Row row;
				row.setComment(rn->comment);
				for (const auto& [col, val] : rn->fields)
					row[col] = val;
				t.append(std::move(row));
			}
			coda::detail::Value v(std::move(t));
			v.setComment(n->comment);
			return v;
		}

		case coda_doc::Kind::KeyedTable: {
			std::set<std::string> hdrs(n->cols.begin(), n->cols.end());
			coda::KeyedTable kt(hdrs);
			kt.setHeaderComment(n->header_comment);
			for (const auto& [rowKey, rid] : n->entries) {
				const auto* rn = d.get(rid);
				if (!rn || rn->kind != coda_doc::Kind::Row) continue;
				coda::Row row;
				row.setComment(rn->comment);
				for (const auto& [col, val] : rn->fields)
					row[col] = val;
				kt.getContent()[rowKey] = std::move(row);
			}
			coda::detail::Value v(std::move(kt));
			v.setComment(n->comment);
			return v;
		}

		default: {
			coda::detail::Value v(std::string(""));
			v.setComment(n->comment);
			return v;
		}
	}
}

static coda::File emit_file(const coda_doc& d) {
	coda::File f;
	const auto* root = d.get(d.root);
	if (!root) return f;
	for (const auto& [k, child] : root->entries)
		f.getRoot().getContent()[k] =
			std::make_unique<coda::detail::Value>(emit_value(d, child));
	return f;
}

// ─── Utilities ────────────────────────────────────────────────────────────────

static void fill_parse_error(coda_error_t* err, const coda::ParseError& e) {
	if (!err) return;
	coda_error_clear(err);
	err->code   = static_cast<uint32_t>(e.code);
	err->line   = static_cast<uint32_t>(e.loc.line);
	err->col    = static_cast<uint32_t>(e.loc.col);
	err->offset = e.loc.offset;
	err->message = owned_from_std(std::string(e.what()));
}

static void rebuild_from_file(coda_doc& d, const coda::File& f) {
	d.nodes.clear();
	d.nodes.emplace_back();                         // null sentinel
	d.root = d.new_node(coda_doc::Kind::File);
	for (const auto& [k, vp] : f.getRoot().getContent()) {
		uint32_t child = intern_value(d, *vp);
		auto* root = d.get(d.root);
		root->index[k] = root->entries.size();
		root->entries.emplace_back(k, child);
	}
}

// ─── C API — memory ───────────────────────────────────────────────────────────

extern "C" CODA_FFI_EXPORT void coda_free(void* p) { std::free(p); }

extern "C" CODA_FFI_EXPORT void coda_owned_str_free(coda_owned_str_t s) { std::free(s.ptr); }

extern "C" CODA_FFI_EXPORT void coda_error_clear(coda_error_t* err) {
	if (!err) return;
	std::free(err->message.ptr);
	*err = {};
}

extern "C" CODA_FFI_EXPORT uint32_t coda_ffi_abi_version(void) { return 3; }

// ─── C API — doc lifecycle ────────────────────────────────────────────────────

extern "C" CODA_FFI_EXPORT coda_doc_t* coda_doc_new(void) {
	try { return new coda_doc(); } catch (...) { return nullptr; }
}

extern "C" CODA_FFI_EXPORT void coda_doc_free(coda_doc_t* doc) { delete doc; }

extern "C" CODA_FFI_EXPORT coda_doc_t* coda_doc_parse(
	const char* src, size_t len,
	const char* filename,
	coda_error_t* err
) {
	if (err) coda_error_clear(err);
	try {
		std::string text(src ? src : "", src ? len : 0);
		coda::detail::Parser p(std::move(text), filename ? filename : "");
		coda::File f = p.parse();

		auto* d = new coda_doc();
		rebuild_from_file(*d, f);
		return d;
	} catch (const coda::ParseError& e) {
		fill_parse_error(err, e);
		return nullptr;
	} catch (const std::exception& e) {
		if (err) err->message = owned_from_std(std::string("exception: ") + e.what());
		return nullptr;
	} catch (...) {
		if (err) err->message = owned_from_std("unknown exception");
		return nullptr;
	}
}

extern "C" CODA_FFI_EXPORT coda_doc_t* coda_doc_parse_file(
	const char* path, coda_error_t* err
) {
	if (err) coda_error_clear(err);
	if (!path) {
		if (err) err->message = owned_from_std("path is null");
		return nullptr;
	}
	try {
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

extern "C" CODA_FFI_EXPORT coda_doc_t* coda_doc_parse_fp(
	FILE* fp, const char* filename, coda_error_t* err
) {
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
			break;
		}
	}
	return coda_doc_parse(data.data(), data.size(), filename, err);
}

extern "C" CODA_FFI_EXPORT coda_owned_str_t coda_doc_serialize(
	const coda_doc_t* doc,
	const char* indent_unit, size_t indent_unit_len,
	coda_error_t* err
) {
	if (err) coda_error_clear(err);
	if (!doc) {
		if (err) err->message = owned_from_std("doc is null");
		return { nullptr, 0 };
	}
	try {
		std::string unit = indent_unit ? std::string(indent_unit, indent_unit_len) : "\t";
		coda::File f = emit_file(*doc);
		return owned_from_std(f.serialize(unit));
	} catch (const std::exception& e) {
		if (err) err->message = owned_from_std(std::string("exception: ") + e.what());
		return { nullptr, 0 };
	} catch (...) {
		if (err) err->message = owned_from_std("unknown exception");
		return { nullptr, 0 };
	}
}

extern "C" CODA_FFI_EXPORT void coda_doc_order(coda_doc_t* doc) {
	if (!doc) return;
	try {
		coda::File f = emit_file(*doc);
		f.order();
		rebuild_from_file(*doc, f);
	} catch (...) {}
}

extern "C" CODA_FFI_EXPORT void coda_doc_order_weighted(
	coda_doc_t* doc,
	const char** keys, const float* weights, size_t count
) {
	if (!doc) return;
	try {
		std::unordered_map<std::string, float> wmap;
		wmap.reserve(count);
		for (size_t i = 0; i < count; ++i)
			wmap[keys && keys[i] ? keys[i] : ""] = weights ? weights[i] : 0.0f;

		coda::File f = emit_file(*doc);
		f.order([&](const std::string& k) -> float {
			auto it = wmap.find(k);
			return it != wmap.end() ? it->second : 0.0f;
		});
		rebuild_from_file(*doc, f);
	} catch (...) {}
}

extern "C" CODA_FFI_EXPORT coda_node_t coda_doc_root(const coda_doc_t* doc) {
	return doc ? doc->root : 0;
}

// ─── C API — node-level serialize / order ────────────────────────────────────

// Sort entries alphabetically and rebuild the index for a BLOCK/FILE/KEYED_TABLE node.
static void sort_entries(coda_doc::Node* n) {
	std::sort(n->entries.begin(), n->entries.end(),
	          [](const auto& a, const auto& b) { return a.first < b.first; });
	n->index.clear();
	for (size_t i = 0; i < n->entries.size(); ++i)
		n->index[n->entries[i].first] = i;
}

static void dom_order_node(coda_doc& d, uint32_t id,
                           const std::function<float(const std::string&)>* wfn);

static void dom_order_node(coda_doc& d, uint32_t id,
                           const std::function<float(const std::string&)>* wfn) {
	auto* n = d.get(id);
	if (!n) return;

	switch (n->kind) {
		case coda_doc::Kind::File:
		case coda_doc::Kind::Block:
		case coda_doc::Kind::KeyedTable: {
			if (wfn) {
				std::stable_sort(n->entries.begin(), n->entries.end(),
				    [&](const auto& a, const auto& b) {
				        return (*wfn)(a.first) < (*wfn)(b.first);
				    });
			} else {
				sort_entries(n);
			}
			// Rebuild index after sort
			n->index.clear();
			for (size_t i = 0; i < n->entries.size(); ++i)
				n->index[n->entries[i].first] = i;
			// Recurse into children
			for (const auto& [k, child] : n->entries)
				dom_order_node(d, child, wfn);
			break;
		}
		case coda_doc::Kind::Array:
			for (uint32_t child : n->arr)
				dom_order_node(d, child, wfn);
			break;
		default:
			break;
	}
}

extern "C" CODA_FFI_EXPORT int coda_node_is_container(
	const coda_doc_t* doc, coda_node_t n
) {
	if (!doc) return 0;
	const auto* node = doc->get(n);
	if (!node) return 0;
	switch (node->kind) {
		case coda_doc::Kind::Block:
		case coda_doc::Kind::Array:
		case coda_doc::Kind::Table:
		case coda_doc::Kind::KeyedTable:
			return 1;
		default:
			return 0;
	}
}

extern "C" CODA_FFI_EXPORT coda_owned_str_t coda_node_serialize(
	const coda_doc_t* doc,
	coda_node_t       n,
	const char*       indent_unit,
	size_t            indent_unit_len,
	coda_error_t*     err
) {
	if (err) coda_error_clear(err);
	if (!doc) {
		if (err) err->message = owned_from_std("doc is null");
		return { nullptr, 0 };
	}
	try {
		std::string unit = indent_unit ? std::string(indent_unit, indent_unit_len) : "\t";
		const auto* node = doc->get(n);
		// FILE node — serialize the whole document
		if (!node || node->kind == coda_doc::Kind::File) {
			coda::File f = emit_file(*doc);
			return owned_from_std(f.serialize(unit));
		}
		coda::detail::Value v = emit_value(*doc, n);
		return owned_from_std(v.serializeInline(0, unit));
	} catch (const std::exception& e) {
		if (err) err->message = owned_from_std(std::string("exception: ") + e.what());
		return { nullptr, 0 };
	} catch (...) {
		if (err) err->message = owned_from_std("unknown exception");
		return { nullptr, 0 };
	}
}

extern "C" CODA_FFI_EXPORT void coda_node_order(coda_doc_t* doc, coda_node_t n) {
	if (!doc) return;
	dom_order_node(*doc, n, nullptr);
}

extern "C" CODA_FFI_EXPORT void coda_node_order_weighted(
	coda_doc_t*  doc,
	coda_node_t  n,
	const char** keys,
	const float* weights,
	size_t       count
) {
	if (!doc) return;
	std::unordered_map<std::string, float> wmap;
	wmap.reserve(count);
	for (size_t i = 0; i < count; ++i)
		wmap[keys && keys[i] ? keys[i] : ""] = weights ? weights[i] : 0.0f;
	auto wfn = std::function<float(const std::string&)>([&](const std::string& k) -> float {
		auto it = wmap.find(k);
		return it != wmap.end() ? it->second : 0.0f;
	});
	dom_order_node(*doc, n, &wfn);
}

// ─── C API — parse error code name ───────────────────────────────────────────

extern "C" CODA_FFI_EXPORT coda_str_t coda_parse_error_code_name(uint32_t code) {
	switch ((coda_parse_error_code_t)code) {
		case CODA_PARSE_UNEXPECTED_TOKEN:    return { "UnexpectedToken",    20 };
		case CODA_PARSE_UNEXPECTED_EOF:      return { "UnexpectedEOF",      13 };
		case CODA_PARSE_DUPLICATE_KEY:       return { "DuplicateKey",       12 };
		case CODA_PARSE_DUPLICATE_FIELD:     return { "DuplicateField",     14 };
		case CODA_PARSE_RAGGED_ROW:          return { "RaggedRow",           9 };
		case CODA_PARSE_INVALID_ESCAPE:      return { "InvalidEscape",      13 };
		case CODA_PARSE_UNTERMINATED_STRING: return { "UnterminatedString", 18 };
		case CODA_PARSE_NESTED_BLOCK:        return { "NestedBlock",        11 };
		case CODA_PARSE_CONTENT_AFTER_BRACE: return { "ContentAfterBrace",  17 };
		case CODA_PARSE_KEY_IN_BLOCK:        return { "KeyInBlock",         10 };
		default:                             return { "",                    0 };
	}
}

static_assert((uint32_t)coda::ParseErrorCode::UnexpectedToken == CODA_PARSE_UNEXPECTED_TOKEN);
static_assert((uint32_t)coda::ParseErrorCode::KeyInBlock      == CODA_PARSE_KEY_IN_BLOCK);

// ─── C API — node inspection ──────────────────────────────────────────────────

extern "C" CODA_FFI_EXPORT coda_node_kind_t coda_node_kind(
	const coda_doc_t* doc, coda_node_t n
) {
	if (!doc) return CODA_NODE_NULL;
	const auto* node = doc->get(n);
	if (!node) return CODA_NODE_NULL;
	switch (node->kind) {
		case coda_doc::Kind::File:       return CODA_NODE_FILE;
		case coda_doc::Kind::String:     return CODA_NODE_STRING;
		case coda_doc::Kind::Block:      return CODA_NODE_BLOCK;
		case coda_doc::Kind::Array:      return CODA_NODE_ARRAY;
		case coda_doc::Kind::Table:      return CODA_NODE_TABLE;
		case coda_doc::Kind::KeyedTable: return CODA_NODE_KEYED_TABLE;
		case coda_doc::Kind::Row:        return CODA_NODE_ROW;
		default:                         return CODA_NODE_NULL;
	}
}

extern "C" CODA_FFI_EXPORT coda_str_t coda_node_comment_get(
	const coda_doc_t* doc, coda_node_t n
) {
	if (!doc) return view_of(g_empty);
	const auto* node = doc->get(n);
	return node ? view_of(node->comment) : view_of(g_empty);
}

extern "C" CODA_FFI_EXPORT coda_status_t coda_node_comment_set(
	coda_doc_t* doc, coda_node_t n, const char* s, size_t len
) {
	if (!doc) return CODA_ERR;
	auto* node = doc->get(n);
	if (!node) return CODA_ERR;
	try { node->comment.assign(s ? s : "", len); return CODA_OK; }
	catch (...) { return CODA_ERR; }
}

extern "C" CODA_FFI_EXPORT coda_str_t coda_node_header_comment_get(
	const coda_doc_t* doc, coda_node_t n
) {
	if (!doc) return view_of(g_empty);
	const auto* node = doc->get(n);
	if (!node) return view_of(g_empty);
	if (node->kind != coda_doc::Kind::Array &&
	    node->kind != coda_doc::Kind::Table &&
	    node->kind != coda_doc::Kind::KeyedTable)
		return view_of(g_empty);
	return view_of(node->header_comment);
}

extern "C" CODA_FFI_EXPORT coda_status_t coda_node_header_comment_set(
	coda_doc_t* doc, coda_node_t n, const char* s, size_t len
) {
	if (!doc) return CODA_ERR;
	auto* node = doc->get(n);
	if (!node) return CODA_ERR;
	if (node->kind != coda_doc::Kind::Array &&
	    node->kind != coda_doc::Kind::Table &&
	    node->kind != coda_doc::Kind::KeyedTable)
		return CODA_BAD_KIND;
	try { node->header_comment.assign(s ? s : "", len); return CODA_OK; }
	catch (...) { return CODA_ERR; }
}

// ─── C API — string nodes ─────────────────────────────────────────────────────

extern "C" CODA_FFI_EXPORT coda_str_t coda_string_get(
	const coda_doc_t* doc, coda_node_t n
) {
	if (!doc) return view_of(g_empty);
	const auto* node = doc->get(n);
	if (!node || node->kind != coda_doc::Kind::String) return view_of(g_empty);
	return view_of(node->s);
}

extern "C" CODA_FFI_EXPORT coda_status_t coda_string_set(
	coda_doc_t* doc, coda_node_t n, const char* s, size_t len
) {
	if (!doc) return CODA_ERR;
	auto* node = doc->get(n);
	if (!node) return CODA_ERR;
	if (node->kind != coda_doc::Kind::String) return CODA_BAD_KIND;
	try { node->s.assign(s ? s : "", len); return CODA_OK; }
	catch (...) { return CODA_ERR; }
}

// ─── C API — array nodes ──────────────────────────────────────────────────────

extern "C" CODA_FFI_EXPORT size_t coda_array_len(
	const coda_doc_t* doc, coda_node_t a
) {
	if (!doc) return 0;
	const auto* n = doc->get(a);
	return (n && n->kind == coda_doc::Kind::Array) ? n->arr.size() : 0;
}

extern "C" CODA_FFI_EXPORT coda_node_t coda_array_get(
	const coda_doc_t* doc, coda_node_t a, size_t idx
) {
	if (!doc) return 0;
	const auto* n = doc->get(a);
	if (!n || n->kind != coda_doc::Kind::Array || idx >= n->arr.size()) return 0;
	return n->arr[idx];
}

extern "C" CODA_FFI_EXPORT coda_status_t coda_array_set(
	coda_doc_t* doc, coda_node_t a, size_t idx, coda_node_t value
) {
	if (!doc) return CODA_ERR;
	auto* n = doc->get(a);
	if (!n) return CODA_ERR;
	if (n->kind != coda_doc::Kind::Array) return CODA_BAD_KIND;
	if (idx >= n->arr.size()) return CODA_OUT_OF_RANGE;
	n->arr[idx] = value;
	return CODA_OK;
}

extern "C" CODA_FFI_EXPORT coda_status_t coda_array_push(
	coda_doc_t* doc, coda_node_t a, coda_node_t value
) {
	if (!doc) return CODA_ERR;
	auto* n = doc->get(a);
	if (!n) return CODA_ERR;
	if (n->kind != coda_doc::Kind::Array) return CODA_BAD_KIND;
	try { n->arr.push_back(value); return CODA_OK; }
	catch (...) { return CODA_ERR; }
}

extern "C" CODA_FFI_EXPORT coda_status_t coda_array_remove(
	coda_doc_t* doc, coda_node_t a, size_t idx
) {
	if (!doc) return CODA_ERR;
	auto* n = doc->get(a);
	if (!n) return CODA_ERR;
	if (n->kind != coda_doc::Kind::Array) return CODA_BAD_KIND;
	if (idx >= n->arr.size()) return CODA_OUT_OF_RANGE;
	n->arr.erase(n->arr.begin() + idx);
	return CODA_OK;
}

// ─── C API — map nodes (FILE / BLOCK) ────────────────────────────────────────

extern "C" CODA_FFI_EXPORT size_t coda_map_len(
	const coda_doc_t* doc, coda_node_t m
) {
	if (!doc) return 0;
	const auto* n = doc->get(m);
	return is_map_kind(n) ? n->entries.size() : 0;
}

extern "C" CODA_FFI_EXPORT coda_str_t coda_map_key_at(
	const coda_doc_t* doc, coda_node_t m, size_t idx
) {
	if (!doc) return view_of(g_empty);
	const auto* n = doc->get(m);
	if (!is_map_kind(n) || idx >= n->entries.size()) return view_of(g_empty);
	return view_of(n->entries[idx].first);
}

extern "C" CODA_FFI_EXPORT coda_node_t coda_map_value_at(
	const coda_doc_t* doc, coda_node_t m, size_t idx
) {
	if (!doc) return 0;
	const auto* n = doc->get(m);
	if (!is_map_kind(n) || idx >= n->entries.size()) return 0;
	return n->entries[idx].second;
}

extern "C" CODA_FFI_EXPORT coda_node_t coda_map_get(
	const coda_doc_t* doc, coda_node_t m, const char* key, size_t key_len
) {
	if (!doc) return 0;
	const auto* n = doc->get(m);
	if (!is_map_kind(n)) return 0;
	std::string k(key ? key : "", key_len);
	auto it = n->index.find(k);
	return it != n->index.end() ? n->entries[it->second].second : 0;
}

extern "C" CODA_FFI_EXPORT coda_node_t coda_map_get_or_insert(
	coda_doc_t* doc, coda_node_t m, const char* key, size_t key_len
) {
	if (!doc) return 0;
	auto* n = doc->get(m);
	if (!is_map_kind(n)) return 0;
	std::string k(key ? key : "", key_len);
	auto it = n->index.find(k);
	if (it != n->index.end()) return n->entries[it->second].second;

	uint32_t child = doc->new_node(coda_doc::Kind::String);
	// Re-fetch n: new_node may have reallocated nodes vector
	n = doc->get(m);
	n->index[k] = n->entries.size();
	n->entries.emplace_back(std::move(k), child);
	return child;
}

extern "C" CODA_FFI_EXPORT coda_status_t coda_map_set(
	coda_doc_t* doc, coda_node_t m,
	const char* key, size_t key_len, coda_node_t value
) {
	if (!doc) return CODA_ERR;
	auto* n = doc->get(m);
	if (!is_map_kind(n)) return CODA_BAD_KIND;
	try {
		std::string k(key ? key : "", key_len);
		auto it = n->index.find(k);
		if (it == n->index.end()) {
			n->index[k] = n->entries.size();
			n->entries.emplace_back(std::move(k), value);
		} else {
			n->entries[it->second].second = value;
		}
		return CODA_OK;
	} catch (...) { return CODA_ERR; }
}

extern "C" CODA_FFI_EXPORT coda_status_t coda_map_remove(
	coda_doc_t* doc, coda_node_t m, const char* key, size_t key_len
) {
	if (!doc) return CODA_ERR;
	auto* n = doc->get(m);
	if (!is_map_kind(n)) return CODA_BAD_KIND;
	std::string k(key ? key : "", key_len);
	auto it = n->index.find(k);
	if (it == n->index.end()) return CODA_NOT_FOUND;
	size_t idx = it->second;
	n->entries.erase(n->entries.begin() + idx);
	n->index.erase(it);
	for (size_t i = idx; i < n->entries.size(); ++i)
		n->index[n->entries[i].first] = i;
	return CODA_OK;
}

// ─── C API — plain table (TABLE) ─────────────────────────────────────────────

extern "C" CODA_FFI_EXPORT size_t coda_table_col_count(
	const coda_doc_t* doc, coda_node_t t
) {
	if (!doc) return 0;
	const auto* n = doc->get(t);
	return (n && n->kind == coda_doc::Kind::Table) ? n->cols.size() : 0;
}

extern "C" CODA_FFI_EXPORT coda_str_t coda_table_col_name(
	const coda_doc_t* doc, coda_node_t t, size_t col_idx
) {
	if (!doc) return view_of(g_empty);
	const auto* n = doc->get(t);
	if (!n || n->kind != coda_doc::Kind::Table || col_idx >= n->cols.size())
		return view_of(g_empty);
	return view_of(n->cols[col_idx]);
}

extern "C" CODA_FFI_EXPORT coda_status_t coda_table_col_append(
	coda_doc_t* doc, coda_node_t t, const char* name, size_t name_len
) {
	if (!doc) return CODA_ERR;
	auto* n = doc->get(t);
	if (!n) return CODA_ERR;
	if (n->kind != coda_doc::Kind::Table) return CODA_BAD_KIND;
	try { n->cols.emplace_back(name ? name : "", name_len); return CODA_OK; }
	catch (...) { return CODA_ERR; }
}

extern "C" CODA_FFI_EXPORT size_t coda_table_row_count(
	const coda_doc_t* doc, coda_node_t t
) {
	if (!doc) return 0;
	const auto* n = doc->get(t);
	return (n && n->kind == coda_doc::Kind::Table) ? n->arr.size() : 0;
}

extern "C" CODA_FFI_EXPORT coda_node_t coda_table_row_at(
	const coda_doc_t* doc, coda_node_t t, size_t row_idx
) {
	if (!doc) return 0;
	const auto* n = doc->get(t);
	if (!n || n->kind != coda_doc::Kind::Table || row_idx >= n->arr.size()) return 0;
	return n->arr[row_idx];
}

extern "C" CODA_FFI_EXPORT coda_status_t coda_table_row_append(
	coda_doc_t* doc, coda_node_t t, coda_node_t row
) {
	if (!doc) return CODA_ERR;
	auto* n = doc->get(t);
	if (!n) return CODA_ERR;
	if (n->kind != coda_doc::Kind::Table) return CODA_BAD_KIND;
	const auto* rn = doc->get(row);
	if (!rn || rn->kind != coda_doc::Kind::Row) return CODA_BAD_KIND;
	try { n->arr.push_back(row); return CODA_OK; }
	catch (...) { return CODA_ERR; }
}

extern "C" CODA_FFI_EXPORT coda_status_t coda_table_row_set(
	coda_doc_t* doc, coda_node_t t, size_t row_idx, coda_node_t row
) {
	if (!doc) return CODA_ERR;
	auto* n = doc->get(t);
	if (!n) return CODA_ERR;
	if (n->kind != coda_doc::Kind::Table) return CODA_BAD_KIND;
	if (row_idx >= n->arr.size()) return CODA_OUT_OF_RANGE;
	const auto* rn = doc->get(row);
	if (!rn || rn->kind != coda_doc::Kind::Row) return CODA_BAD_KIND;
	n->arr[row_idx] = row;
	return CODA_OK;
}

extern "C" CODA_FFI_EXPORT coda_status_t coda_table_row_remove(
	coda_doc_t* doc, coda_node_t t, size_t row_idx
) {
	if (!doc) return CODA_ERR;
	auto* n = doc->get(t);
	if (!n) return CODA_ERR;
	if (n->kind != coda_doc::Kind::Table) return CODA_BAD_KIND;
	if (row_idx >= n->arr.size()) return CODA_OUT_OF_RANGE;
	n->arr.erase(n->arr.begin() + row_idx);
	return CODA_OK;
}

// ─── C API — keyed table (KEYED_TABLE) ───────────────────────────────────────

extern "C" CODA_FFI_EXPORT size_t coda_keyed_table_col_count(
	const coda_doc_t* doc, coda_node_t kt
) {
	if (!doc) return 0;
	const auto* n = doc->get(kt);
	return (n && n->kind == coda_doc::Kind::KeyedTable) ? n->cols.size() : 0;
}

extern "C" CODA_FFI_EXPORT coda_str_t coda_keyed_table_col_name(
	const coda_doc_t* doc, coda_node_t kt, size_t col_idx
) {
	if (!doc) return view_of(g_empty);
	const auto* n = doc->get(kt);
	if (!n || n->kind != coda_doc::Kind::KeyedTable || col_idx >= n->cols.size())
		return view_of(g_empty);
	return view_of(n->cols[col_idx]);
}

extern "C" CODA_FFI_EXPORT coda_status_t coda_keyed_table_col_append(
	coda_doc_t* doc, coda_node_t kt, const char* name, size_t name_len
) {
	if (!doc) return CODA_ERR;
	auto* n = doc->get(kt);
	if (!n) return CODA_ERR;
	if (n->kind != coda_doc::Kind::KeyedTable) return CODA_BAD_KIND;
	try { n->cols.emplace_back(name ? name : "", name_len); return CODA_OK; }
	catch (...) { return CODA_ERR; }
}

extern "C" CODA_FFI_EXPORT size_t coda_keyed_table_row_count(
	const coda_doc_t* doc, coda_node_t kt
) {
	if (!doc) return 0;
	const auto* n = doc->get(kt);
	return (n && n->kind == coda_doc::Kind::KeyedTable) ? n->entries.size() : 0;
}

extern "C" CODA_FFI_EXPORT coda_str_t coda_keyed_table_row_key_at(
	const coda_doc_t* doc, coda_node_t kt, size_t row_idx
) {
	if (!doc) return view_of(g_empty);
	const auto* n = doc->get(kt);
	if (!n || n->kind != coda_doc::Kind::KeyedTable || row_idx >= n->entries.size())
		return view_of(g_empty);
	return view_of(n->entries[row_idx].first);
}

extern "C" CODA_FFI_EXPORT coda_node_t coda_keyed_table_row_at(
	const coda_doc_t* doc, coda_node_t kt, size_t row_idx
) {
	if (!doc) return 0;
	const auto* n = doc->get(kt);
	if (!n || n->kind != coda_doc::Kind::KeyedTable || row_idx >= n->entries.size()) return 0;
	return n->entries[row_idx].second;
}

extern "C" CODA_FFI_EXPORT coda_node_t coda_keyed_table_row_get(
	const coda_doc_t* doc, coda_node_t kt, const char* key, size_t key_len
) {
	if (!doc) return 0;
	const auto* n = doc->get(kt);
	if (!n || n->kind != coda_doc::Kind::KeyedTable) return 0;
	std::string k(key ? key : "", key_len);
	auto it = n->index.find(k);
	return it != n->index.end() ? n->entries[it->second].second : 0;
}

extern "C" CODA_FFI_EXPORT coda_status_t coda_keyed_table_row_set(
	coda_doc_t* doc, coda_node_t kt,
	const char* key, size_t key_len, coda_node_t row
) {
	if (!doc) return CODA_ERR;
	auto* n = doc->get(kt);
	if (!n) return CODA_ERR;
	if (n->kind != coda_doc::Kind::KeyedTable) return CODA_BAD_KIND;
	const auto* rn = doc->get(row);
	if (!rn || rn->kind != coda_doc::Kind::Row) return CODA_BAD_KIND;
	try {
		std::string k(key ? key : "", key_len);
		auto it = n->index.find(k);
		if (it == n->index.end()) {
			n->index[k] = n->entries.size();
			n->entries.emplace_back(std::move(k), row);
		} else {
			n->entries[it->second].second = row;
		}
		return CODA_OK;
	} catch (...) { return CODA_ERR; }
}

extern "C" CODA_FFI_EXPORT coda_status_t coda_keyed_table_row_remove(
	coda_doc_t* doc, coda_node_t kt, const char* key, size_t key_len
) {
	if (!doc) return CODA_ERR;
	auto* n = doc->get(kt);
	if (!n) return CODA_ERR;
	if (n->kind != coda_doc::Kind::KeyedTable) return CODA_BAD_KIND;
	std::string k(key ? key : "", key_len);
	auto it = n->index.find(k);
	if (it == n->index.end()) return CODA_NOT_FOUND;
	size_t idx = it->second;
	n->entries.erase(n->entries.begin() + idx);
	n->index.erase(it);
	for (size_t i = idx; i < n->entries.size(); ++i)
		n->index[n->entries[i].first] = i;
	return CODA_OK;
}

// ─── C API — row nodes ────────────────────────────────────────────────────────

extern "C" CODA_FFI_EXPORT coda_str_t coda_row_get(
	const coda_doc_t* doc, coda_node_t row, const char* col, size_t col_len
) {
	if (!doc) return view_of(g_empty);
	const auto* n = doc->get(row);
	if (!n || n->kind != coda_doc::Kind::Row) return view_of(g_empty);
	std::string c(col ? col : "", col_len);
	auto it = n->field_index.find(c);
	return it != n->field_index.end() ? view_of(n->fields[it->second].second) : view_of(g_empty);
}

extern "C" CODA_FFI_EXPORT coda_status_t coda_row_set(
	coda_doc_t* doc, coda_node_t row,
	const char* col, size_t col_len,
	const char* val, size_t val_len
) {
	if (!doc) return CODA_ERR;
	auto* n = doc->get(row);
	if (!n) return CODA_ERR;
	if (n->kind != coda_doc::Kind::Row) return CODA_BAD_KIND;
	try {
		std::string c(col ? col : "", col_len);
		std::string v(val ? val : "", val_len);
		auto it = n->field_index.find(c);
		if (it == n->field_index.end()) {
			n->field_index[c] = n->fields.size();
			n->fields.emplace_back(std::move(c), std::move(v));
		} else {
			n->fields[it->second].second = std::move(v);
		}
		return CODA_OK;
	} catch (...) { return CODA_ERR; }
}

extern "C" CODA_FFI_EXPORT coda_status_t coda_row_remove(
	coda_doc_t* doc, coda_node_t row, const char* col, size_t col_len
) {
	if (!doc) return CODA_ERR;
	auto* n = doc->get(row);
	if (!n) return CODA_ERR;
	if (n->kind != coda_doc::Kind::Row) return CODA_BAD_KIND;
	std::string c(col ? col : "", col_len);
	auto it = n->field_index.find(c);
	if (it == n->field_index.end()) return CODA_NOT_FOUND;
	size_t idx = it->second;
	n->fields.erase(n->fields.begin() + idx);
	n->field_index.erase(it);
	for (size_t i = idx; i < n->fields.size(); ++i)
		n->field_index[n->fields[i].first] = i;
	return CODA_OK;
}

extern "C" CODA_FFI_EXPORT size_t coda_row_col_count(
	const coda_doc_t* doc, coda_node_t row
) {
	if (!doc) return 0;
	const auto* n = doc->get(row);
	return (n && n->kind == coda_doc::Kind::Row) ? n->fields.size() : 0;
}

extern "C" CODA_FFI_EXPORT coda_str_t coda_row_col_name_at(
	const coda_doc_t* doc, coda_node_t row, size_t idx
) {
	if (!doc) return view_of(g_empty);
	const auto* n = doc->get(row);
	if (!n || n->kind != coda_doc::Kind::Row || idx >= n->fields.size())
		return view_of(g_empty);
	return view_of(n->fields[idx].first);
}

extern "C" CODA_FFI_EXPORT coda_str_t coda_row_col_value_at(
	const coda_doc_t* doc, coda_node_t row, size_t idx
) {
	if (!doc) return view_of(g_empty);
	const auto* n = doc->get(row);
	if (!n || n->kind != coda_doc::Kind::Row || idx >= n->fields.size())
		return view_of(g_empty);
	return view_of(n->fields[idx].second);
}

extern "C" CODA_FFI_EXPORT coda_str_t coda_row_comment_get(
	const coda_doc_t* doc, coda_node_t row
) {
	if (!doc) return view_of(g_empty);
	const auto* n = doc->get(row);
	if (!n || n->kind != coda_doc::Kind::Row) return view_of(g_empty);
	return view_of(n->comment);
}

extern "C" CODA_FFI_EXPORT coda_status_t coda_row_comment_set(
	coda_doc_t* doc, coda_node_t row, const char* s, size_t len
) {
	if (!doc) return CODA_ERR;
	auto* n = doc->get(row);
	if (!n) return CODA_ERR;
	if (n->kind != coda_doc::Kind::Row) return CODA_BAD_KIND;
	try { n->comment.assign(s ? s : "", len); return CODA_OK; }
	catch (...) { return CODA_ERR; }
}

// ─── C API — node creation ────────────────────────────────────────────────────

extern "C" CODA_FFI_EXPORT coda_node_t coda_new_string(
	coda_doc_t* doc, const char* s, size_t len
) {
	if (!doc) return 0;
	try {
		uint32_t id = doc->new_node(coda_doc::Kind::String);
		doc->get(id)->s.assign(s ? s : "", len);
		return id;
	} catch (...) { return 0; }
}

extern "C" CODA_FFI_EXPORT coda_node_t coda_new_block(coda_doc_t* doc) {
	return doc ? doc->new_node(coda_doc::Kind::Block) : 0;
}

extern "C" CODA_FFI_EXPORT coda_node_t coda_new_array(coda_doc_t* doc) {
	return doc ? doc->new_node(coda_doc::Kind::Array) : 0;
}

extern "C" CODA_FFI_EXPORT coda_node_t coda_new_table(coda_doc_t* doc) {
	return doc ? doc->new_node(coda_doc::Kind::Table) : 0;
}

extern "C" CODA_FFI_EXPORT coda_node_t coda_new_keyed_table(coda_doc_t* doc) {
	return doc ? doc->new_node(coda_doc::Kind::KeyedTable) : 0;
}

extern "C" CODA_FFI_EXPORT coda_node_t coda_new_row(coda_doc_t* doc) {
	return doc ? doc->new_node(coda_doc::Kind::Row) : 0;
}
