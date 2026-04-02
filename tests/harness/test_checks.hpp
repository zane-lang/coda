#pragma once

#include "test_data.hpp"
#include "../../include/coda.hpp"

#include <functional>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace test_framework {

// ─── Abstract parse interface ─────────────────────────────────────────────────

struct ParseAdapter {
	// Parse source, store result. Returns true on success.
	virtual bool parse(const char* src) = 0;

	// Returns true if parsing src throws/fails.
	virtual bool parse_fails(const char* src) = 0;

	// Returns true if parsing src fails and the error message contains
	// at least one of the needles.
	virtual bool parse_fails_with_msg(const char* src,
	                                  const std::vector<std::string>& needles) = 0;

	// Returns true if parsing src fails with the given error code.
	virtual bool parse_fails_with_code(const char* src, int code) = 0;

	// ── Scalar access ─────────────────────────────────────────────────────
	virtual std::string get_string(const char* key) = 0;
	virtual std::string get_string_path(const std::vector<std::string>& keys) = 0;

	// ── Structure queries ─────────────────────────────────────────────────
	virtual bool has_key(const char* key) = 0;
	virtual bool is_container(const char* key) = 0;
	virtual size_t get_map_len(const char* key) = 0;
	virtual std::vector<std::string> get_map_keys(const char* key) = 0;
	virtual int get_node_kind(const char* key) = 0;

	// ── Array access ──────────────────────────────────────────────────────
	virtual size_t get_array_len(const char* key) = 0;
	virtual std::string get_array_element(const char* key, size_t idx) = 0;
	virtual bool array_index_throws(const char* key, size_t idx) = 0;

	// ── Nested array element → block field ────────────────────────────────
	virtual std::string get_array_block_field(const char* array_key, size_t idx,
	                                          const char* field) = 0;
	virtual size_t get_array_block_count(const char* array_key) = 0;

	// ── Table access (keyed & plain) ──────────────────────────────────────
	virtual std::string get_table_cell(const char* table, const char* row,
	                                   const char* col) = 0;
	virtual std::vector<std::string> get_table_row_keys(const char* table) = 0;
	virtual bool table_row_missing_throws(const char* table, const char* row) = 0;
	virtual bool table_row_missing_inserts(const char* table, const char* row) = 0;

	// ── Plain table (array of row-blocks) ─────────────────────────────────
	virtual std::string get_plain_table_cell(const char* table, size_t row,
	                                         const char* col) = 0;

	// ── Comments ──────────────────────────────────────────────────────────
	virtual std::string get_comment(const char* key) = 0;
	virtual std::string get_header_comment(const char* key) = 0;
	virtual std::string get_comment_path(const std::vector<std::string>& keys) = 0;
	virtual std::string get_array_element_comment(const char* key, size_t idx) = 0;
	virtual std::string get_table_row_comment(const char* table, const char* row) = 0;
	virtual std::string get_plain_table_row_comment(const char* table, size_t row) = 0;

	// ── Mutation ──────────────────────────────────────────────────────────
	virtual bool set_string(const char* key, const char* value) = 0;
	virtual bool set_string_path(const std::vector<std::string>& keys,
	                             const char* value) = 0;

	// ── Serialization ─────────────────────────────────────────────────────
	virtual std::string serialize(const char* indent = "\t") = 0;
	virtual bool roundtrip_stable(const char* src) = 0;

	// ── Ordering ──────────────────────────────────────────────────────────
	virtual std::string order_default_and_serialize() = 0;
	virtual std::string order_weighted_and_serialize(
		const std::vector<std::pair<std::string, float>>& weights) = 0;

	// ── Type error queries ────────────────────────────────────────────────
	virtual bool string_index_on_scalar_throws(const char* key, const char* sub) = 0;
	virtual bool int_index_on_block_throws(const char* key, size_t idx) = 0;
	virtual bool as_array_on_scalar_throws(const char* key) = 0;
	virtual bool as_block_on_array_throws(const char* key) = 0;
	virtual bool as_table_on_block_throws(const char* key) = 0;
	virtual bool const_missing_key_throws(const char* key) = 0;

