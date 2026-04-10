#pragma once
#include "helpers/types.hpp"
#include "helpers/ordered_map.hpp"
#include <set>
#include <string>
#include <vector>
#include <functional>
#include <stdexcept>

namespace coda {

namespace detail { class Value; };

class Block;
class Array;
class KeyedTable;

// ─── detail helpers (no CodaValue dependency) ────────────────────────────────

namespace detail {

inline std::string pad(int level, const std::string& unit) {
	std::string out;
	for (int i = 0; i < level; ++i) out += unit;
	return out;
}

inline std::string serializeToken(const std::string& s) {
	if (s == "key") return "\"key\"";

	auto isBareChar = [](unsigned char c) -> bool {
		if (std::isspace(c)) return false;
		switch (c) {
			case '{': case '}':
			case '[': case ']':
			case '"': case '#':
				return false;
			default:
				return true;
		}
	};

	bool needsQuotes = s.empty();
	if (!needsQuotes)
		for (unsigned char c : s)
			if (!isBareChar(c)) { needsQuotes = true; break; }

	if (!needsQuotes) return s;

	std::string out = "\"";
	for (char c : s) {
		switch (c) {
			case '\n': out += "\\n";  break;
			case '\t': out += "\\t";  break;
			case '\r': out += "\\r";  break;
			case '"':  out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			default:   out += c;
		}
	}
	return out + '"';
}

inline std::string serializeComment(const std::string& comment, int indent, const std::string& unit) {
	if (comment.empty()) return "";
	std::string out, line;
	for (char c : comment) {
		if (c == '\n') { out += pad(indent, unit) + "# " + line + "\n"; line.clear(); }
		else           { line += c; }
	}
	if (!line.empty()) out += pad(indent, unit) + "# " + line + "\n";
	return out;
}

// Declared here, defined after CodaValue is complete.
inline std::string serializeMap(
	const OrderedMap<std::string, Value>& m,
	int indent,
	const std::string& unit);
} // namespace detail

class Block {
	detail::OrderedMap<std::string, detail::Value> content;

public:
	Block() {}

	Block insert(const std::string& key, detail::Value value) {
		content[key] = value;
		return *this;
	}

	const detail::Value& operator[](const std::string& key) const { return content.at(key); }
	detail::Value&       operator[](const std::string& key) { return content[key]; }

	auto begin() const { return content.begin(); }
	auto begin()       { return content.begin(); }
	auto end()   const { return content.end(); }
	auto end()         { return content.end(); }

	// Defined after CodaValue is complete (uses serializeMap).
	std::string serialize(int indent, const std::string& unit) const;
};

class Row {
	detail::OrderedMap<std::string, std::string> content;
	std::string comment;

public:
	Row() {}

	Row insert(const std::string& key, std::string value) {
		content[key] = value;
		return *this;
	}

	const std::string& operator[](const std::string& key) const { return content.at(key); }
	std::string&       operator[](const std::string& key) { return content[key]; }

	auto begin() const { return content.begin(); }
	auto begin()       { return content.begin(); }
	auto end()   const { return content.end(); }
	auto end()         { return content.end(); }

};

class KeyedTable {
	detail::OrderedMap<std::string, Row> content;
	std::set<std::string> headers;
	std::string headerComment;

public:
	KeyedTable(std::set<std::string> headers) : headers(headers) {}

	KeyedTable insert(const std::string& key, Row value) {
		content[key] = value;
		return *this;
	}

	const Row& operator[](const std::string& key) const { return content.at(key); }
	Row&       operator[](const std::string& key) { return content[key]; }

	auto begin() const { return content.begin(); }
	auto begin()       { return content.begin(); }
	auto end()   const { return content.end(); }
	auto end()         { return content.end(); }

	// Defined after CodaValue is complete (uses asString/asTable).
	std::string serializeRow(const std::vector<std::string>& fields) const;
	std::string serialize(int indent, const std::string& unit) const;
};

class Table {
	std::vector<Row> content;
	std::set<std::string> headers;
	std::string headerComment;

public:
	Table(std::set<std::string> headers) : headers(headers) {}

	Table append(Row value) {
		content.push_back(value);
		return *this;
	}

	const Row& operator[](size_t i) const { return content.at(i); }
	Row&       operator[](size_t i) { return content.at(i); }

	auto begin() const { return content.begin(); }
	auto begin()       { return content.begin(); }
	auto end()   const { return content.end(); }
	auto end()         { return content.end(); }

	// Defined after CodaValue is complete (uses asTable/serializeRow).
	std::string serialize(int indent, const std::string& unit) const;
};

class Array {
	std::vector<detail::Value> content;
	std::string headerComment;

public:
	Array append(detail::Value value) {
		content.push_back(value);
		return *this;
	}

