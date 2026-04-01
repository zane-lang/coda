#pragma once

namespace test_data {

// ── Basic ─────────────────────────────────────────────────────────────────────

inline constexpr const char* SIMPLE_DOC =
	"name example\n"
	"type executable\n";

inline constexpr const char* QUOTED_STRINGS =
	"name example\n"
	"type executable\n"
	"author \"Albert Einstein\"\n";

inline constexpr const char* WITH_COMMENTS =
	"# Project name\n"
	"name example\n";

// ── Scalars & escapes ─────────────────────────────────────────────────────────

inline constexpr const char* UNQUOTED_STRING        = "name myproject\n";
inline constexpr const char* QUOTED_STRING_SPACES    = "author \"Albert Einstein\"\n";
inline constexpr const char* URL_UNQUOTED            = "url https://example.com/path?query=1\n";
inline constexpr const char* EMAIL_UNQUOTED          = "email user@domain.com\n";
inline constexpr const char* ESCAPE_NEWLINE          = "msg \"hello\\nworld\"\n";
inline constexpr const char* ESCAPE_BACKSLASH        = "path \"C:\\\\Users\\\\name\"\n";
inline constexpr const char* ESCAPE_QUOTE            = "q \"He said \\\"hello\\\"\"\n";
inline constexpr const char* ESCAPE_TAB              = "tab \"a\\tb\"\n";
inline constexpr const char* ESCAPE_CR               = "cr \"a\\rb\"\n";
inline constexpr const char* EMPTY_QUOTED_STRING     = "empty \"\"\n";
inline constexpr const char* QUOTED_KEY              = "\"key with spaces\" value\n";
inline constexpr const char* SCALAR_HELLO            = "x hello\n";

// ── Reserved word: key ────────────────────────────────────────────────────────

inline constexpr const char* BARE_KEY_AS_KEY         = "key value\n";
inline constexpr const char* QUOTED_KEY_AS_KEY       = "\"key\" value\n";

// ── Comments ──────────────────────────────────────────────────────────────────

inline constexpr const char* COMMENT_BEFORE_KV =
	"# Project configuration\n"
	"name myproject\n";

inline constexpr const char* COMMENT_TOP =
	"# top comment\n"
	"name myproject\n";

inline constexpr const char* COMMENT_INNER_BLOCK =
	"compiler {\n"
	"  # Enable optimizations\n"
	"  optimize true\n"
	"  debug false\n"
	"}\n";

inline constexpr const char* COMMENT_BEFORE_KV2 =
	"# whole line comment\n"
	"k value\n";

inline constexpr const char* COMMENT_BEFORE_BARE_LIST =
	"# list comment\n"
	"items [\n"
	"  one\n"
	"  two\n"
	"]\n";

inline constexpr const char* COMMENT_BEFORE_ARRAY_ELEMENT =
	"items [\n"
	"  # first\n"
	"  one\n"
	"  two\n"
	"]\n";

inline constexpr const char* COMMENT_ON_PLAIN_TABLE_ROW =
	"t [\n"
	"\tx y\n"
	"\t# row comment\n"
	"\t1 2\n"
	"]\n";

inline constexpr const char* COMMENT_ON_KEYED_TABLE_ROW =
	"t [\n"
	"\tkey x\n"
	"\t# row comment\n"
	"\ta 1\n"
	"]\n";

// ── Blocks ────────────────────────────────────────────────────────────────────

inline constexpr const char* NESTED_BLOCK =
	"compiler {\n"
	"  debug false\n"
	"  optimize true\n"
	"}\n";

inline constexpr const char* DEEPLY_NESTED_BLOCK =
	"outer {\n"
	"  inner {\n"
	"    x 42\n"
	"  }\n"
	"}\n";

inline constexpr const char* BLOCK_SINGLE =
	"b {\n"
	"  a 1\n"
	"}\n";

inline constexpr const char* BLOCK_TWO_KEYS =
	"b {\n"
	"  x 1\n"
	"  y 2\n"
	"}\n";

inline constexpr const char* INLINE_BLOCK_ILLEGAL =
	"compiler { debug false }\n";

inline constexpr const char* UNCLOSED_BLOCK =
	"b {\n"
	"  x 1\n";

inline constexpr const char* KEY_INSIDE_BLOCK =
	"b {\n"
	"  key link ver\n"
	"  a b c\n"
	"}\n";

// ── Arrays ────────────────────────────────────────────────────────────────────

inline constexpr const char* ARRAY_DOC =
	"targets [\n"
	"  x86_64-linux\n"
	"  x86_64-windows\n"
	"  aarch64-macos\n"
	"]\n";

inline constexpr const char* EMPTY_ARRAY =
	"empty [\n"
	"]\n";

inline constexpr const char* ARRAY_SINGLE =
	"a [\n"
	"  x\n"
	"]\n";

inline constexpr const char* NESTED_BLOCKS_IN_BARE_LIST =
	"items [\n"
	"  {\n"
	"    val 1\n"
	"  }\n"
	"  {\n"
	"    val 2\n"
	"  }\n"
	"]\n";

// ── Tables ────────────────────────────────────────────────────────────────────

inline constexpr const char* PLAIN_TABLE =
	"points [\n"
	"  x y z\n"
	"  1 2 3\n"
	"  4 5 6\n"
	"]\n";

inline constexpr const char* PLAIN_TABLE_SHORT_ROW =
	"points [\n"
	"  x y z\n"
	"  1 2\n"
	"]\n";

inline constexpr const char* PLAIN_TABLE_LONG_ROW =
	"points [\n"
	"  x y z\n"
	"  1 2 3 4\n"
	"]\n";

inline constexpr const char* PLAIN_TABLE_DUP_HEADER =
	"t [\n"
	"  a a\n"
	"  1 2\n"
	"]\n";

inline constexpr const char* PLAIN_TABLE_NESTED_ROW =
	"t [\n"
	"  x y\n"
	"  1 {\n"
	"    a b\n"
	"  }\n"
	"]\n";

inline constexpr const char* PLAIN_TABLE_SINGLE_VALUE_ROW =
	"t [\n"
	"  x y\n"
	"  1\n"
	"]\n";

inline constexpr const char* KEYED_TABLE =
	"deps [\n"
	"  key link version\n"
	"  plot github.com/zane-lang/plot 4.0.3\n"
	"  http github.com/zane-lang/http 2.1.0\n"
	"]\n";

inline constexpr const char* KEYED_TABLE_SHORT_ROW =
	"deps [\n"
	"  key link version\n"
	"  plot github.com/zane-lang/plot\n"
	"]\n";

inline constexpr const char* KEYED_TABLE_DUPLICATE_KEY =
	"deps [\n"
	"  key link version\n"
	"  plot github.com/zane-lang/plot 4.0.3\n"
	"  plot github.com/zane-lang/plot 5.0.0\n"
	"]\n";

inline constexpr const char* KEYED_TABLE_SIMPLE =
	"t [\n"
	"  key link\n"
	"  a http://a\n"
	"  b http://b\n"
	"]\n";

inline constexpr const char* KEYED_TABLE_SINGLE_COL =
	"deps [\n"
	"  key link\n"
	"  a b\n"
	"]\n";

inline constexpr const char* KEYED_TABLE_NESTED_ROW =
	"t [\n"
	"  key v\n"
	"  a {\n"
	"    x 1\n"
	"  }\n"
	"]\n";

inline constexpr const char* KEYED_TABLE_LONG_ROW =
	"deps [\n"
	"  key link version\n"
	"  plot github.com/plot 4.0.3\n"
	"  http github.com/http 2.0.2 beta\n"
	"]\n";

inline constexpr const char* KEYED_TABLE_KEY_ONLY_ROW =
	"t [\n"
	"  key v\n"
	"  a 1\n"
	"  b\n"
	"]\n";

inline constexpr const char* KEYED_TABLE_RAGGED_MSG =
	"t [\n"
	"  key x y\n"
	"  a 1 2\n"
	"  b 3\n"
	"]\n";

// ── Escape sequences ──────────────────────────────────────────────────────────

inline constexpr const char* ESCAPE_SEQUENCES =
	"message \"hello\\nworld\"\n"
	"path \"C:\\\\Users\\\\name\"\n"
	"quote \"He said \\\"hello\\\"\"\n"
	"tab \"col1\\tcol2\"\n";

// ── Comments on tables ────────────────────────────────────────────────────────

inline constexpr const char* COMMENT_BEFORE_PLAIN_TABLE_HEADER =
	"points [\n"
	"  # this should error\n"
	"  x y z\n"
	"  1 2 3\n"
	"]\n";

inline constexpr const char* COMMENT_BEFORE_KEYED_TABLE_HEADER =
	"deps [\n"
	"  # this should error\n"
	"  key link version\n"
	"  plot github.com/zane-lang/plot 4.0.3\n"
	"]\n";

inline constexpr const char* COMMENT_ON_TABLE_ROW =
	"deps [\n"
	"  key link version\n"
	"  # HTTP client\n"
	"  http github.com/zane-lang/http 2.1.0\n"
	"]\n";

// ── Error cases ───────────────────────────────────────────────────────────────

inline constexpr const char* INVALID_UNCLOSED            = "name example\ntype {\n";
inline constexpr const char* INVALID_UNTERMINATED_STRING  = "name \"unterminated\n";
inline constexpr const char* INVALID_DUPLICATE_KEY        = "name first\nname second\n";

inline constexpr const char* UNCLOSED_BRACKET =
	"a [\n"
	"  1\n";

// ── Ordering & indentation ────────────────────────────────────────────────────

inline constexpr const char* ORDERING_DOC =
	"b 2\n"
	"aa {\n"
	"  x 1\n"
	"}\n"
	"cc 3\n"
	"z [\n"
	"  a\n"
	"]\n";

inline constexpr const char* WEIGHTED_ORDER_DOC =
	"z 1\n"
	"a 2\n"
	"middle 3\n";

// ── Mutation ──────────────────────────────────────────────────────────────────

inline constexpr const char* SINGLE_KV = "a 1\n";
inline constexpr const char* NAME_OLD  = "name old\n";

// ── Complex ───────────────────────────────────────────────────────────────────

inline constexpr const char* COMPLEX_DOC =
	"name example\n"
	"type executable\n"
	"\n"
	"deps [\n"
	"  key link version\n"
	"  plot github.com/zane-lang/plot 4.0.3\n"
	"  http github.com/zane-lang/http 2.1.0\n"
	"]\n"
	"\n"
	"compiler {\n"
	"  debug false\n"
	"  optimize true\n"
	"  targets [\n"
	"    x86_64-linux\n"
	"    x86_64-windows\n"
	"    aarch64-macos\n"
	"  ]\n"
	"}\n"
	"\n"
	"meta {\n"
	"  author \"Albert Einstein\"\n"
	"  description \"a test project\"\n"
	"}\n";

} // namespace test_data
