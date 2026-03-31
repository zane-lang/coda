#pragma once
#include "ast.hpp"
#include "console.hpp"

#include <stdexcept>
#include <string>
#include <vector>
#include <map>

enum class TokenType {
	Ident, String, Key, Comment,
	LBrace, RBrace,
	LBracket, RBracket,
	Newline, Eof
};

const std::map<TokenType, std::string> tokenToString = {
	{ TokenType::Ident, "Ident" },
	{ TokenType::String, "String" },
	{ TokenType::Key, "Key" },
	{ TokenType::Comment, "Comment" },

	{ TokenType::LBrace, "LBrace" },
	{ TokenType::RBrace, "RBrace" },
	{ TokenType::LBracket, "LBracket" },
	{ TokenType::RBracket, "RBracket" },

	{ TokenType::Newline, "Newline" },
	{ TokenType::Eof, "Eof" }
};

struct Token {
	TokenType   type;
	std::string value;
};

class Lexer {
	std::string src;
	size_t      pos = 0;

	char peek() { return pos < src.size() ? src[pos] : '\0'; }
	char advance() { return src[pos++]; }

	void skipHorizontal() {
		while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\t'))
			pos++;
	}

public:
	Lexer(std::string src) : src(std::move(src)) {}

	Token next() {
		skipHorizontal();
		if (pos >= src.size()) return {TokenType::Eof, ""};
		char c = peek();
		if (c == '\n' || c == '\r') {
			while (pos < src.size() && (src[pos] == '\n' || src[pos] == '\r'))
				pos++;
			return {TokenType::Newline, ""};
		}
		if (c == '{') { advance(); return {TokenType::LBrace,   "{"}; }
		if (c == '}') { advance(); return {TokenType::RBrace,   "}"}; }
		if (c == '[') { advance(); return {TokenType::LBracket, "["}; }
		if (c == ']') { advance(); return {TokenType::RBracket, "]"}; }
		if (c == '"') {
			advance();
			std::string val;
			while (peek() != '"' && pos < src.size()) val += advance();
			advance();
			return {TokenType::String, val};
		}
		std::string val;
		while (pos < src.size()
			&& !std::isspace(peek())
			&& peek() != '{' && peek() != '}'
			&& peek() != '[' && peek() != ']'
			&& peek() != '"') {
			val += advance();
		}

		if (c == '#') {
			while (peek() != '\n' && peek() != '\r') val += advance();
			return {TokenType::Comment, val};
		}

		if (val == "key") return {TokenType::Key, val};
		return {TokenType::Ident, val};
	}
};

class Parser {
	Lexer lexer;
	Token current;
	Token lookahead;
	std::string pendingComment;

	Token advance() {
		Token t   = current;
		current   = lookahead;
		lookahead = lexer.next();
		return t;
	}

