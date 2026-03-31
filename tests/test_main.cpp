#include "../include/coda.hpp"

#include <exception>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// ─── tiny test harness ─────────────────────────────────────────────────────────

static int passed = 0;
static int failed = 0;

static void suite(const std::string& name) {
	std::cout << "\n[" << name << "]\n";
}

static void pass(const std::string& name) {
	++passed;
	std::cout << "  ✓  " << name << "\n";
}

static void fail(const std::string& name, const std::string& reason) {
	++failed;
	std::cout << "  ✗  " << name << "\n      " << reason << "\n";
}

#define CHECK(name, ...) \
do { \
	try { \
		if (__VA_ARGS__) { pass(name); } \
		else      { fail(name, "expression was false: " #__VA_ARGS__); } \
	} catch (const std::exception& e) { \
		fail(name, std::string("unexpected exception: ") + e.what()); \
	} \
} while (0)

#define CHECK_THROWS(name, expr) \
do { \
	try { \
		(void)(expr); \
		fail(name, "expected an exception but none was thrown"); \
	} catch (...) { \
		pass(name); \
	} \
} while (0)

#define CHECK_THROWS_TYPE(name, ExType, expr) \
do { \
	try { \
		(void)(expr); \
		fail(name, std::string("expected ") + #ExType + " but nothing was thrown"); \
	} catch (const ExType&) { \
		pass(name); \
	} catch (const std::exception& e) { \
		fail(name, std::string("wrong exception type (std::exception): ") + e.what()); \
	} catch (...) { \
		fail(name, "wrong exception type (non-std exception)"); \
	} \
} while (0)

static bool contains(std::string_view haystack, std::string_view needle) {
	return haystack.find(needle) != std::string_view::npos;
}

// ─── parse helper (string → CodaFile) ─────────────────────────────────────────

static coda::CodaFile parse(const std::string& src) {
	return coda::detail::Parser(src).parseFile();
}

// Parse must fail with coda::ParseError; message should contain at least one needle.
// (Loose matching keeps tests meaningful without being brittle across wording changes.)
static void check_parse_throws_msg(
	const std::string& testName,
	const std::string& src,
	std::initializer_list<std::string_view> needles)
{
	try {
		(void)parse(src);
		fail(testName, "expected coda::ParseError but parsing succeeded");
	} catch (const coda::ParseError& e) {
		std::string_view msg(e.what());

		bool ok = needles.size() == 0;
		for (auto n : needles) ok = ok || contains(msg, n);

		if (!ok) {
			std::ostringstream oss;
			oss << "ParseError message did not contain any expected substrings.\n"
				<< "  message: " << e.what();
			fail(testName, oss.str());
		} else {
			pass(testName);
		}
	} catch (const std::exception& e) {
		fail(testName, std::string("wrong exception type (std::exception): ") + e.what());
	} catch (...) {
		fail(testName, "wrong exception type (non-std exception)");
	}
}

// Parse must fail with a ParseError whose .code matches the expected code.
static void check_parse_throws_code(
	const std::string& testName,
	const std::string& src,
	coda::ParseErrorCode expectedCode)
{
	try {
		(void)parse(src);
		fail(testName, "expected coda::ParseError but parsing succeeded");
	} catch (const coda::ParseError& e) {
		if (e.code == expectedCode) {
			pass(testName);
		} else {
			std::ostringstream oss;
			oss << "ParseError code mismatch. Got message: " << e.what();
			fail(testName, oss.str());
		}
	} catch (const std::exception& e) {
		fail(testName, std::string("wrong exception type (std::exception): ") + e.what());
	} catch (...) {
		fail(testName, "wrong exception type (non-std exception)");
	}
}

static bool roundtrip_equal(const std::string& src) {
	auto f1 = parse(src);
	std::string ser1 = f1.serialize();
	auto f2 = parse(ser1);
	std::string ser2 = f2.serialize();
	return ser1 == ser2;
}

// ─── suites ───────────────────────────────────────────────────────────────────

static void test_scalars_and_escapes() {
	suite("Scalars & escapes");

	CHECK("unquoted string value",
	parse("name myproject\n")["name"].asString() == "myproject");

	CHECK("quoted string with spaces",
	parse("author \"Albert Einstein\"\n")["author"].asString() == "Albert Einstein");

	CHECK("URL as unquoted string",
	parse("url https://example.com/path?query=1\n")["url"].asString() == "https://example.com/path?query=1");

	CHECK("email as unquoted string",
	parse("email user@domain.com\n")["email"].asString() == "user@domain.com");

	CHECK("escape \\n",
	parse("msg \"hello\\nworld\"\n")["msg"].asString() == std::string("hello\nworld"));

	CHECK("escape \\\\",
	parse("path \"C:\\\\Users\\\\name\"\n")["path"].asString() == std::string("C:\\Users\\name"));

	CHECK("escape \\\"",
	parse("q \"He said \\\"hello\\\"\"\n")["q"].asString() == std::string("He said \"hello\""));

	CHECK("escape \\t",
	parse("tab \"a\\tb\"\n")["tab"].asString() == std::string("a\tb"));

	CHECK("escape \\r",
	parse("cr \"a\\rb\"\n")["cr"].asString() == std::string("a\rb"));

	CHECK("empty quoted string",
	parse("empty \"\"\n")["empty"].asString() == "");

	CHECK("quoted key",
	parse("\"key with spaces\" value\n")["key with spaces"].asString() == "value");

	CHECK("isContainer false for string",
	!parse("x hello\n")["x"].isContainer());
}

static void test_reserved_key_word() {
	suite("Reserved word: key");

	check_parse_throws_msg(
		"bare 'key' as key is illegal",
		"key value\n",
		{ "expected key", "got 'key'", "'key'" }
	);

	CHECK("quoted \"key\" can be used as a key",
	parse("\"key\" value\n")["key"].asString() == "value");
}

static void test_comments() {
	suite("Comments");

	CHECK("comment before key parses",
	parse("# Project configuration\nname myproject\n")["name"].asString() == "myproject");

	CHECK("comment attaches to following node",
	parse("# top comment\nname myproject\n")["name"].comment == "top comment");

	CHECK("inner comment attaches inside block",
	parse("compiler {\n  # Enable optimizations\n  optimize true\n  debug false\n}\n")
	["compiler"]["optimize"].comment == "Enable optimizations");

	CHECK("comment followed by kv works",
	parse("# whole line comment\nk value\n")["k"].asString() == "value");

	CHECK("comment before bare list array is allowed",
	[]{
	auto f = parse("# list comment\nitems [\n  one\n  two\n]\n");
	return f["items"].comment == "list comment"
		&& f["items"].asArray().content.size() == 2;
	}());

	CHECK("comment before array element in bare list is preserved",
	[]{
	auto f = parse("items [\n  # first\n  one\n  two\n]\n");
	return f["items"].asArray().content[0].comment == "first";
	}());

	check_parse_throws_msg(
		"comment before plain table header is a hard error",
		"t [\n  # oops\n  x y\n  1 2\n]\n",
		{ "comment", "header", "before" }
	);

	check_parse_throws_code(
		"comment before plain table header produces CommentBeforeHeader error code",
		"t [\n  # oops\n  x y\n  1 2\n]\n",
		coda::ParseErrorCode::CommentBeforeHeader
	);

	check_parse_throws_msg(
		"comment before keyed table header is a hard error",
		"t [\n  # oops\n  key link version\n  plot github.com/plot 4.0.3\n]\n",
		{ "comment", "header", "before" }
	);

	check_parse_throws_code(
		"comment before keyed table header produces CommentBeforeHeader error code",
		"t [\n  # oops\n  key link version\n  plot github.com/plot 4.0.3\n]\n",
		coda::ParseErrorCode::CommentBeforeHeader
	);

	CHECK("comment on plain table data row is preserved",
	[]{
	auto f = parse("t [\n\tx y\n\t# row comment\n\t1 2\n]\n");
	return f["t"].asArray().content[0].comment == "row comment";
	}());

	CHECK("comment on keyed table data row is preserved",
	[]{
	auto f = parse("t [\n\tkey x\n\t# row comment\n\ta 1\n]\n");
	return f["t"].asTable()["a"].comment == "row comment";
	}());
}

static void test_blocks() {
	suite("Blocks");

	CHECK("basic block values",
	[]{
	auto f = parse("compiler {\n  debug false\n  optimize true\n}\n");
	return f["compiler"]["debug"].asString() == "false"
	&& f["compiler"]["optimize"].asString() == "true";
	}());

	CHECK("nested blocks",
	parse("outer {\n  inner {\n    x 42\n  }\n}\n")
	["outer"]["inner"]["x"].asString() == "42");

	CHECK("isContainer true for block",
	parse("b {\n  a 1\n}\n")["b"].isContainer());

	CHECK("block iteration yields expected keys",
	[]{
	auto f = parse("b {\n  x 1\n  y 2\n}\n");
	const auto& b = f["b"].asBlock();
	std::vector<std::string> keys;
	for (const auto& [k, _] : b) keys.push_back(k);
	return keys == std::vector<std::string>{"x", "y"};
	}());

	check_parse_throws_msg(
		"inline block syntax is illegal",
		"compiler { debug false }\n",
		{ "unexpected content", "new line", "newline" }
	);

	check_parse_throws_msg(
		"unclosed block throws",
		"b {\n  x 1\n",
		{ "expected", "'}'", "end of file" }
	);

	check_parse_throws_msg(
		"'key' inside block is illegal",
		"b {\n  key link ver\n  a b c\n}\n",
		{ "expected key", "'key'" }
	);
}

static void test_arrays_bare_lists() {
	suite("Arrays: bare lists");

	CHECK("bare list size and indexing",
	[]{
	auto f = parse("targets [\n  x86_64-linux\n  x86_64-windows\n  aarch64-macos\n]\n");
	const auto& arr = f["targets"].asArray();
	return arr.content.size() == 3
	&& arr[0].asString() == "x86_64-linux"
	&& arr[2].asString() == "aarch64-macos";
	}());

	CHECK("empty array",
	parse("empty [\n]\n")["empty"].asArray().content.empty());

	CHECK("isContainer true for array",
	parse("a [\n  x\n]\n")["a"].isContainer());

	CHECK("nested blocks inside a bare list are allowed",
	[]{
	auto f = parse(
	"items [\n"
	"  {\n"
	"    val 1\n"
	"  }\n"
	"  {\n"
	"    val 2\n"
	"  }\n"
	"]\n"
	);
	const auto& a = f["items"].asArray();
	return a.content.size() == 2
	&& a[0]["val"].asString() == "1"
	&& a[1]["val"].asString() == "2";
	}());

	CHECK_THROWS_TYPE("array content.at out-of-range throws std::out_of_range",
				std::out_of_range,
				[]{
				auto f = parse("a [\n  x\n]\n");
				(void)f["a"].asArray().content.at(99);
				}());
}

static void test_plain_tables() {
	suite("Arrays: plain tables");

	CHECK("plain table rows become array<tables>",
	[]{
	auto f = parse("points [\n  x y z\n  1 2 3\n  4 5 6\n]\n");
	const auto& a = f["points"].asArray();
	return a.content.size() == 2
	&& a[0]["x"].asString() == "1"
	&& a[0]["z"].asString() == "3"
	&& a[1]["y"].asString() == "5";
	}());

	check_parse_throws_msg(
		"duplicate field names in plain table header throws",
		"t [\n  a a\n  1 2\n]\n",
		{ "duplicate", "field", "header" }
	);

	check_parse_throws_msg(
		"nesting inside a plain table row is illegal",
		"t [\n  x y\n  1 {\n    a b\n  }\n]\n",
		{ "table", "row", "nested", "nesting", "block" }
	);

	check_parse_throws_msg(
		"plain table: short row is a hard error",
		"t [\n  x y z\n  1 2 3\n  4 5\n]\n",
		{ "row", "2", "expected", "3" }
	);

	check_parse_throws_code(
		"plain table: short row produces RaggedRow error code",
		"t [\n  x y z\n  1 2 3\n  4 5\n]\n",
		coda::ParseErrorCode::RaggedRow
	);

	check_parse_throws_msg(
		"plain table: single-value row against multi-column header is a hard error",
		"t [\n  x y\n  1\n]\n",
		{ "row", "1", "expected", "2" }
	);
}

static void test_keyed_tables() {
	suite("Arrays: keyed tables");

	CHECK("keyed table lookup by row key works",
	[]{
	auto f = parse(
	"deps [\n"
	"  key link version\n"
	"  plot github.com/zane-lang/plot 4.0.3\n"
	"  http github.com/zane-lang/http 2.1.0\n"
	"]\n"
	);
	return f["deps"]["plot"]["link"].asString() == "github.com/zane-lang/plot"
	&& f["deps"]["plot"]["version"].asString() == "4.0.3"
	&& f["deps"]["http"]["version"].asString() == "2.1.0";
	}());

	CHECK("iterate keyed table preserves insertion order",
	[]{
	auto f = parse("t [\n  key link\n  a http://a\n  b http://b\n]\n");
	const auto& t = f["t"].asTable();
	std::vector<std::string> keys;
	for (const auto& [k, _] : t) keys.push_back(k);
	return keys == std::vector<std::string>{"a", "b"};
	}());

	CHECK("missing key in keyed table inserts a default row/value",
	[]{
	auto f = parse("deps [\n  key link\n  a b\n]\n");
	auto& v = f["deps"]["nope"];
	return f["deps"].asTable().content.contains("nope")
	&& v.asString() == "";
	}());

	CHECK_THROWS("const keyed table lookup still throws on missing row",
			[]{
			auto f = parse("deps [\n  key link\n  a b\n]\n");
			const auto& cf = f;
			(void)cf["deps"]["missing_row"];
			}());

	check_parse_throws_msg(
		"duplicate row key in keyed table throws",
		"t [\n  key v\n  a 1\n  a 2\n]\n",
		{ "duplicate", "key" }
	);

	check_parse_throws_msg(
		"nesting inside keyed table rows is illegal",
		"t [\n  key v\n  a {\n    x 1\n  }\n]\n",
		{ "table", "row", "nested", "nesting", "block" }
	);

	check_parse_throws_msg(
		"keyed table: row missing a field is a hard error",
		"deps [\n  key link version\n  plot github.com/plot 4.0.3\n  http github.com/http\n]\n",
		{ "row", "http", "1", "expected", "2" }
	);

	check_parse_throws_code(
		"keyed table: short row produces RaggedRow error code",
		"deps [\n  key link version\n  plot github.com/plot 4.0.3\n  http github.com/http\n]\n",
		coda::ParseErrorCode::RaggedRow
	);

	check_parse_throws_msg(
		"keyed table: key-only row (zero values) is a hard error",
		"t [\n  key v\n  a 1\n  b\n]\n",
		{ "row", "b", "0", "expected", "1" }
	);
}

static void test_type_errors_and_insertion_semantics() {
	suite("Type errors + [] insertion semantics");

	CHECK_THROWS("string-indexing a scalar throws",
			parse("s hello\n")["s"]["x"]);

	CHECK_THROWS("int-indexing a block throws",
			parse("b {\n  x 1\n}\n")["b"][size_t(0)]);

	CHECK_THROWS("asArray on scalar throws",
			parse("s hello\n")["s"].asArray());

	CHECK_THROWS("asBlock on array throws",
			parse("a [\n  x\n]\n")["a"].asBlock());

	CHECK_THROWS("asTable on block throws",
			parse("b {\n  x 1\n}\n")["b"].asTable());

	CHECK("missing top-level key inserts default value",
	[]{
	auto f = parse("a 1\n");
	auto& v = f["b"];
	return f.has("b") && v.asString() == "";
	}());

	CHECK("top-level [] allows create-then-assign",
	[]{
	auto f = parse("a 1\n");
	f["b"] = std::string("2");
	return f.has("b") && f["b"].asString() == "2";
	}());

	CHECK("block [] allows inserting a new key",
	[]{
	auto f = parse("compiler {\n  debug false\n}\n");
	f["compiler"]["optimize"] = std::string("true");
	return f["compiler"]["optimize"].asString() == "true";
	}());

	CHECK_THROWS("const file[] still throws on missing key",
			[]{
			auto f = parse("a 1\n");
			const auto& cf = f;
			(void)cf["missing"];
			}());
}

static void test_ordering_and_indentation() {
	suite("Ordering & indentation");

	CHECK("default order puts scalars before containers and sorts alphabetically",
	[]{
	auto f = parse("zblock {\n  inner v\n}\n"
				"b two\n"
				"a one\n");
	f.order();
	std::string s = f.serialize();
	auto pa = s.find("a one");
	auto pb = s.find("b two");
	auto pz = s.find("zblock {");
	return pa != std::string::npos
	&& pb != std::string::npos
	&& pz != std::string::npos
	&& pa < pb
	&& pb < pz;
	}());

	CHECK("weighted order puts higher-weight keys earlier",
	[]{
	auto f = parse("b two\na one\nname myproject\n");
	f.order([](const std::string& k) -> float {
	if (k == "name") return 100.0f;
	return 0.0f;
	});
	std::string s = f.serialize();
	return s.find("name myproject") < s.find("a one");
	}());

	CHECK("serialize with 4 spaces contains expected indentation",
	[]{
	auto f = parse("b {\n  x 1\n}\n");
	std::string s = f.serialize("    ");
	return s.find("    x 1") != std::string::npos;
	}());
}

static void test_roundtrip_and_mutation() {
	suite("Round-trip & mutation");

	CHECK("scalar round-trip stable",
	roundtrip_equal("name myproject\ntype executable\n"));

	CHECK("block round-trip stable",
	roundtrip_equal("compiler {\n\tdebug false\n\toptimize true\n}\n"));

	CHECK("bare list round-trip stable",
	roundtrip_equal("targets [\n\tx86_64-linux\n\tx86_64-windows\n]\n"));

	CHECK("keyed table round-trip stable",
	roundtrip_equal("deps [\n\tkey link version\n\tplot github.com/zane-lang/plot 4.0.3\n]\n"));

	CHECK("plain table round-trip stable",
	roundtrip_equal("points [\n\tx y\n\t1 2\n\t3 4\n]\n"));

	CHECK("escape sequences survive round-trip",
	roundtrip_equal("msg \"hello\\nworld\"\npath \"C:\\\\Users\\\\name\"\n"));

	CHECK("CodaFile::has works",
	[]{
	auto f = parse("a 1\nb 2\n");
	return f.has("a") && f.has("b") && !f.has("z");
	}());

	CHECK("mutation: assign string to existing key",
	[]{
	auto f = parse("name old\n");
	f["name"] = std::string("new");
	return f["name"].asString() == "new";
	}());

	CHECK("mutation: mutate nested block value",
	[]{
	auto f = parse("compiler {\n  debug true\n}\n");
	f["compiler"]["debug"] = std::string("false");
	return f["compiler"]["debug"].asString() == "false";
	}());
}

static void test_parse_errors() {
	suite("Parse errors (message-based)");

	check_parse_throws_msg(
		"expected key ... got 'key' message includes location and reason",
		"# comment\nkey value\n",
		{ "error", "expected key", "'key'" }
	);

	check_parse_throws_msg(
		"unclosed bracket throws",
		"a [\n  x\n",
		{ "expected", "']'", "end of file" }
	);

	check_parse_throws_msg(
		"duplicate top-level key throws",
		"name a\nname b\n",
		{ "duplicate", "name" }
	);

	check_parse_throws_msg(
		"unterminated string throws",
		"k \"unterminated\n",
		{ "unterminated", "string", "error", "expected" }
	);

	check_parse_throws_msg(
		"ragged row in plain table mentions row and expected counts",
		"matrix [\n  a b c\n  1 2\n]\n",
		{ "row", "2", "expected", "3" }
	);

	check_parse_throws_msg(
		"ragged row in keyed table mentions row key and expected counts",
		"items [\n  key x y\n  first 1 2\n  second 3\n]\n",
		{ "row", "second", "1", "expected", "2" }
	);
}

int main() {
	std::cout << "=== Coda Test Suite ===\n";

	test_scalars_and_escapes();
	test_reserved_key_word();
	test_comments();
	test_blocks();
	test_arrays_bare_lists();
	test_plain_tables();
	test_keyed_tables();
	test_type_errors_and_insertion_semantics();
	test_ordering_and_indentation();
	test_roundtrip_and_mutation();
	test_parse_errors();

	std::cout << "\n══════════════════════════════\n";
	std::cout << "  Passed: " << passed << "\n";
	std::cout << "  Failed: " << failed << "\n";
	std::cout << "══════════════════════════════\n";

	return failed > 0 ? 1 : 0;
}