	virtual ~ParseAdapter() = default;
};

// ─── Test entry ───────────────────────────────────────────────────────────────

struct TestEntry {
	std::string suite;
	std::string name;
	std::function<bool(ParseAdapter&)> run;  // returns true = pass
};

using TestCatalog = std::vector<TestEntry>;

// ─── Build the catalog ───────────────────────────────────────────────────────

namespace {

inline std::string read_file(const std::string& path) {
	std::ifstream f(path, std::ios::binary);
	if (!f) return {};
	std::ostringstream ss;
	ss << f.rdbuf();
	return ss.str();
}

inline const coda::CodaValue& block_at(
	const coda::CodaValue& v, const std::string& key) {
	return v.asBlock().content.at(key);
}

inline bool block_has(const coda::CodaValue& v, const std::string& key) {
	return v.asBlock().content.count(key) > 0;
}

inline std::string block_string(
	const coda::CodaValue& v, const std::string& key) {
	const auto& b = v.asBlock().content;
	if (b.count(key) > 0) return b.at(key).asString();
	if (b.count("field") > 0) return b.at("field").asString();
	return "";
}

inline std::vector<std::string> block_string_list(
	const coda::CodaValue& v, const std::string& key) {
	std::vector<std::string> out;
	for (const auto& item : block_at(v, key).asArray().content)
		out.push_back(item.asString());
	return out;
}

inline std::vector<std::string> value_string_list(const coda::CodaValue& v) {
	std::vector<std::string> out;
	for (const auto& item : v.asArray().content)
		out.push_back(item.asString());
	return out;
}

inline bool parse_bool(const std::string& v) {
	return v == "true" || v == "1" || v == "yes";
}

inline int parse_int(const std::string& v) {
	return std::stoi(v);
}

inline float parse_float(const std::string& v) {
	return std::stof(v);
}

inline bool check_order_contains(const std::string& s,
	const std::vector<std::string>& order) {
	if (order.empty()) return true;
	std::size_t pos = 0;
	for (const auto& needle : order) {
		std::size_t found = s.find(needle, pos);
		if (found == std::string::npos) return false;
		pos = found + needle.size();
	}
	return true;
}

inline bool run_check(ParseAdapter& p, const coda::CodaValue& check) {
	const std::string op = block_string(check, "op");

	if (op == "get_string") {
		return p.get_string(block_string(check, "field").c_str())
			== block_string(check, "eq");
	}
	if (op == "get_string_path") {
		auto path = block_string_list(check, "path");
		return p.get_string_path(path) == block_string(check, "eq");
	}
	if (op == "is_container") {
		bool got = p.is_container(block_string(check, "field").c_str());
		return got == parse_bool(block_string(check, "eq_bool"));
	}
	if (op == "has_key") {
		bool got = p.has_key(block_string(check, "field").c_str());
		return got == parse_bool(block_string(check, "eq_bool"));
	}
	if (op == "map_len") {
		return static_cast<int>(p.get_map_len(block_string(check, "field").c_str()))
			== parse_int(block_string(check, "eq_int"));
	}
	if (op == "map_keys") {
		auto got = p.get_map_keys(block_string(check, "field").c_str());
		return got == value_string_list(block_at(check, "eq_list"));
	}
	if (op == "array_len") {
		return static_cast<int>(p.get_array_len(block_string(check, "field").c_str()))
			== parse_int(block_string(check, "eq_int"));
	}
	if (op == "array_element") {
		return p.get_array_element(block_string(check, "field").c_str(),
			static_cast<size_t>(parse_int(block_string(check, "idx"))))
			== block_string(check, "eq");
	}
	if (op == "array_block_count") {
		return static_cast<int>(p.get_array_block_count(block_string(check, "field").c_str()))
			== parse_int(block_string(check, "eq_int"));
	}
	if (op == "array_block_field") {
		return p.get_array_block_field(
			block_string(check, "field").c_str(),
			static_cast<size_t>(parse_int(block_string(check, "idx"))),
			block_string(check, "field_name").c_str())
			== block_string(check, "eq");
	}
	if (op == "array_index_throws") {
		bool got = p.array_index_throws(
			block_string(check, "field").c_str(),
			static_cast<size_t>(parse_int(block_string(check, "idx"))));
		return got == parse_bool(block_string(check, "eq_bool"));
	}
	if (op == "plain_table_cell") {
		return p.get_plain_table_cell(
			block_string(check, "table").c_str(),
			static_cast<size_t>(parse_int(block_string(check, "idx"))),
			block_string(check, "col").c_str())
			== block_string(check, "eq");
	}
	if (op == "table_cell") {
		return p.get_table_cell(
			block_string(check, "table").c_str(),
			block_string(check, "row").c_str(),
			block_string(check, "col").c_str())
			== block_string(check, "eq");
	}
	if (op == "table_row_keys") {
		auto got = p.get_table_row_keys(block_string(check, "table").c_str());
		return got == value_string_list(block_at(check, "eq_list"));
	}
	if (op == "table_row_missing_inserts") {
		bool got = p.table_row_missing_inserts(
			block_string(check, "table").c_str(),
			block_string(check, "row").c_str());
		return got == parse_bool(block_string(check, "eq_bool"));
	}
	if (op == "table_row_missing_throws") {
		bool got = p.table_row_missing_throws(
			block_string(check, "table").c_str(),
			block_string(check, "row").c_str());
		return got == parse_bool(block_string(check, "eq_bool"));
	}
	if (op == "comment") {
		return p.get_comment(block_string(check, "field").c_str())
			== block_string(check, "eq");
	}
	if (op == "header_comment") {
		return p.get_header_comment(block_string(check, "field").c_str())
			== block_string(check, "eq");
	}
	if (op == "comment_path") {
		auto path = block_string_list(check, "path");
		return p.get_comment_path(path) == block_string(check, "eq");
	}
	if (op == "array_element_comment") {
		return p.get_array_element_comment(
			block_string(check, "field").c_str(),
			static_cast<size_t>(parse_int(block_string(check, "idx"))))
			== block_string(check, "eq");
	}
	if (op == "table_row_comment") {
		return p.get_table_row_comment(
			block_string(check, "table").c_str(),
			block_string(check, "row").c_str())
			== block_string(check, "eq");
	}
	if (op == "plain_table_row_comment") {
		return p.get_plain_table_row_comment(
			block_string(check, "table").c_str(),
			static_cast<size_t>(parse_int(block_string(check, "idx"))))
			== block_string(check, "eq");
	}
	if (op == "set_string") {
		bool ok = p.set_string(
			block_string(check, "field").c_str(),
			block_string(check, "value").c_str());
		return ok == parse_bool(block_string(check, "eq_bool"));
	}
	if (op == "set_string_path") {
		auto path = block_string_list(check, "path");
		bool ok = p.set_string_path(path, block_string(check, "value").c_str());
		return ok == parse_bool(block_string(check, "eq_bool"));
	}
	if (op == "string_index_on_scalar_throws") {
		bool got = p.string_index_on_scalar_throws(
			block_string(check, "field").c_str(),
			block_string(check, "sub").c_str());
		return got == parse_bool(block_string(check, "eq_bool"));
	}
	if (op == "int_index_on_block_throws") {
		bool got = p.int_index_on_block_throws(
			block_string(check, "field").c_str(),
			static_cast<size_t>(parse_int(block_string(check, "idx"))));
		return got == parse_bool(block_string(check, "eq_bool"));
	}
	if (op == "as_array_on_scalar_throws") {
		bool got = p.as_array_on_scalar_throws(block_string(check, "field").c_str());
		return got == parse_bool(block_string(check, "eq_bool"));
	}
	if (op == "as_block_on_array_throws") {
		bool got = p.as_block_on_array_throws(block_string(check, "field").c_str());
		return got == parse_bool(block_string(check, "eq_bool"));
	}
	if (op == "as_table_on_block_throws") {
		bool got = p.as_table_on_block_throws(block_string(check, "field").c_str());
		return got == parse_bool(block_string(check, "eq_bool"));
	}
	if (op == "const_missing_key_throws") {
		bool got = p.const_missing_key_throws(block_string(check, "field").c_str());
		return got == parse_bool(block_string(check, "eq_bool"));
	}
	if (op == "order_default_contains_order") {
		auto order = value_string_list(block_at(check, "order"));
		return check_order_contains(p.order_default_and_serialize(), order);
	}
	if (op == "order_weighted_contains_order") {
		auto order = value_string_list(block_at(check, "order"));
		std::vector<std::pair<std::string, float>> weights;
		for (const auto& entry : block_at(check, "weights").asArray().content) {
			weights.emplace_back(
				block_string(entry, "field"),
				parse_float(block_string(entry, "weight")));
		}
		std::string s = p.order_weighted_and_serialize(weights);
		return check_order_contains(s, order);
	}
	if (op == "serialize_contains") {
		std::string s = p.serialize(block_string(check, "indent").c_str());
		return s.find(block_string(check, "contains")) != std::string::npos;
	}

	return false;
}

inline int parse_error_code(const std::string& code) {
	if (code == "UnexpectedToken") return static_cast<int>(coda::ParseErrorCode::UnexpectedToken);
	if (code == "UnexpectedEOF") return static_cast<int>(coda::ParseErrorCode::UnexpectedEOF);
	if (code == "DuplicateKey") return static_cast<int>(coda::ParseErrorCode::DuplicateKey);
	if (code == "DuplicateField") return static_cast<int>(coda::ParseErrorCode::DuplicateField);
	if (code == "RaggedRow") return static_cast<int>(coda::ParseErrorCode::RaggedRow);
	if (code == "InvalidEscape") return static_cast<int>(coda::ParseErrorCode::InvalidEscape);
	if (code == "UnterminatedString") return static_cast<int>(coda::ParseErrorCode::UnterminatedString);
	if (code == "NestedBlock") return static_cast<int>(coda::ParseErrorCode::NestedBlock);
	if (code == "ContentAfterBrace") return static_cast<int>(coda::ParseErrorCode::ContentAfterBrace);
	if (code == "KeyInBlock") return static_cast<int>(coda::ParseErrorCode::KeyInBlock);
	return -1;
}

} // namespace

