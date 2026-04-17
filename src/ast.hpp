#pragma once

#include "helpers/types.hpp"
#include "helpers/ordered_map.hpp"
#include <set>
#include <string>
#include <vector>
#include <functional>
#include <stdexcept>
#include <memory>

namespace coda {

// ─── detail helpers (no Value dependency) ────────────────────────────────────

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

} // namespace detail

// ─── Row ─────────────────────────────────────────────────────────────────────

class Row {
	detail::OrderedMap<std::string, std::string> content;
	std::string comment;

public:
	Row() {}

	void setComment(const std::string& c) { comment = c; }
	const std::string& getComment() const  { return comment; }

	Row& insert(const std::string& key, std::string value) {
		content[key] = std::move(value);
		return *this;
	}

	const std::string& operator[](const std::string& key) const { return content.at(key); }
	std::string&       operator[](const std::string& key)       { return content[key]; }

	auto begin() const { return content.begin(); }
	auto begin()       { return content.begin(); }
	auto end()   const { return content.end(); }
	auto end()         { return content.end(); }
};

// ─── Table ───────────────────────────────────────────────────────────────────

class Table {
	std::vector<Row> content;
	std::string headerComment;
	std::set<std::string> headers;

public:
	explicit Table(std::set<std::string> headers) : headers(std::move(headers)) {}

	void setHeaderComment(const std::string& c) { headerComment = c; }
	const std::string& getHeaderComment() const  { return headerComment; }

	Table& append(Row row) {
		for (auto& [field, val] : row) {
			if (headers.find(field) == headers.end())
				throw std::invalid_argument("Table::append — unknown field '" + field + "'");
		}
		for (const auto& h : headers) {
			bool found = false;
			for (auto& [field, val] : row)
				if (field == h) { found = true; break; }
			if (!found)
				throw std::invalid_argument("Table::append — missing required field '" + h + "'");
		}
		content.push_back(std::move(row));
		return *this;
	}

	const Row& operator[](size_t i) const { return content.at(i); }
	Row&       operator[](size_t i)       { return content.at(i); }

	bool empty() const { return content.empty(); }
	size_t size()  const { return content.size(); }
	const Row& front() const { return content.front(); }
	const std::set<std::string>& getHeaders() const { return headers; }

	auto begin() const { return content.begin(); }
	auto begin()       { return content.begin(); }
	auto end()   const { return content.end(); }
	auto end()         { return content.end(); }

	std::string serialize(int indent, const std::string& unit) const;
};

// ─── KeyedTable ──────────────────────────────────────────────────────────────

class KeyedTable {
	detail::OrderedMap<std::string, Row> content;
	std::string headerComment;
	std::set<std::string> headers;

public:
	explicit KeyedTable(std::set<std::string> headers) : headers(std::move(headers)) {}

	void setHeaderComment(const std::string& c) { headerComment = c; }
	const std::string& getHeaderComment() const  { return headerComment; }

	KeyedTable& insert(const std::string& key, Row row) {
		for (auto& [field, val] : row) {
			if (headers.find(field) == headers.end())
				throw std::invalid_argument("KeyedTable::insert — unknown field '" + field + "'");
		}
		for (const auto& h : headers) {
			bool found = false;
			for (auto& [field, val] : row)
				if (field == h) { found = true; break; }
			if (!found)
				throw std::invalid_argument("KeyedTable::insert — missing required field '" + h + "'");
		}
		content[key] = std::move(row);
		return *this;
	}

	const Row& operator[](const std::string& key) const { return content.at(key); }
	Row&       operator[](const std::string& key)       { return content[key]; }

	bool empty() const { return content.empty(); }
	const std::set<std::string>& getHeaders() const { return headers; }

	auto begin() const { return content.begin(); }
	auto begin()       { return content.begin(); }
	auto end()   const { return content.end(); }
	auto end()         { return content.end(); }

	const detail::OrderedMap<std::string, Row>& getContent() const { return content; }
	detail::OrderedMap<std::string, Row>&       getContent()       { return content; }

	std::string serialize(int indent, const std::string& unit) const;
};

// ─── Value (forward) ─────────────────────────────────────────────────────────

namespace detail { class Value; }

// ─── Block ───────────────────────────────────────────────────────────────────

class Block {
	detail::OrderedMap<std::string, std::unique_ptr<detail::Value>> content;

public:
	Block();
	Block(const Block& o);
	Block(Block&&) = default;
	Block& operator=(const Block& o);
	Block& operator=(Block&&) = default;

	Block& insert(const std::string& key, detail::Value value);

	const detail::Value& operator[](const std::string& key) const;
	detail::Value&       operator[](const std::string& key);

	bool has(const std::string& key) const { return content.contains(key); }

