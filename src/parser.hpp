#pragma once
#include "ast.hpp"
#include "console.hpp"

#include <set>
#include <string>
#include <vector>
#include <map>

// ─── Token types ────────────────────────────────────────────────────────────

enum class TokenType {
	Ident, String, Key, Comment,
	LBrace, RBrace,
	LBracket, RBracket,
	Newline, Eof
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
};

// ─── Source location ────────────────────────────────────────────────────────

struct SourceLoc {
	int    line      = 1;
	int    col       = 1;
	size_t lineStart = 0;
};

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
		if (c == '\n') { ++line_; lineStart = pos; }
		return c;
	}

	void skipHorizontal() {
		while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\t'))
			++pos;
	}

	SourceLoc loc() const {
		return { line_, static_cast<int>(pos - lineStart) + 1, lineStart };
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
			while (pos < src.size() && peek() != '"')
				val += advance();
			if (pos < src.size()) advance();
			return { TokenType::String, val, tokenLoc };
		}

		if (c == '#') {
			std::string val;
			while (pos < src.size() && peek() != '\n' && peek() != '\r')
				val += advance();
			return { TokenType::Comment, val, tokenLoc };
		}

		std::string val;
		while (pos < src.size()
			&& !std::isspace(peek())
			&& peek() != '{' && peek() != '}'
			&& peek() != '[' && peek() != ']'
			&& peek() != '"')
		{
			val += advance();
		}

		if (val == "key") return { TokenType::Key, val, tokenLoc };
		return { TokenType::Ident, val, tokenLoc };
	}
};

// ─── Parser ─────────────────────────────────────────────────────────────────

class Parser {
	std::string source;          // must be declared before lexer
	Lexer       lexer;
	Token       current;
	Token       lookahead;
	std::string pendingComment;

	// ── token helpers ───────────────────────────────────────────────────

	Token advance() {
		Token t   = current;
		current   = lookahead;
		lookahead = lexer.next();
		return t;
	}

	// ── diagnostics ─────────────────────────────────────────────────────

	std::string extractLine(size_t start) const {
		size_t end = source.find_first_of("\r\n", start);
		if (end == std::string::npos) end = source.size();
		return source.substr(start, end - start);
	}

	[[noreturn]] void error(const std::string& msg, const SourceLoc& loc) {
		std::string srcLine = extractLine(loc.lineStart);

		// mirror whitespace characters so ^ aligns regardless of tab width
		std::string pointer;
		for (int i = 0; i < loc.col - 1 && i < (int)srcLine.size(); ++i)
			pointer += (srcLine[i] == '\t') ? '\t' : ' ';
		pointer += '^';

		PRINT("error (line " + std::to_string(loc.line)
			+ ", col " + std::to_string(loc.col) + "): " + msg
			+ "\n" + srcLine
			+ "\n" + pointer);
		throw "";
	}

	[[noreturn]] void error(const std::string& msg) {
		error(msg, current.loc);
	}

