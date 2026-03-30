#pragma once
#include "ast.hpp"
#include <stdexcept>
#include <string>
#include <vector>
#include <map>

enum class TokenType {
	Ident,
	String,
	Key,
	LBrace,
	RBrace,
	LBracket,
	RBracket,
	Newline,
	Eof
};

struct Token {
	TokenType   type;
	std::string value;
};

class Lexer {
	std::string src;
	size_t      pos = 0;

	char peek()    { return pos < src.size() ? src[pos] : '\0'; }
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
		if (val == "key") return {TokenType::Key, val};
		return {TokenType::Ident, val};
	}
};

class Parser {
	Lexer lexer;
	Token current;
	Token lookahead;

	Token advance() {
		Token t   = current;
		current   = lookahead;
		lookahead = lexer.next();
		return t;
	}

	Token expect(TokenType type) {
		if (current.type != type)
			throw std::runtime_error("unexpected token: '" + current.value + "'");
		return advance();
	}

	void skipNewlines() {
		while (current.type == TokenType::Newline) advance();
	}

	// collect tokens on the current line into a row
	std::vector<std::string> collectRow() {
		std::vector<std::string> row;
		while (current.type != TokenType::Newline
			&& current.type != TokenType::RBracket
			&& current.type != TokenType::RBrace
			&& current.type != TokenType::Eof) {
			row.push_back(advance().value);
		}
		return row;
	}

public:
	Parser(std::string src) : lexer(std::move(src)) {
		current   = lexer.next();
		lookahead = lexer.next();
	}

	ZifFile parseFile() {
		ZifFile file;
		skipNewlines();
		while (current.type != TokenType::Eof) {
			std::string key        = expect(TokenType::Ident).value;
			Ptr<ZifValue> value    = parseValue();
			file.statements[key]   = std::move(value);
			skipNewlines();
		}
		return file;
	}

private:
	Ptr<ZifValue> parseValue() {
		skipNewlines();
		if (current.type == TokenType::LBrace)
			return makePtr<ZifValue>(ZifValue{parseBlock()});
		if (current.type == TokenType::LBracket)
			return makePtr<ZifValue>(ZifValue{parseArray()});
		std::string val = current.value;
		advance();
		return makePtr<ZifValue>(ZifValue{val});
	}

	ZifBlock parseBlock() {
		expect(TokenType::LBrace);
		skipNewlines();
		ZifBlock block;

		if (current.type == TokenType::Key) {
			// keyed table mode
			advance(); // consume "key"
			std::vector<std::string> fields;
			while (current.type == TokenType::Ident
				|| current.type == TokenType::String) {
				fields.push_back(advance().value);
			}
			skipNewlines();

			std::map<std::string, ZifTable> table;
			while (current.type != TokenType::RBrace
				&& current.type != TokenType::Eof) {
				std::vector<std::string> row = collectRow();
				if (row.empty()) { skipNewlines(); continue; }
				// first value is the key, rest map to fields
				std::string rowKey = row[0];
				ZifTable entry;
				for (size_t i = 0; i < fields.size() && (i + 1) < row.size(); i++)
					entry.content[fields[i]] = row[i + 1];
				table[rowKey] = std::move(entry);
				skipNewlines();
			}
			block.content = std::move(table);

		} else {
			// struct mode: named children
			std::map<std::string, Ptr<ZifValue>> children;
			while (current.type != TokenType::RBrace
				&& current.type != TokenType::Eof) {
				std::string key     = expect(TokenType::Ident).value;
				Ptr<ZifValue> value = parseValue();
				children[key]       = std::move(value);
				skipNewlines();
			}
			block.content = std::move(children);
		}

		expect(TokenType::RBrace);
		return block;
	}

	ZifArray parseArray() {
		expect(TokenType::LBracket);
		skipNewlines();
		ZifArray array;

		if (current.type == TokenType::Key) {
			// keyed table
			advance();
			std::vector<std::string> fields;
			while (current.type == TokenType::Ident
				|| current.type == TokenType::String) {
				fields.push_back(advance().value);
			}
			skipNewlines();

			std::map<std::string, ZifTable> table;
			while (current.type != TokenType::RBracket
				&& current.type != TokenType::Eof) {
				std::vector<std::string> row = collectRow();
				if (row.empty()) { skipNewlines(); continue; }
				std::string rowKey = row[0];
				ZifTable entry;
				for (size_t i = 0; i < fields.size() && (i + 1) < row.size(); i++)
					entry.content[fields[i]] = row[i + 1];
				table[rowKey] = std::move(entry);
				skipNewlines();
			}
			array.content = std::move(table);

		} else {
			// peek at first row to decide: list or plain table
			std::vector<std::vector<std::string>> rows;
			while (current.type != TokenType::RBracket
				&& current.type != TokenType::Eof) {
				std::vector<std::string> row = collectRow();
				if (!row.empty()) rows.push_back(std::move(row));
				skipNewlines();
			}

			bool isTable = !rows.empty() && rows[0].size() > 1;
			if (isTable) {
				// first row is the header
				std::vector<std::string> fields = rows[0];
				std::vector<ZifTable> table;
				for (size_t i = 1; i < rows.size(); i++) {
					ZifTable entry;
					for (size_t j = 0; j < fields.size() && j < rows[i].size(); j++)
						entry.content[fields[j]] = rows[i][j];
					table.push_back(std::move(entry));
				}
				array.content = std::move(table);
			} else {
				// bare list
				std::vector<std::string> list;
				for (auto& row : rows)
				if (!row.empty()) list.push_back(row[0]);
				array.content = std::move(list);
			}
		}

		expect(TokenType::RBracket);
		return array;
	}
};