	auto begin() const { return content.begin(); }
	auto begin()       { return content.begin(); }
	auto end()   const { return content.end(); }
	auto end()         { return content.end(); }

	const detail::OrderedMap<std::string, std::unique_ptr<detail::Value>>& getContent() const { return content; }
	detail::OrderedMap<std::string, std::unique_ptr<detail::Value>>&       getContent()       { return content; }

	void order();
	void order(const std::function<float(const std::string&)>& weightFn);

	std::string serialize(int indent, const std::string& unit) const;
	std::string serialize(const std::string& unit = "\t") const;
};

// ─── Array ───────────────────────────────────────────────────────────────────

class Array {
	std::vector<std::unique_ptr<detail::Value>> content;
	std::string headerComment;

public:
	Array();
	Array(const Array& o);
	Array(Array&&) = default;
	Array& operator=(const Array& o);
	Array& operator=(Array&&) = default;

	void setHeaderComment(const std::string& c) { headerComment = c; }
	const std::string& getHeaderComment() const  { return headerComment; }

	Array& append(detail::Value value);

	const detail::Value& operator[](size_t i) const;
	detail::Value&       operator[](size_t i);

	size_t size() const { return content.size(); }

	auto begin() const { return content.begin(); }
	auto begin()       { return content.begin(); }
	auto end()   const { return content.end(); }
	auto end()         { return content.end(); }

	std::string serialize(int indent, const std::string& unit) const;
};

// ─── Value ───────────────────────────────────────────────────────────────────

namespace detail {

class Value {
	Variant<std::string, Block, Array, Table, KeyedTable> content;
	std::string comment;

public:
	void setComment(const std::string& c) { comment = c; }
	const std::string& getComment() const  { return comment; }

	Value() : content(std::string("")) {}
	Value(std::string   str)   : content(std::move(str))   {}
	Value(Block         block) : content(std::move(block)) {}
	Value(Array         arr)   : content(std::move(arr))   {}
	Value(Table         table) : content(std::move(table)) {}
	Value(KeyedTable    table) : content(std::move(table)) {}
	Value(const char*   str)   : content(std::string(str)) {}

	bool isContainer() const {
		return !std::holds_alternative<std::string>(content.value);
	}

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
			[&](const Block&      b) { return b.serialize(indent, unit); },
			[&](const Array&      a) { return a.serialize(indent, unit); },
			[&](const Table&      t) { return t.serialize(indent, unit); },
			[&](const KeyedTable& t) { return t.serialize(indent, unit); }
		);
	}

	void order();
	void order(const std::function<float(const std::string&)>& weightFn);
};

} // namespace detail

// ─── Block/Array out-of-line (Value now complete) ─────────────────────────────

inline Block::Block(const Block& o) {
	for (const auto& [k, v] : o.content)
		content[k] = std::make_unique<detail::Value>(*v);
}
inline Block& Block::operator=(const Block& o) {
	content.clear();
	for (const auto& [k, v] : o.content)
		content[k] = std::make_unique<detail::Value>(*v);
	return *this;
}
inline Block& Block::insert(const std::string& key, detail::Value value) {
	content[key] = std::make_unique<detail::Value>(std::move(value));
	return *this;
}
inline const detail::Value& Block::operator[](const std::string& key) const { return *content.at(key); }
inline detail::Value&       Block::operator[](const std::string& key) {
	if (!content.count(key)) content[key] = std::make_unique<detail::Value>();
	return *content[key];
}

inline Array::Array(const Array& o) {
	for (const auto& v : o.content)
		content.push_back(std::make_unique<detail::Value>(*v));
}
inline Array& Array::operator=(const Array& o) {
	content.clear();
	for (const auto& v : o.content)
		content.push_back(std::make_unique<detail::Value>(*v));
	return *this;
}
inline Array& Array::append(detail::Value value) {
	content.push_back(std::make_unique<detail::Value>(std::move(value)));
	return *this;
}
inline const detail::Value& Array::operator[](size_t i) const { return *content.at(i); }
inline detail::Value&       Array::operator[](size_t i)       { return *content.at(i); }

// ─── serializeMap ─────────────────────────────────────────────────────────────