inline TestCatalog build_catalog() {
	TestCatalog t;
	const std::string path = "tests/catalog/catalog.coda";
	std::string text = read_file(path);
	if (text.empty()) {
		t.push_back({"Catalog", "load catalog", [path](ParseAdapter&) {
			(void)path;
			return false;
		}});
		return t;
	}

	try {
		coda::CodaFile file = coda::detail::Parser(text, path).parse();
		const auto& tests = file["tests"].asArray().content;
		for (const auto& test : tests) {
			const std::string suite = block_string(test, "suite");
			const std::string name = block_string(test, "name");
			const std::string src = block_string(test, "src");
			const bool has_action = block_has(test, "action");
			const std::string action = has_action ? block_string(test, "action") : "";

			coda::CodaValue test_copy = test;
			t.push_back({ suite, name, [test_copy, src, action](ParseAdapter& p) {
				if (!action.empty()) {
					if (action == "parse_fail_msg") {
						auto needles = block_string_list(test_copy, "needles");
						return p.parse_fails_with_msg(src.c_str(), needles);
					}
					if (action == "parse_fail_code") {
						int code = parse_error_code(block_string(test_copy, "code"));
						return p.parse_fails_with_code(src.c_str(), code);
					}
					if (action == "roundtrip") {
						return p.roundtrip_stable(src.c_str());
					}
					return false;
				}

				if (!p.parse(src.c_str())) return false;
				if (!block_has(test_copy, "checks")) return true;
				for (const auto& check : block_at(test_copy, "checks").asArray().content) {
					if (!run_check(p, check)) return false;
				}
				return true;
			}});
		}
	} catch (...) {
		t.push_back({"Catalog", "parse catalog", [](ParseAdapter&) { return false; }});
	}

	return t;
}

} // namespace test_framework
