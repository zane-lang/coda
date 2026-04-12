#pragma once
#include "ast.hpp"

#include <exception>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace coda {

struct SourceLoc {
	int    line      = 1;
	int    col       = 1;
	size_t lineStart = 0;
	size_t offset    = 0;
};

enum class ParseErrorCode {
	// Structural
	UnexpectedToken,
	UnexpectedEOF,

	// Semantic / validation
	DuplicateKey,
	DuplicateField,
	RaggedRow,

	// String / lexer level
	InvalidEscape,
	UnterminatedString,

	// Block / table structure
	NestedBlock,
	ContentAfterBrace,
	KeyInBlock,
};

struct ParseError : std::exception {
	ParseErrorCode code;
	SourceLoc      loc;
	std::string    message;
	std::string    filename;
	std::string    sourceLine;
	std::string    formatted;

	ParseError(ParseErrorCode code,
	           SourceLoc      loc,
	           std::string    message,
	           std::string    filename,
	           std::string    sourceLine)
		: code(code)
		, loc(loc)
		, message(std::move(message))
		, filename(std::move(filename))
		, sourceLine(std::move(sourceLine))
	{
		std::ostringstream os;
		if (!this->filename.empty())
			os << this->filename << ":";
		os << this->loc.line << ":" << this->loc.col
		   << ": error: " << this->message << "\n";

		if (!this->sourceLine.empty()) {
			os << "  " << this->sourceLine << "\n  ";
			for (int i = 0; i < this->loc.col - 1 && i < (int)this->sourceLine.size(); ++i)
				os << (this->sourceLine[i] == '\t' ? '\t' : ' ');
			os << "^";
		}
		formatted = os.str();
	}

	const char* what() const noexcept override {
		return formatted.c_str();
	}
};

namespace detail {

// ─── Token types ────────────────────────────────────────────────────────────

enum class TokenType {
	Ident, String, Key, Comment,
	LBrace, RBrace,
	LBracket, RBracket,
	Newline, Eof,
	Error
};

inline const std::map<TokenType, std::string> tokenToString = {
	{ TokenType::Ident,    "identifier"  },
	{ TokenType::String,   "string"      },
	{ TokenType::Key,      "'key'"       },
	{ TokenType::Comment,  "comment"     },
	{ TokenType::LBrace,   "'{'"         },
	{ TokenType::RBrace,   "'}'"         },
	{ TokenType::LBracket, "'['"         },
	{ TokenType::RBracket, "']'"         },
	{ TokenType::Newline,  "newline"     },
	{ TokenType::Eof,      "end of file" },
	{ TokenType::Error,    "error"       },
};

// ─── Token ──────────────────────────────────────────────────────────────────

struct Token {
	TokenType   type;
	std::string value;
	SourceLoc   loc;
};

// ─── Lexer ──────────────────────────────────────────────────────────────────

class Lexer {
	const std::string& src;
	size_t pos       = 0;
	int    line_     = 1;
	size_t lineStart = 0;

	char peek() const { return pos < src.size() ? src[pos] : '\0'; }

	char advance() {
		char c = src[pos++];
		if (c == '\n') {
			++line_; lineStart = pos;
		} else if (c == '\r') {
			if (pos < src.size() && src[pos] == '\n') ++pos;
			++line_; lineStart = pos;
		}
		return c;
	}

	void skipHorizontal() {
		while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\t'))
			++pos;
	}

	SourceLoc loc() const {
		return { line_, static_cast<int>(pos - lineStart) + 1, lineStart, pos };
	}

	bool isIdentChar(char c) const {
		if (std::isspace(static_cast<unsigned char>(c))) return false;
		switch (c) {
			case '{': case '}':
			case '[': case ']':
			case '"': case '#':
				return false;
		}
		return true;
	}

public:
	Lexer(const std::string& src) : src(src) {}

