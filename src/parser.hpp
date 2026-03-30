#pragma once
#include "ast.hpp"
#include <stdexcept>
#include <string>
#include <vector>
#include <map>

enum class TokenType {
    Ident, String, Key,
    LBrace, RBrace,
    LBracket, RBracket,
    Newline, Eof
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

    bool isLineEnd() {
        return current.type == TokenType::Newline
            || current.type == TokenType::RBracket
            || current.type == TokenType::RBrace
            || current.type == TokenType::Eof;
    }

    // collect flat string tokens on current line — used in tabular contexts
    std::vector<std::string> collectFlatRow() {
        std::vector<std::string> row;
        while (!isLineEnd()) {
            if (current.type == TokenType::LBrace || current.type == TokenType::LBracket)
                throw std::runtime_error("nested blocks not allowed in tabular context");
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
            std::string key      = expect(TokenType::Ident).value;
            file.statements[key] = parseValue();
            skipNewlines();
        }
        return file;
    }

private:
    Ptr<ZifValue> parseValue() {
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
            // table mode — no nesting allowed
            advance();
            std::vector<std::string> fields;
            while (current.type == TokenType::Ident || current.type == TokenType::String)
                fields.push_back(advance().value);
            skipNewlines();

            std::map<std::string, ZifValue> table;
            while (current.type != TokenType::RBrace && current.type != TokenType::Eof) {
                auto row = collectFlatRow();
                skipNewlines();
                if (row.empty()) continue;
                std::string rowKey = row[0];
                ZifTable entry;
                for (size_t i = 0; i < fields.size() && (i + 1) < row.size(); i++)
                    entry.content[fields[i]] = row[i + 1];
                table[rowKey] = ZifValue{std::move(entry)};
            }
            block.content = std::move(table);

        } else {
            // struct mode — nesting allowed
            std::map<std::string, Ptr<ZifValue>> children;
            while (current.type != TokenType::RBrace && current.type != TokenType::Eof) {
                if (current.type == TokenType::Newline) { advance(); continue; }
                std::string key = expect(TokenType::Ident).value;
                children[key]   = parseValue();
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
            // keyed table — no nesting allowed
            advance();
            std::vector<std::string> fields;
            while (current.type == TokenType::Ident || current.type == TokenType::String)
                fields.push_back(advance().value);
            skipNewlines();

            std::map<std::string, ZifValue> table;
            while (current.type != TokenType::RBracket && current.type != TokenType::Eof) {
                auto row = collectFlatRow();
                skipNewlines();
                if (row.empty()) continue;
                std::string rowKey = row[0];
                ZifTable entry;
                for (size_t i = 0; i < fields.size() && (i + 1) < row.size(); i++)
                    entry.content[fields[i]] = row[i + 1];
                table[rowKey] = ZifValue{std::move(entry)};
            }
            array.content = std::move(table);

        } else if (current.type == TokenType::LBrace
                || current.type == TokenType::LBracket) {
            // starts with a nested value — bare list, nesting allowed
            std::vector<ZifValue> list;
            while (current.type != TokenType::RBracket && current.type != TokenType::Eof) {
                skipNewlines();
                if (current.type == TokenType::RBracket) break;
                list.push_back(*parseValue());
                skipNewlines();
            }
            array.content = std::move(list);

        } else {
            // peek at first line to decide: plain table or bare list
            auto firstRow = collectFlatRow();
            skipNewlines();

            if (firstRow.size() > 1) {
                // plain table — firstRow is the header, no nesting allowed
                std::vector<ZifValue> rows;
                while (current.type != TokenType::RBracket && current.type != TokenType::Eof) {
                    auto row = collectFlatRow();
                    skipNewlines();
                    if (row.empty()) continue;
                    ZifTable entry;
                    for (size_t i = 0; i < firstRow.size() && i < row.size(); i++)
                        entry.content[firstRow[i]] = row[i];
                    rows.push_back(ZifValue{std::move(entry)});
                }
                array.content = std::move(rows);

            } else {
                // bare list — nesting allowed for subsequent elements
                std::vector<ZifValue> list;
                if (!firstRow.empty())
                    list.push_back(ZifValue{firstRow[0]});
                while (current.type != TokenType::RBracket && current.type != TokenType::Eof) {
                    skipNewlines();
                    if (current.type == TokenType::RBracket) break;
                    list.push_back(*parseValue());
                    skipNewlines();
                }
                array.content = std::move(list);
            }
        }

        expect(TokenType::RBracket);
        return array;
    }
};
