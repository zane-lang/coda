#include "../harness/test_runner.hpp"
#include "../harness/test_data.hpp"
#include "../../include/coda.hpp"

#include <iostream>
#include <optional>
#include <string>
#include <vector>

using namespace test_framework;

// ─── C++ ParseAdapter ────────────────────────────────────────────────────────

class CppAdapter : public ParseAdapter {
	std::optional<coda::CodaFile> file_;

	coda::CodaFile& f() { return *file_; }

	static coda::CodaFile do_parse(const char* src) {
		return coda::detail::Parser(src).parse();
	}

public:
	bool parse(const char* src) override {
		try {
			file_.emplace(do_parse(src));
			return true;
		} catch (...) { return false; }
	}

	bool parse_fails(const char* src) override {
		try { (void)do_parse(src); return false; }
		catch (...) { return true; }
	}

	bool parse_fails_with_msg(const char* src,
	                          const std::vector<std::string>& needles) override {
		try {
			(void)do_parse(src);
			return false;
		} catch (const coda::ParseError& e) {
			std::string_view msg(e.what());
			for (const auto& n : needles)
				if (msg.find(n) != std::string_view::npos) return true;
			return needles.empty();
		} catch (...) { return false; }
	}

	bool parse_fails_with_code(const char* src, int code) override {
		try {
			(void)do_parse(src);
			return false;
		} catch (const coda::ParseError& e) {
			return static_cast<int>(e.code) == code;
		} catch (...) { return false; }
	}

	std::string get_string(const char* key) override {
		return f()[key].asString();
	}

	std::string get_string_path(const std::vector<std::string>& keys) override {
		coda::CodaValue* v = nullptr;
		for (size_t i = 0; i < keys.size(); ++i) {
			if (i == 0) v = &f()[keys[i]];
			else v = &(*v)[keys[i]];
		}
		return v->asString();
	}

	bool has_key(const char* key) override {
		return f().has(key);
	}

	bool is_container(const char* key) override {
		return f()[key].isContainer();
	}

	size_t get_map_len(const char* key) override {
		return f()[key].asBlock().content.size();
	}

	std::vector<std::string> get_map_keys(const char* key) override {
		std::vector<std::string> keys;
		for (const auto& [k, _] : f()[key].asBlock()) keys.push_back(k);
		return keys;
	}

	int get_node_kind(const char* key) override {
		return 0;
	}

	size_t get_array_len(const char* key) override {
		return f()[key].asArray().content.size();
	}

	std::string get_array_element(const char* key, size_t idx) override {
		return f()[key].asArray()[idx].asString();
	}

	bool array_index_throws(const char* key, size_t idx) override {
		try { (void)f()[key].asArray().content.at(idx); return false; }
		catch (...) { return true; }
	}

	std::string get_array_block_field(const char* array_key, size_t idx,
	                                  const char* field) override {
		return f()[array_key].asArray()[idx][field].asString();
	}

	size_t get_array_block_count(const char* array_key) override {
		return f()[array_key].asArray().content.size();
	}

	std::string get_table_cell(const char* table, const char* row,
	                           const char* col) override {
		return f()[table][row][col].asString();
	}

	std::vector<std::string> get_table_row_keys(const char* table) override {
		std::vector<std::string> keys;
		for (const auto& [k, _] : f()[table].asTable()) keys.push_back(k);
		return keys;
	}

	bool table_row_missing_throws(const char* table, const char* row) override {
		try {
			const auto& cf = f();
			(void)cf[table][row];
			return false;
		} catch (...) { return true; }
	}

	bool table_row_missing_inserts(const char* table, const char* row) override {
		auto& v = f()[table][row];
		return f()[table].asTable().content.contains(row) && v.asString() == "";
	}

	std::string get_plain_table_cell(const char* table, size_t row,
	                                 const char* col) override {
		return f()[table].asArray()[row][col].asString();
	}

	std::string get_comment(const char* key) override {
		return f()[key].comment;
	}

	std::string get_comment_path(const std::vector<std::string>& keys) override {
		coda::CodaValue* v = nullptr;
		for (size_t i = 0; i < keys.size(); ++i) {
			if (i == 0) v = &f()[keys[i]];
			else v = &(*v)[keys[i]];
		}
		return v->comment;
	}

	std::string get_array_element_comment(const char* key, size_t idx) override {
		return f()[key].asArray().content[idx].comment;
	}

	std::string get_table_row_comment(const char* table, const char* row) override {
		return f()[table].asTable()[row].comment;
	}

	std::string get_plain_table_row_comment(const char* table, size_t row) override {
		return f()[table].asArray().content[row].comment;
	}

	bool set_string(const char* key, const char* value) override {
		f()[key] = std::string(value);
		return true;
	}

	bool set_string_path(const std::vector<std::string>& keys,
	                     const char* value) override {
		coda::CodaValue* v = nullptr;
		for (size_t i = 0; i < keys.size(); ++i) {
			if (i == 0) v = &f()[keys[i]];
			else v = &(*v)[keys[i]];
		}
		*v = std::string(value);
		return true;
	}

	std::string serialize(const char* indent) override {
		return f().serialize(indent);
	}

	bool roundtrip_stable(const char* src) override {
		auto f1 = do_parse(src);
		std::string s1 = f1.serialize();
		auto f2 = do_parse(s1.c_str());
		std::string s2 = f2.serialize();
		return s1 == s2;
	}

	std::string order_default_and_serialize() override {
		f().order();
		return f().serialize();
	}

	std::string order_weighted_and_serialize(
		const std::vector<std::pair<std::string, float>>& weights) override {
		f().order([&](const std::string& k) -> float {
			for (const auto& [wk, wv] : weights)
				if (wk == k) return wv;
			return 0.f;
		});
		return f().serialize();
	}

	bool string_index_on_scalar_throws(const char* key, const char* sub) override {
		try { (void)f()[key][sub]; return false; }
		catch (...) { return true; }
	}

	bool int_index_on_block_throws(const char* key, size_t idx) override {
		try { (void)f()[key][idx]; return false; }
		catch (...) { return true; }
	}

	bool as_array_on_scalar_throws(const char* key) override {
		try { (void)f()[key].asArray(); return false; }
		catch (...) { return true; }
	}

	bool as_block_on_array_throws(const char* key) override {
		try { (void)f()[key].asBlock(); return false; }
		catch (...) { return true; }
	}

	bool as_table_on_block_throws(const char* key) override {
		try { (void)f()[key].asTable(); return false; }
		catch (...) { return true; }
	}

	bool const_missing_key_throws(const char* key) override {
		try {
			const auto& cf = f();
			(void)cf[key];
			return false;
		} catch (...) { return true; }
	}

	std::string get_header_comment(const char* key) override {
		auto& v = f()[key];

		// Plain table container = array
		if (std::holds_alternative<coda::CodaArray>(v.content.value))
			return v.asArray().headerComment;   // (whatever field name you added)

		// Keyed table container = table
		if (std::holds_alternative<coda::CodaTable>(v.content.value))
			return v.asTable().headerComment;

		return "";
	}
};

// ─── Main ─────────────────────────────────────────────────────────────────────

int main() {
	std::cout << "\n" << ANSI_BOLD << ANSI_BLUE
	          << "=== Coda C++ Test Suite ===" << ANSI_RESET << "\n";

	CppAdapter adapter;
	run_catalog(adapter);

	print_summary();
	return failed > 0 ? 1 : 0;
}