	Token next() {
		skipHorizontal();
		SourceLoc tokenLoc = loc();

		if (pos >= src.size()) return { TokenType::Eof, "", tokenLoc };

		char c = peek();

		if (c == '\n' || c == '\r') {
			while (pos < src.size() && (src[pos] == '\n' || src[pos] == '\r'))
				advance();
			return { TokenType::Newline, "", tokenLoc };
		}

		if (c == '{') { advance(); return { TokenType::LBrace,   "{", tokenLoc }; }
		if (c == '}') { advance(); return { TokenType::RBrace,   "}", tokenLoc }; }
		if (c == '[') { advance(); return { TokenType::LBracket, "[", tokenLoc }; }
		if (c == ']') { advance(); return { TokenType::RBracket, "]", tokenLoc }; }

		if (c == '"') {
			advance();
			std::string val;
			while (pos < src.size() && peek() != '"') {
				if (peek() == '\n' || peek() == '\r')
					return { TokenType::Error, "unterminated string", tokenLoc };

				if (peek() == '\\' && pos + 1 < src.size()) {
					advance();
					char esc = advance();
					switch (esc) {
						case 'n':  val += '\n'; break;
						case 't':  val += '\t'; break;
						case 'r':  val += '\r'; break;
						case '"':  val += '"';  break;
						case '\\': val += '\\'; break;
						default:
							return { TokenType::Error,
							         std::string("invalid escape '\\") + esc + "'",
							         tokenLoc };
					}
				} else {
					val += advance();
				}
			}
			if (pos >= src.size())
				return { TokenType::Error, "unterminated string", tokenLoc };
			advance(); // closing "
			return { TokenType::String, val, tokenLoc };
		}

		if (c == '#') {
			advance();
			if (pos < src.size() && peek() == ' ') advance();
			std::string val;
			while (pos < src.size() && peek() != '\n' && peek() != '\r')
				val += advance();
			return { TokenType::Comment, val, tokenLoc };
		}

		if (!isIdentChar(c)) {
			std::string bad(1, advance());
			return { TokenType::Error, bad, tokenLoc };
		}

		std::string val;
		while (pos < src.size() && isIdentChar(peek()))
			val += advance();

		if (val == "key") return { TokenType::Key, val, tokenLoc };
		return { TokenType::Ident, val, tokenLoc };
	}
};

// ─── Parser ─────────────────────────────────────────────────────────────────

class Parser {
	// ── members ─────────────────────────────────────────────────────────

	std::string source;
	std::string filename;
	Lexer       lexer;
	Token       current;
	Token       lookahead;
	std::string pendingComment;

	// ── token helpers ───────────────────────────────────────────────────

	void checkNotError() {
		if (current.type != TokenType::Error) return;
		coda::ParseErrorCode code;
		if (current.value.find("unterminated") != std::string::npos)
			code = coda::ParseErrorCode::UnterminatedString;
		else if (current.value.find("escape") != std::string::npos)
			code = coda::ParseErrorCode::InvalidEscape;
		else
			code = coda::ParseErrorCode::UnexpectedToken;
		fatalError(code, current.value, current.loc);
	}

	Token advance() {
		checkNotError();
		Token t   = current;
		current   = lookahead;
		lookahead = lexer.next();
		return t;
	}

	Token expect(TokenType type) {
		if (current.type == TokenType::Eof && type != TokenType::Eof)
			fatalError(coda::ParseErrorCode::UnexpectedEOF,
			           "expected " + tokenToString.at(type)
			           + ", got " + tokenToString.at(current.type),
			           current.loc);
		if (current.type != type)
			fatalError(coda::ParseErrorCode::UnexpectedToken,
			           "expected " + tokenToString.at(type)
			           + ", got " + tokenToString.at(current.type),
			           current.loc);
		return advance();
	}

	Token expectKey() {
		if (current.type == TokenType::Eof)
			fatalError(coda::ParseErrorCode::UnexpectedEOF,
			           "expected key (identifier or string), got "
			           + tokenToString.at(current.type),
			           current.loc);
		if (current.type != TokenType::Ident && current.type != TokenType::String)
			fatalError(coda::ParseErrorCode::UnexpectedToken,
			           "expected key (identifier or string), got "
			           + tokenToString.at(current.type),
			           current.loc);
		return advance();
	}

	void skipNewlines() {
		while (true) {
			if (current.type == TokenType::Newline) {
				advance();
			} else if (current.type == TokenType::Comment) {
				if (!pendingComment.empty()) pendingComment += '\n';
				pendingComment += advance().value;
			} else {
				break;
			}
		}
	}

	void expectLineEnd() {
		if (current.type == TokenType::Comment)
			advance();

		if (current.type != TokenType::Newline
		 && current.type != TokenType::Eof
		 && current.type != TokenType::RBrace
		 && current.type != TokenType::RBracket)
			fatalError(coda::ParseErrorCode::ContentAfterBrace,
			           "unexpected content — must be on new line",
			           current.loc);
		skipNewlines();
	}

	bool isLineEnd() const {
		return current.type == TokenType::Newline
		    || current.type == TokenType::RBracket
		    || current.type == TokenType::RBrace
		    || current.type == TokenType::Eof;
	}

	// ── diagnostics ─────────────────────────────────────────────────────

	std::string extractLine(size_t start) const {
		size_t end = source.find_first_of("\r\n", start);
		if (end == std::string::npos) end = source.size();
		return source.substr(start, end - start);
	}

