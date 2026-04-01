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
	CommentBeforeHeader,

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
		if (c == '\n') { ++line_; lineStart = pos; }
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

	std::vector<coda::ParseError> errors_;

	// ── token helpers ───────────────────────────────────────────────────

	Token advance() {
		if (current.type == TokenType::Error) {
			coda::ParseErrorCode code;
			if (current.value.find("unterminated") != std::string::npos)
				code = coda::ParseErrorCode::UnterminatedString;
			else if (current.value.find("escape") != std::string::npos)
				code = coda::ParseErrorCode::InvalidEscape;
			else
				code = coda::ParseErrorCode::UnexpectedToken;

			fatalError(code,
			           current.value,
			           current.loc);
		}
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

	void recordError(coda::ParseErrorCode code,
	                 const std::string& msg,
	                 const SourceLoc& loc)
	{
		std::string srcLine = extractLine(loc.lineStart);
		errors_.emplace_back(code, loc, msg, filename, srcLine);
	}

	[[noreturn]]
	void fatalError(coda::ParseErrorCode code,
	                const std::string& msg,
	                const SourceLoc& loc)
	{
		recordError(code, msg, loc);
		throw errors_.back();
	}

	void synchronize() {
		while (current.type != TokenType::Newline
		    && current.type != TokenType::Eof
		    && current.type != TokenType::RBrace
		    && current.type != TokenType::RBracket)
		{
			current   = lookahead;
			lookahead = lexer.next();
		}
		skipNewlines();
	}

	// ── comment handling ────────────────────────────────────────────────

	std::string takeComment() {
		std::string c = std::move(pendingComment);
		pendingComment.clear();
		return c;
	}

	CodaValue withComment(CodaValue v) {
		v.comment = takeComment();
		return v;
	}

	// ── comment-before-header check ─────────────────────────────────────

	void checkNoOrphanComment() {
		if (!pendingComment.empty())
			fatalError(coda::ParseErrorCode::CommentBeforeHeader,
			           "comments are not allowed before a table header — "
			           "place the comment before the array",
			           current.loc);
	}

	// ── duplicate-key guards ────────────────────────────────────────────

	void insertChecked(OrderedMap<std::string, CodaValue>& map,
	                   const std::string& key, CodaValue value,
	                   const SourceLoc& loc)
	{
		auto [it, inserted] = map.insert(key, std::move(value));
		if (!inserted)
			fatalError(coda::ParseErrorCode::DuplicateKey,
			           "duplicate key '" + key + "'", loc);
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

	CodaValue parseValue() {
		std::string comment = takeComment();

		CodaValue v;
		if (current.type == TokenType::LBrace)        v = parseBlock();
		else if (current.type == TokenType::LBracket)  v = parseArray();
		else                                           v = advance().value;

		v.comment = std::move(comment);
		return v;
	}

	CodaBlock parseBlock() {
		expect(TokenType::LBrace);
		expectLineEnd();

		CodaBlock block;
		while (current.type != TokenType::RBrace && current.type != TokenType::Eof) {
			if (current.type == TokenType::Key)
				fatalError(coda::ParseErrorCode::KeyInBlock,
				           "'key' header not allowed inside block — use [] for tables",
				           current.loc);

			Token keyTok  = expectKey();
			CodaValue val = parseValue();
			insertChecked(block.content, keyTok.value, std::move(val), keyTok.loc);
			skipNewlines();
		}

		expect(TokenType::RBrace);
		return block;
	}

	// ── array / table parsing ───────────────────────────────────────────

	CodaValue parseArray() {
		expect(TokenType::LBracket);
		expectLineEnd();

		if (current.type == TokenType::Key) {
			checkNoOrphanComment();
			return parseKeyedTable();
		}
		if (current.type == TokenType::LBrace || current.type == TokenType::LBracket)
			return parseNestedList();
		return parseAutoList();
	}

	CodaValue parseKeyedTable() {
		advance(); // consume 'key'

		std::vector<Token> fieldToks;
		while (current.type == TokenType::Ident || current.type == TokenType::String)
			fieldToks.push_back(advance());
		checkUniqueFields(fieldToks);
		skipNewlines();

		CodaTable table;
		while (current.type != TokenType::RBracket && current.type != TokenType::Eof) {
			std::string comment = takeComment();
			auto row = collectFlatRow();
			skipNewlines();
			if (row.empty()) continue;

			if (row.size() - 1 != fieldToks.size())
				fatalError(
					coda::ParseErrorCode::RaggedRow,
					"row '" + row[0].value + "' has " + std::to_string(row.size() - 1) + " value(s), expected " + std::to_string(fieldToks.size()),
					row[0].loc
				);

			CodaTable entry;
			for (size_t i = 0; i < fieldToks.size() && (i + 1) < row.size(); ++i)
				entry.content[fieldToks[i].value] = CodaValue(row[i + 1].value);

			CodaValue entryVal{std::move(entry)};
			entryVal.comment = std::move(comment);
			insertChecked(table.content, row[0].value, std::move(entryVal), row[0].loc);
		}

		expect(TokenType::RBracket);
		return CodaValue{std::move(table)};
	}

	CodaValue parseNestedList() {
		CodaArray array;
		while (current.type != TokenType::RBracket && current.type != TokenType::Eof) {
			skipNewlines();
			if (current.type == TokenType::RBracket) break;
			array.content.push_back(parseValue());
			skipNewlines();
		}

		expect(TokenType::RBracket);
		return CodaValue{std::move(array)};
	}

	CodaValue parseAutoList() {
		std::string firstComment = takeComment();
		auto firstRow = collectFlatRow();
		skipNewlines();

		if (firstRow.size() > 1) {
			if (!firstComment.empty())
				fatalError(coda::ParseErrorCode::CommentBeforeHeader,
			   "comments are not allowed before a table header — "
			   "place the comment before the array",
			   current.loc);
			return parsePlainTable(std::move(firstRow));
		}
		return parseBareList(std::move(firstRow), std::move(firstComment));
	}

	CodaValue parsePlainTable(std::vector<Token> header) {
		checkUniqueFields(header);

		CodaArray array;
		while (current.type != TokenType::RBracket && current.type != TokenType::Eof) {
			std::string comment = takeComment();
			auto row = collectFlatRow();
			skipNewlines();
			if (row.empty()) continue;

			if (row.size() != header.size())
				fatalError(
					coda::ParseErrorCode::RaggedRow,
					"row has " + std::to_string(row.size()) + " value(s), expected " + std::to_string(header.size()),
					row[0].loc
				);

			CodaTable entry;
			for (size_t i = 0; i < header.size() && i < row.size(); ++i)
				entry.content[header[i].value] = CodaValue(row[i].value);

			CodaValue entryVal{std::move(entry)};
			entryVal.comment = std::move(comment);
			array.content.push_back(std::move(entryVal));
		}

		expect(TokenType::RBracket);
		return CodaValue{std::move(array)};
	}

	CodaValue parseBareList(std::vector<Token> firstRow, std::string firstComment) {
		CodaArray array;
		if (!firstRow.empty()) {
			CodaValue firstVal{firstRow[0].value};
			firstVal.comment = std::move(firstComment);
			array.content.push_back(std::move(firstVal));
		}

		while (current.type != TokenType::RBracket && current.type != TokenType::Eof) {
			skipNewlines();
			if (current.type == TokenType::RBracket) break;
			array.content.push_back(parseValue());
			skipNewlines();
		}

		expect(TokenType::RBracket);
		return CodaValue{std::move(array)};
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

	CodaFile parse() {
		CodaFile file;
		skipNewlines();
		while (current.type != TokenType::Eof) {
			Token keyTok  = expectKey();
			CodaValue val = parseValue();
			insertChecked(file.statements, keyTok.value, std::move(val), keyTok.loc);
			skipNewlines();
		}
		return file;
	}

	const std::vector<coda::ParseError>& errors() const { return errors_; }
	bool hasErrors() const { return !errors_.empty(); }
};

} // namespace detail

} // namespace coda