	const detail::Value& operator[](size_t i) const { return content.at(i); }
	detail::Value&       operator[](size_t i) { return content.at(i); }

	auto begin() const { return content.begin(); }
	auto begin()       { return content.begin(); }
	auto end()   const { return content.end(); }
	auto end()         { return content.end(); }

	// Defined after CodaValue is complete (uses asTable/serializeRow).
	std::string serialize(int indent, const std::string& unit) const;
};

class File {
	Block root;

public:
	const detail::Value& operator[](const std::string& key) const { return root[key]; }
	detail::Value&       operator[](const std::string& key) { return root[key]; }

	void order();
	void order(const std::function<float(const std::string&)>& weightFn);

	std::string serialize(const std::string& unit = "\t") const {
		return root.serialize(0, unit);
	}
};

namespace detail {

class Value {
	Variant<std::string, Block, Array, Table, KeyedTable> content;
	std::string comment;

public:
	Value() : content(std::string("")) {}
	Value(std::string  str)   : content(std::move(str))   {}
	Value(Block    block) : content(std::move(block)) {}
	Value(Array    arr)   : content(std::move(arr))   {}
	Value(KeyedTable    table) : content(std::move(table)) {}
	Value(const char*  str)   : content(std::string(str)) {}

	const std::string& asString() const {
		if (auto* p = std::get_if<std::string>(&content.value)) return *p;
		throw std::runtime_error("CodaValue::asString() — value is not a string");
	}
	std::string& asString() {
		if (auto* p = std::get_if<std::string>(&content.value)) return *p;
		throw std::runtime_error("CodaValue::asString() — value is not a string");
	}

	const Block& asBlock() const {
		if (auto* p = std::get_if<Block>(&content.value)) return *p;
		throw std::runtime_error("CodaValue::asBlock() — value is not a block");
	}
	Block& asBlock() {
		if (auto* p = std::get_if<Block>(&content.value)) return *p;
		throw std::runtime_error("CodaValue::asBlock() — value is not a block");
	}

	const Array& asArray() const {
		if (auto* p = std::get_if<Array>(&content.value)) return *p;
		throw std::runtime_error("CodaValue::asArray() — value is not an array");
	}
	Array& asArray() {
		if (auto* p = std::get_if<Array>(&content.value)) return *p;
		throw std::runtime_error("CodaValue::asArray() — value is not an array");
	}

	const Table& asTable() const {
		if (auto* p = std::get_if<Table>(&content.value)) return *p;
		throw std::runtime_error("CodaValue::asTable() — value is not a table");
	}
	Table& asTable() {
		if (auto* p = std::get_if<Table>(&content.value)) return *p;
		throw std::runtime_error("CodaValue::asTable() — value is not a table");
	}

	const KeyedTable& asKeyedTable() const {
		if (auto* p = std::get_if<KeyedTable>(&content.value)) return *p;
		throw std::runtime_error("CodaValue::asKeyedTable() — value is not a keyed table");
	}
	KeyedTable& asKeyedTable() {
		if (auto* p = std::get_if<KeyedTable>(&content.value)) return *p;
		throw std::runtime_error("CodaValue::asKeyedTable() — value is not a keyed table");
	}

	std::string serializeInline(int indent, const std::string& unit) const {
		return content.match(
			[](const std::string& s) { return detail::serializeToken(s); },
			[&](const Block&  b) { return b.serialize(indent, unit); },
			[&](const Array&  a) { return a.serialize(indent, unit); },
			[&](const Table&  t) { return t.serialize(indent, unit); },
			[&](const KeyedTable&  t) { return t.serialize(indent, unit); }
		);
	}