	[[noreturn]]
	void fatalError(coda::ParseErrorCode code,
	                const std::string& msg,
	                const SourceLoc& loc)
	{
		throw coda::ParseError(code, loc, msg, filename, extractLine(loc.lineStart));
	}

	// ── comment handling ────────────────────────────────────────────────

	std::string takeComment() {
		std::string c = std::move(pendingComment);
		pendingComment.clear();
		return c;
	}

	// ── duplicate-key guard for Block ───────────────────────────────────

	// Inserts directly into the Block's underlying map so we can check for
	// duplicates.  Block::operator[] auto-inserts without a duplicate check,
	// and Block::insert() doesn't report whether the key already existed, so
	// we access getContent() directly here.
	void blockInsertChecked(coda::Block& block,
	                        const std::string& key,
	                        coda::detail::Value value,
	                        const SourceLoc& loc)
	{
		auto& map = block.getContent();
		if (map.count(key))
			fatalError(coda::ParseErrorCode::DuplicateKey,
			           "duplicate key '" + key + "'", loc);
		map[key] = std::make_unique<coda::detail::Value>(std::move(value));
	}

	// Same guard for KeyedTable rows.
	void keyedTableInsertChecked(coda::KeyedTable& table,
	                             const std::string& key,
	                             coda::Row row,
	                             const SourceLoc& loc)
	{
		auto& map = table.getContent();
		if (map.count(key))
			fatalError(coda::ParseErrorCode::DuplicateKey,
			           "duplicate key '" + key + "'", loc);
		map[key] = std::move(row);
	}

	void checkUniqueFields(const std::vector<Token>& fieldToks) {
		std::set<std::string> seen;
		for (const auto& tok : fieldToks)
			if (!seen.insert(tok.value).second)
				fatalError(coda::ParseErrorCode::DuplicateField,
				           "duplicate field '" + tok.value + "' in table header",
				           tok.loc);
	}

	// ── row collection ──────────────────────────────────────────────────

	std::vector<Token> collectFlatRow() {
		std::vector<Token> row;
		while (!isLineEnd()) {
			if (current.type == TokenType::LBrace || current.type == TokenType::LBracket)
				fatalError(coda::ParseErrorCode::NestedBlock,
				           "nested blocks not allowed in tabular context",
				           current.loc);
			row.push_back(advance());
		}
		return row;
	}

	// ── value parsing ───────────────────────────────────────────────────

	coda::detail::Value parseValue() {
		std::string comment = takeComment();

		checkNotError();

		coda::detail::Value v;
		if (current.type == TokenType::LBrace) {
			v = coda::detail::Value(parseBlock());
		} else if (current.type == TokenType::LBracket) {
			v = parseArray();
		} else if (current.type == TokenType::Ident
		        || current.type == TokenType::String
		        || current.type == TokenType::Key) {
			// TokenType::Key ('key') is reserved as a table header marker, but
			// when it appears in a value position it is just the string "key".
			v = coda::detail::Value(advance().value);
		} else {
			fatalError(coda::ParseErrorCode::UnexpectedToken,
			           "expected value (string, identifier, block, or array), got "
			           + tokenToString.at(current.type),
			           current.loc);
		}

		v.setComment(std::move(comment));
		return v;
	}

	coda::Block parseBlock() {
		expect(TokenType::LBrace);
		expectLineEnd();

		coda::Block block;
		while (current.type != TokenType::RBrace && current.type != TokenType::Eof) {
			if (current.type == TokenType::Key)
				fatalError(coda::ParseErrorCode::KeyInBlock,
				           "'key' header not allowed inside block — use [] for tables",
				           current.loc);

			Token keyTok = expectKey();
			coda::detail::Value val = parseValue();
			blockInsertChecked(block, keyTok.value, std::move(val), keyTok.loc);
			skipNewlines();
		}

		expect(TokenType::RBrace);
		return block;
	}

	// ── array / table parsing ───────────────────────────────────────────

	coda::detail::Value parseArray() {
		expect(TokenType::LBracket);
		expectLineEnd();

		if (current.type == TokenType::Key) {
			std::string headerComment = takeComment();
			return parseKeyedTable(std::move(headerComment));
		}
		if (current.type == TokenType::LBrace || current.type == TokenType::LBracket)
			return parseNestedList();
		return parseAutoList();
	}