	Token expect(TokenType type) {
		if (current.type != type) {
			PRINT("Got token " + tokenToString.at(current.type) + ", expected " + tokenToString.at(type));
			throw "";
		}
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

	bool isLineEnd() {
		return current.type == TokenType::Newline
			|| current.type == TokenType::RBracket
			|| current.type == TokenType::RBrace
			|| current.type == TokenType::Eof;
	}

	// Collect flat string tokens on current line — used in tabular contexts.
	std::vector<std::string> collectFlatRow() {
		std::vector<std::string> row;
		while (!isLineEnd()) {
			if (current.type == TokenType::LBrace || current.type == TokenType::LBracket)
				throw std::runtime_error("nested blocks not allowed in tabular context");
			row.push_back(advance().value);
		}
		return row;
	}

	// Central point for all CodaValue construction — always drains pendingComment.
	template<typename T>
	CodaValue makeValue(T&& content) {
		CodaValue v{std::forward<T>(content)};
		if (!pendingComment.empty()) {
			v.comment = std::move(pendingComment);
			pendingComment.clear();
		}
		return v;
	}

public:
	Parser(std::string src) : lexer(std::move(src)) {
		current   = lexer.next();
		lookahead = lexer.next();
	}

	CodaFile parseFile() {
		CodaFile file;
		skipNewlines();
		while (current.type != TokenType::Eof) {
			std::string key = expect(TokenType::Ident).value;
			file.statements[key] = parseValue();
			skipNewlines();
		}
		return file;
	}

private:
	CodaValue parseValue() {
		if (current.type == TokenType::LBrace)
			return makeValue(parseBlock());
		if (current.type == TokenType::LBracket)
			return parseArray();

		std::string val = current.value;
		advance();
		return makeValue(std::move(val));
	}

	CodaBlock parseBlock() {
		expect(TokenType::LBrace);
		skipNewlines();
		CodaBlock block;

		std::map<std::string, CodaValue> children;

		while (current.type != TokenType::RBrace && current.type != TokenType::Eof) {
			if (current.type == TokenType::Newline) { advance(); continue; }
			if (current.type == TokenType::Key)
				throw std::runtime_error("key header not allowed in block — use [] for tables");

			std::string key = expect(TokenType::Ident).value;
			children[key] = parseValue();
			skipNewlines();
		}
		block.content = std::move(children);

		expect(TokenType::RBrace);
		return block;
	}

	// Returns CodaValue because the bracket syntax can produce either a CodaArray or CodaTable.
	CodaValue parseArray() {
		expect(TokenType::LBracket);
		skipNewlines();

		if (current.type == TokenType::Key) {
			// Keyed table — no nesting allowed.
			advance();
			std::vector<std::string> fields;
			while (current.type == TokenType::Ident || current.type == TokenType::String)
				fields.push_back(advance().value);
			skipNewlines();

			CodaTable table;
			while (current.type != TokenType::RBracket && current.type != TokenType::Eof) {
				auto row = collectFlatRow();
				skipNewlines();
				if (row.empty()) continue;
				std::string rowKey = row[0];
				CodaTable entry;
				for (size_t i = 0; i < fields.size() && (i + 1) < row.size(); i++)
					entry.content[fields[i]] = row[i + 1];
				table.content[rowKey] = makeValue(std::move(entry));
			}

			expect(TokenType::RBracket);
			return makeValue(std::move(table));

		} else if (current.type == TokenType::LBrace
				|| current.type == TokenType::LBracket) {
			// Starts with a nested value — bare list, nesting allowed.
			CodaArray array;
			std::vector<CodaValue> list;
			while (current.type != TokenType::RBracket && current.type != TokenType::Eof) {
				skipNewlines();
				if (current.type == TokenType::RBracket) break;
				list.push_back(parseValue());
				skipNewlines();
			}
			array.content = std::move(list);

			expect(TokenType::RBracket);
			return makeValue(std::move(array));

		} else {
			// Peek at first line to decide: plain table or bare list.
			auto firstRow = collectFlatRow();
			skipNewlines();

			if (firstRow.size() > 1) {
				// Plain table — firstRow is the header, no nesting allowed.
				CodaArray array;
				std::vector<CodaValue> rows;
				while (current.type != TokenType::RBracket && current.type != TokenType::Eof) {
					auto row = collectFlatRow();
					skipNewlines();
					if (row.empty()) continue;
					CodaTable entry;
					for (size_t i = 0; i < firstRow.size() && i < row.size(); i++)
						entry.content[firstRow[i]] = row[i];
					rows.push_back(makeValue(std::move(entry)));
				}
				array.content = std::move(rows);

				expect(TokenType::RBracket);
				return makeValue(std::move(array));

			} else {
				// Bare list — nesting allowed for subsequent elements.
				CodaArray array;
				std::vector<CodaValue> list;
				if (!firstRow.empty())
					list.push_back(makeValue(std::string{firstRow[0]}));
				while (current.type != TokenType::RBracket && current.type != TokenType::Eof) {
					skipNewlines();
					if (current.type == TokenType::RBracket) break;
					list.push_back(parseValue());
					skipNewlines();
				}
				array.content = std::move(list);

				expect(TokenType::RBracket);
				return makeValue(std::move(array));
			}
		}
	}
};
