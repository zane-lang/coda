#include "../harness/cpp/runner.hpp"
#include "../../include/coda.hpp"

#include <iostream>
#include <optional>
#include <string>
#include <vector>

using namespace test_framework;

// ─── C++ ParseAdapter ────────────────────────────────────────────────────────

class CppAdapter : public ParseAdapter {
	std::optional<coda::Block> root_;

	coda::Block& f() { return *root_; }

	static coda::Block do_parse(const char* src) {
		return coda::detail::Parser(src).parse();
	}

public:
	bool parse(const char* src) override {
		try {
			root_.emplace(do_parse(src));
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
			for (const auto& needle : needles)
				if (msg.find(needle) == std::string_view::npos) return false;
			return true;
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
		coda::detail::Value* v = nullptr;
		for (size_t i = 0; i < keys.size(); ++i) {
			if (i == 0) v = &f()[keys[i]];
			else        v = &v->asBlock()[keys[i]];
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
		return f()[key].asBlock().getContent().size();
	}

	std::vector<std::string> get_map_keys(const char* key) override {
		std::vector<std::string> keys;
		for (const auto& [k, _] : f()[key].asBlock()) keys.push_back(k);
		return keys;
	}

	int get_node_kind(const char* key) override {
		// No catalog op reads node kind by integer; map the variant to the
		// FFI kind codes for parity, without try/catch.
		return f()[key].visitContent(
			[](const std::string&)      { return 2; }, // CODA_NODE_STRING
			[](const coda::Block&)      { return 3; },
			[](const coda::Array&)      { return 4; },
			[](const coda::Table&)      { return 5; },
			[](const coda::KeyedTable&) { return 6; });
	}

	size_t get_array_len(const char* key) override {
		// Arrays and plain tables both expose a length; dispatch on kind
		// rather than probing via exceptions.
		return f()[key].visitContent(
			[](const std::string&)      -> size_t { return 0; },
			[](const coda::Block& b)    -> size_t { return b.size(); },
			[](const coda::Array& a)    -> size_t { return a.size(); },
			[](const coda::Table& t)    -> size_t { return t.size(); },
			[](const coda::KeyedTable&) -> size_t { return 0; });
	}

	std::string get_array_element(const char* key, size_t idx) override {
		return f()[key].asArray()[idx].asString();
	}

	bool array_index_throws(const char* key, size_t idx) override {
		return throws([&] { (void)f()[key].asArray()[idx]; });
	}

	std::string get_array_block_field(const char* array_key, size_t idx,
	                                  const char* field) override {
		return f()[array_key].asArray()[idx].asBlock()[field].asString();
	}

	size_t get_array_block_count(const char* array_key) override {
		return f()[array_key].asArray().size();
	}

	std::string get_table_cell(const char* table, const char* row,
	                           const char* col) override {
		return f()[table].asKeyedTable()[row][col];
	}

	std::vector<std::string> get_table_row_keys(const char* table) override {
		std::vector<std::string> keys;
		for (const auto& [k, _] : f()[table].asKeyedTable()) keys.push_back(k);
		return keys;
	}

	bool table_row_missing_throws(const char* table, const char* row) override {
		return throws([&] { const auto& cf = f(); (void)cf[table].asKeyedTable()[row]; });
	}

	bool table_row_missing_inserts(const char* table, const char* row) override {
		auto& kt = f()[table].asKeyedTable();
		try { (void)kt[row]; } catch (...) {}
		return kt.getContent().count(row) > 0;
	}

	std::string get_plain_table_cell(const char* table, size_t row,
	                                 const char* col) override {
		return f()[table].asTable()[row][col];
	}

	std::string get_comment(const char* key) override {
		return f()[key].getComment();
	}

	std::string get_comment_path(const std::vector<std::string>& keys) override {
		coda::detail::Value* v = nullptr;
		for (size_t i = 0; i < keys.size(); ++i) {
			if (i == 0) v = &f()[keys[i]];
			else        v = &v->asBlock()[keys[i]];
		}
		return v->getComment();
	}

	std::string get_array_element_comment(const char* key, size_t idx) override {
		return f()[key].asArray()[idx].getComment();
	}

	std::string get_table_row_comment(const char* table, const char* row) override {
		return f()[table].asKeyedTable()[row].getComment();
	}

	std::string get_plain_table_row_comment(const char* table, size_t row) override {
		return f()[table].asTable()[row].getComment();
	}

	bool set_string(const char* key, const char* value) override {
		f().insert(key, std::string(value));
		return true;
	}

	bool set_string_path(const std::vector<std::string>& keys,
	                     const char* value) override {
		coda::Block* curr = &f();
		for (size_t i = 0; i + 1 < keys.size(); ++i) {
			if (!curr->has(keys[i])) {
				curr->insert(keys[i], coda::Block());
			}
			curr = &curr->operator[](keys[i]).asBlock();
		}
		curr->insert(keys.back(), std::string(value));
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

	// These checks verify that the *library* throws on a type/lookup error,
	// so try/catch is the operation under test (not control flow). One shared
	// helper instead of six copy-pasted try/catch blocks.
	template<typename Fn>
	static bool throws(Fn&& fn) {
		try { fn(); return false; }
		catch (...) { return true; }
	}

	bool string_index_on_scalar_throws(const char* key, const char* sub) override {
		return throws([&] { (void)f()[key].asBlock()[sub]; });
	}

	bool int_index_on_block_throws(const char* key, size_t idx) override {
		return throws([&] { (void)f()[key].asArray()[idx]; });
	}

	bool as_array_on_scalar_throws(const char* key) override {
		return throws([&] { (void)f()[key].asArray(); });
	}

	bool as_block_on_array_throws(const char* key) override {
		return throws([&] { (void)f()[key].asBlock(); });
	}

	bool as_table_on_block_throws(const char* key) override {
		return throws([&] { (void)f()[key].asTable(); });
	}

	bool const_missing_key_throws(const char* key) override {
		return throws([&] { const auto& cf = f(); (void)cf[key]; });
	}

	std::string get_header_comment(const char* key) override {
		return f()[key].visitContent(
			[](const std::string&)        { return std::string{}; },
			[](const coda::Block&)        { return std::string{}; },
			[](const coda::Array& a)      { return a.getHeaderComment(); },
			[](const coda::Table& t)      { return t.getHeaderComment(); },
			[](const coda::KeyedTable& t) { return t.getHeaderComment(); });
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