	// Produces a KeyedTable: rows are indexed by their first token ("key" column).
	coda::detail::Value parseKeyedTable(std::string headerComment) {
		advance(); // consume 'key'

		std::vector<Token> fieldToks;
		while (current.type == TokenType::Ident || current.type == TokenType::String)
			fieldToks.push_back(advance());
		checkUniqueFields(fieldToks);
		skipNewlines();

		coda::KeyedTable table;
		table.setHeaderComment(std::move(headerComment));

		while (current.type != TokenType::RBracket && current.type != TokenType::Eof) {
			std::string comment = takeComment();
			auto rowTokens = collectFlatRow();
			skipNewlines();
			if (rowTokens.empty()) continue;

			if (rowTokens.size() - 1 != fieldToks.size())
				fatalError(
					coda::ParseErrorCode::RaggedRow,
					"row '" + rowTokens[0].value + "' has "
					+ std::to_string(rowTokens.size() - 1) + " value(s), expected "
					+ std::to_string(fieldToks.size()),
					rowTokens[0].loc
				);

			coda::Row row;
			row.setComment(std::move(comment));
			for (size_t i = 0; i < fieldToks.size(); ++i)
				row[fieldToks[i].value] = rowTokens[i + 1].value;

			keyedTableInsertChecked(table, rowTokens[0].value, std::move(row), rowTokens[0].loc);
		}

		expect(TokenType::RBracket);
		return coda::detail::Value(std::move(table));
	}

	// Produces an Array of nested blocks/arrays.
	coda::detail::Value parseNestedList() {
		coda::Array array;
		while (current.type != TokenType::RBracket && current.type != TokenType::Eof) {
			skipNewlines();
			if (current.type == TokenType::RBracket) break;
			array.append(parseValue());
			skipNewlines();
		}

		expect(TokenType::RBracket);
		return coda::detail::Value(std::move(array));
	}

	// Dispatches to either parsePlainTable (multi-column header row) or
	// parseBareList (single-column / no header).
	coda::detail::Value parseAutoList() {
		std::string firstComment = takeComment();
		auto firstRow = collectFlatRow();
		skipNewlines();

		if (firstRow.size() > 1)
			return parsePlainTable(std::move(firstRow), std::move(firstComment));
		return parseBareList(std::move(firstRow), std::move(firstComment));
	}

	// Produces a Table: the first row is the column-name header; subsequent
	// rows become Row objects appended in order.
	coda::detail::Value parsePlainTable(std::vector<Token> header, std::string headerComment) {
		checkUniqueFields(header);

		std::set<std::string> headerSet;
		for (const auto& tok : header) headerSet.insert(tok.value);
		coda::Table table(std::move(headerSet));
		table.setHeaderComment(std::move(headerComment));

		while (current.type != TokenType::RBracket && current.type != TokenType::Eof) {
			std::string comment = takeComment();
			auto rowTokens = collectFlatRow();
			skipNewlines();
			if (rowTokens.empty()) continue;

			if (rowTokens.size() != header.size())
				fatalError(
					coda::ParseErrorCode::RaggedRow,
					"row has " + std::to_string(rowTokens.size())
					+ " value(s), expected " + std::to_string(header.size()),
					rowTokens[0].loc
				);

			coda::Row row;
			row.setComment(std::move(comment));
			for (size_t i = 0; i < header.size(); ++i)
				row[header[i].value] = rowTokens[i].value;

			table.append(std::move(row));
		}

		expect(TokenType::RBracket);
		return coda::detail::Value(std::move(table));
	}

	// Produces an Array of string Values (bare list, one token per line).
	coda::detail::Value parseBareList(std::vector<Token> firstRow, std::string firstComment) {
		coda::Array array;

		if (!firstRow.empty()) {
			coda::detail::Value firstVal(firstRow[0].value);
			firstVal.setComment(std::move(firstComment));
			array.append(std::move(firstVal));
		}

		while (current.type != TokenType::RBracket && current.type != TokenType::Eof) {
			skipNewlines();
			if (current.type == TokenType::RBracket) break;
			array.append(parseValue());
			skipNewlines();
		}

		expect(TokenType::RBracket);
		return coda::detail::Value(std::move(array));
	}

public:
	// ── constructor ─────────────────────────────────────────────────────

	Parser(std::string src, std::string filename = "")
		: source(std::move(src))
		, filename(std::move(filename))
		, lexer(source)
		, current(lexer.next())
		, lookahead(lexer.next())
	{}

	// ── public interface ────────────────────────────────────────────────

	coda::Block parse() {
		coda::Block root;
		skipNewlines();
		while (current.type != TokenType::Eof) {
			Token keyTok            = expectKey();
			coda::detail::Value val = parseValue();
			blockInsertChecked(root, keyTok.value, std::move(val), keyTok.loc);
			skipNewlines();
		}
		return root;
	}

};

} // namespace detail

} // namespace coda