	Token expect(TokenType type) {
		if (current.type != type)
			error("expected " + tokenToString.at(type)
			    + ", got " + tokenToString.at(current.type));
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

	bool isLineEnd() const {
		return current.type == TokenType::Newline
		    || current.type == TokenType::RBracket
		    || current.type == TokenType::RBrace
		    || current.type == TokenType::Eof;
	}

	// ── value construction (drains pending comment) ─────────────────────

	template<typename T>
	CodaValue makeValue(T&& content) {
		CodaValue v{ std::forward<T>(content) };
		if (!pendingComment.empty()) {
			v.comment = std::move(pendingComment);
			pendingComment.clear();
		}
		return v;
	}

	// ── duplicate-key guards ────────────────────────────────────────────

	void insertChecked(OrderedMap<std::string, CodaValue>& map,
					const std::string& key, CodaValue value,
					const SourceLoc& loc) {
		auto [it, inserted] = map.insert(key, std::move(value));
		if (!inserted)
			error("duplicate key '" + key + "'", loc);
	}

	void checkUniqueFields(const std::vector<Token>& fieldToks) {
		std::set<std::string> seen;
		for (const auto& tok : fieldToks)
			if (!seen.insert(tok.value).second)
				error("duplicate field '" + tok.value + "' in table header", tok.loc);
	}

	// ── flat row collection (tabular contexts, no nesting) ──────────────

	std::vector<Token> collectFlatRow() {
		std::vector<Token> row;
		while (!isLineEnd()) {
			if (current.type == TokenType::LBrace || current.type == TokenType::LBracket)
				error("nested blocks not allowed in tabular context");
			row.push_back(advance());
		}
		return row;
	}

public:
	Parser(std::string src)
		: source(std::move(src))
		, lexer(source)
		, current(lexer.next())
		, lookahead(lexer.next())
	{}

	CodaFile parseFile() {
		CodaFile file;
		skipNewlines();
		while (current.type != TokenType::Eof) {
			Token keyTok  = expect(TokenType::Ident);
			CodaValue val = parseValue();
			insertChecked(file.statements, keyTok.value, std::move(val), keyTok.loc);
			skipNewlines();
		}
		return file;
	}

private:
	// ── value dispatch ──────────────────────────────────────────────────

	CodaValue parseValue() {
		if (current.type == TokenType::LBrace)   return makeValue(parseBlock());
		if (current.type == TokenType::LBracket) return parseArray();
		return makeValue(std::string(advance().value));
	}

	// ── block: { key value ... } ────────────────────────────────────────

	CodaBlock parseBlock() {
		expect(TokenType::LBrace);
		skipNewlines();

		CodaBlock block;
		while (current.type != TokenType::RBrace && current.type != TokenType::Eof) {
			if (current.type == TokenType::Key)
				error("'key' header not allowed inside block — use [] for tables");

			Token keyTok  = expect(TokenType::Ident);
			CodaValue val = parseValue();
			insertChecked(block.content, keyTok.value, std::move(val), keyTok.loc);
			skipNewlines();
		}

		expect(TokenType::RBrace);
		return block;
	}

	// ── array / table: [ ... ] ──────────────────────────────────────────

	CodaValue parseArray() {
		expect(TokenType::LBracket);
		skipNewlines();

		if (current.type == TokenType::Key)                                          return parseKeyedTable();
		if (current.type == TokenType::LBrace || current.type == TokenType::LBracket) return parseNestedList();
		return parseAutoList();
	}

	// [ key field1 field2 ... \n  rowKey v1 v2 ... ]
	CodaValue parseKeyedTable() {
		advance(); // consume 'key'

		std::vector<Token> fieldToks;
		while (current.type == TokenType::Ident || current.type == TokenType::String)
			fieldToks.push_back(advance());
		checkUniqueFields(fieldToks);
		skipNewlines();

		CodaTable table;
		while (current.type != TokenType::RBracket && current.type != TokenType::Eof) {
			auto row = collectFlatRow();
			skipNewlines();
			if (row.empty()) continue;

			CodaTable entry;
			for (size_t i = 0; i < fieldToks.size() && (i + 1) < row.size(); ++i)
				entry.content[fieldToks[i].value] = CodaValue(row[i + 1].value);

			insertChecked(table.content, row[0].value,
			              makeValue(std::move(entry)), row[0].loc);
		}

		expect(TokenType::RBracket);
		return makeValue(std::move(table));
	}

	// [ {…} {…} … ]  or  [ […] […] … ]
	CodaValue parseNestedList() {
		CodaArray array;
		while (current.type != TokenType::RBracket && current.type != TokenType::Eof) {
			skipNewlines();
			if (current.type == TokenType::RBracket) break;
			array.content.push_back(parseValue());
			skipNewlines();
		}

		expect(TokenType::RBracket);
		return makeValue(std::move(array));
	}

	// peek first line to decide: plain table (multi-word header) or bare list
	CodaValue parseAutoList() {
		auto firstRow = collectFlatRow();
		skipNewlines();

		if (firstRow.size() > 1) return parsePlainTable(std::move(firstRow));
		return parseBareList(std::move(firstRow));
	}

	// [ h1 h2 … \n  v1 v2 … \n  … ]
	CodaValue parsePlainTable(std::vector<Token> header) {
		checkUniqueFields(header);

		CodaArray array;
		while (current.type != TokenType::RBracket && current.type != TokenType::Eof) {
			auto row = collectFlatRow();
			skipNewlines();
			if (row.empty()) continue;

			CodaTable entry;
			for (size_t i = 0; i < header.size() && i < row.size(); ++i)
				entry.content[header[i].value] = CodaValue(row[i].value);

			array.content.push_back(makeValue(std::move(entry)));
		}

		expect(TokenType::RBracket);
		return makeValue(std::move(array));
	}

	// [ item1 \n item2 \n … ]
	CodaValue parseBareList(std::vector<Token> firstRow) {
		CodaArray array;
		if (!firstRow.empty())
			array.content.push_back(makeValue(std::string(firstRow[0].value)));

		while (current.type != TokenType::RBracket && current.type != TokenType::Eof) {
			skipNewlines();
			if (current.type == TokenType::RBracket) break;
			array.content.push_back(parseValue());
			skipNewlines();
		}

		expect(TokenType::RBracket);
		return makeValue(std::move(array));
	}
};