	void order();
	void order(const std::function<float(const std::string&)>& weightFn);
};

inline std::vector<std::string> fieldsOf(const KeyedTable& t) {
	std::vector<std::string> fields;
	if (t.content.empty()) return fields;
	for (const auto& [k, _] : t.content.begin()->second.asTable().content)
		fields.push_back(k);
	return fields;
}

inline std::string serializeMap(
		const OrderedMap<std::string, Value>& m,
		int indent,
		const std::string& unit) {
	std::string out;
	for (const auto& [k, v] : m) {
		if (v.isContainer() && !out.empty()) out += "\n";
		out += serializeComment(v.comment, indent, unit);
		out += pad(indent, unit) + serializeToken(k) + " "
			+ v.serializeInline(indent, unit) + "\n";
	}
	return out;
}

inline void orderMap(OrderedMap<std::string, Value>& m) {
	for (auto& [k, v] : m) v.order();
	m.sort([](const Value& v) { return v.isContainer(); });
}

inline void orderMapWeighted(
		OrderedMap<std::string, Value>& m,
		const std::function<float(const std::string&)>& weightFn) {
	for (auto& [k, v] : m) v.order(weightFn);
	m.sortByWeight(weightFn);
}

} // namespace detail

// ─── Block::serialize ─────────────────────────────────────────────────────────

inline std::string Block::serialize(int indent, const std::string& unit) const {
	return "{\n" + detail::serializeMap(content, indent + 1, unit) + detail::pad(indent, unit) + "}";
}

// ─── KeyedTable::serialize ────────────────────────────────────────────────────

inline std::string KeyedTable::serializeRow(const std::vector<std::string>& fields) const {
	// Note: With the new Row object, this method is largely redundant. 
	// It's left here returning an empty string to satisfy the linker if you 
	// keep it in the header, but serialization now happens directly below.
	return "";
}

inline std::string KeyedTable::serialize(int indent, const std::string& unit) const {
	if (content.empty()) return "[]";

	// Extract headers dynamically from the first row
	std::vector<std::string> fields;
	for (const auto& [k, _] : content.begin()->second) {
		fields.push_back(k);
	}

	std::string out = "[\n";
	out += detail::serializeComment(headerComment, indent + 1, unit);
	out += detail::pad(indent + 1, unit) + "key";

	for (const auto& f : fields) {
		out += " " + detail::serializeToken(f);
	}
	out += "\n";

	for (const auto& [rowKey, row] : content) {
		// If Row eventually gets a public 'comment' field, serialize it here.
		out += detail::pad(indent + 1, unit) + detail::serializeToken(rowKey);
		for (const auto& f : fields) {
			out += " " + detail::serializeToken(row[f]);
		}
		out += "\n";
	}

	return out + detail::pad(indent, unit) + "]";
}

// ─── Table::serialize ─────────────────────────────────────────────────────────

inline std::string Table::serialize(int indent, const std::string& unit) const {
	if (content.empty()) return "[]";

	std::vector<std::string> fields;
	for (const auto& [k, _] : content.front()) {
		fields.push_back(k);
	}

	std::string out = "[\n";
	out += detail::serializeComment(headerComment, indent + 1, unit);
	out += detail::pad(indent + 1, unit);

	for (size_t i = 0; i < fields.size(); ++i) {
		out += detail::serializeToken(fields[i]) + (i < fields.size() - 1 ? " " : "");
	}
	out += "\n";

	for (const auto& row : content) {
		out += detail::pad(indent + 1, unit);
		for (size_t i = 0; i < fields.size(); ++i) {
			out += detail::serializeToken(row[fields[i]]) + (i < fields.size() - 1 ? " " : "");
		}
		out += "\n";
	}

	return out + detail::pad(indent, unit) + "]";
}

// ─── Array::serialize ─────────────────────────────────────────────────────────

inline std::string Array::serialize(int indent, const std::string& unit) const {
	if (content.empty()) return "[]";

	std::string out = "[\n";
	out += detail::serializeComment(headerComment, indent + 1, unit);

	for (const auto& v : content) {
		out += detail::serializeComment(v.comment, indent + 1, unit);
		out += detail::pad(indent + 1, unit) + v.serializeInline(indent + 1, unit) + "\n";
	}

	return out + detail::pad(indent, unit) + "]";
}

// ─── Value / File Ordering ────────────────────────────────────────────────────

namespace detail {

inline void Value::order() {
	content.match(
		[](std::string&) {},
		[](Block& b)      { detail::orderMap(b.content); },
		[](Array& a)      { for (auto& v : a) v.order(); },
		[](Table& t)      { /* Std::vector handles native insertion order */ },
		[](KeyedTable& t) { detail::orderMap(t.content); }
	);
}

inline void Value::order(const std::function<float(const std::string&)>& weightFn) {
	content.match(
		[](std::string&) {},
		[&](Block& b)      { detail::orderMapWeighted(b.content, weightFn); },
		[&](Array& a)      { for (auto& v : a) v.order(weightFn); },
		[&](Table& t)      { /* Standard tables don't re-sort rows dynamically */ },
		[&](KeyedTable& t) { detail::orderMapWeighted(t.content, weightFn); }
	);
}

} // namespace detail

// File now operates strictly off its root Block
inline void File::order() {
	detail::orderMap(root.content);
}

inline void File::order(const std::function<float(const std::string&)>& weightFn) {
	detail::orderMapWeighted(root.content, weightFn);
}