namespace detail {

inline std::string serializeMap(
		const OrderedMap<std::string, std::unique_ptr<Value>>& m,
		int indent,
		const std::string& unit) {
	std::string out;
	for (const auto& [k, vp] : m) {
		const Value& v = *vp;
		if (v.isContainer() && !out.empty()) out += "\n";
		out += serializeComment(v.getComment(), indent, unit);
		out += pad(indent, unit) + serializeToken(k) + " "
			+ v.serializeInline(indent, unit) + "\n";
	}
	return out;
}

// ─── orderMap ────────────────────────────────────────────────────────────────

inline void orderMap(OrderedMap<std::string, std::unique_ptr<Value>>& m) {
	for (auto& [k, v] : m) v->order();
	m.sort([](const std::unique_ptr<Value>& v) { return v->isContainer(); });
}

inline void orderMap(OrderedMap<std::string, Row>& /*m*/) {
	// Rows have no sub-ordering
}

inline void orderMapWeighted(
		OrderedMap<std::string, std::unique_ptr<Value>>& m,
		const std::function<float(const std::string&)>& weightFn) {
	for (auto& [k, v] : m) v->order(weightFn);
	m.sortByWeight(weightFn);
}

inline void orderMapWeighted(
		OrderedMap<std::string, Row>& /*m*/,
		const std::function<float(const std::string&)>& /*weightFn*/) {
	// Rows have no sub-ordering
}

} // namespace detail (serializeMap / orderMap)

// ─── serialize impls ──────────────────────────────────────────────────────────

inline std::string Block::serialize(const std::string& unit) const {
	return detail::serializeMap(getContent(), 0, unit);
}

inline std::string Block::serialize(int indent, const std::string& unit) const {
	return "{\n" + detail::serializeMap(getContent(), indent + 1, unit) + detail::pad(indent, unit) + "}";
}

inline std::string KeyedTable::serialize(int indent, const std::string& unit) const {
	std::vector<const std::string*> fields;
	if (content.empty()) {
		for (const auto& h : headers)
			fields.push_back(&h);
	} else {
		for (const auto& [k, _] : content.begin()->second)
			fields.push_back(&k);
	}

	std::string out = "[\n";
	out += detail::serializeComment(headerComment, indent + 1, unit);
	out += detail::pad(indent + 1, unit) + "key";
	for (const auto& f : fields)
		out += " " + detail::serializeToken(*f);
	out += "\n";

	for (const auto& [rowKey, row] : content) {
		out += detail::serializeComment(row.getComment(), indent + 1, unit);
		out += detail::pad(indent + 1, unit) + detail::serializeToken(rowKey);
		for (const auto& f : fields)
			out += " " + detail::serializeToken(row[*f]);
		out += "\n";
	}

	return out + detail::pad(indent, unit) + "]";
}

inline std::string Table::serialize(int indent, const std::string& unit) const {
	std::vector<const std::string*> fields;
	if (content.empty()) {
		for (const auto& h : headers)
			fields.push_back(&h);
	} else {
		for (const auto& [k, _] : content.front())
			fields.push_back(&k);
	}

	std::string out = "[\n";
	out += detail::serializeComment(headerComment, indent + 1, unit);
	out += detail::pad(indent + 1, unit);
	for (size_t i = 0; i < fields.size(); ++i)
		out += detail::serializeToken(*fields[i]) + (i < fields.size() - 1 ? " " : "");
	out += "\n";

	for (const auto& row : content) {
		out += detail::serializeComment(row.getComment(), indent + 1, unit);
		out += detail::pad(indent + 1, unit);
		for (size_t i = 0; i < fields.size(); ++i)
			out += detail::serializeToken(row[*fields[i]]) + (i < fields.size() - 1 ? " " : "");
		out += "\n";
	}

	return out + detail::pad(indent, unit) + "]";
}

inline std::string Array::serialize(int indent, const std::string& unit) const {
	if (content.empty())
		return "[\n" + detail::pad(indent, unit) + "]";

	std::string out = "[\n";
	out += detail::serializeComment(headerComment, indent + 1, unit);
	for (const auto& vp : content) {
		out += detail::serializeComment(vp->getComment(), indent + 1, unit);
		out += detail::pad(indent + 1, unit) + vp->serializeInline(indent + 1, unit) + "\n";
	}

	return out + detail::pad(indent, unit) + "]";
}

// ─── Value::order / Block::order ──────────────────────────────────────────────

namespace detail {

inline void Value::order() {
	content.match(
		[](std::string&)  {},
		[](Block& b)      { detail::orderMap(b.getContent()); },
		[](Array& a)      { for (auto& v : a) v->order(); },
		[](Table&)        {},
		[](KeyedTable& t) { detail::orderMap(t.getContent()); }
	);
}

inline void Value::order(const std::function<float(const std::string&)>& weightFn) {
	content.match(
		[](std::string&)   {},
		[&](Block& b)      { detail::orderMapWeighted(b.getContent(), weightFn); },
		[&](Array& a)      { for (auto& v : a) v->order(weightFn); },
		[](Table&)         {},
		[&](KeyedTable& t) { detail::orderMapWeighted(t.getContent(), weightFn); }
	);
}

} // namespace detail

inline void Block::order() {
	detail::orderMap(getContent());
}

inline void Block::order(const std::function<float(const std::string&)>& weightFn) {
	detail::orderMapWeighted(getContent(), weightFn);
}

inline Block::Block() {}
inline Array::Array() {}

} // namespace coda
