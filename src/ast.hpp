#pragma once
#include "types.hpp"
#include "ordered_map.hpp"
#include <string>
#include <vector>
#include <functional>
#include <stdexcept>

struct CodaValue;
struct CodaBlock;
struct CodaArray;
struct CodaTable;

// ─── helpers ────────────────────────────────────────────────────────────────

namespace coda_detail {

inline std::string pad(int level, const std::string& unit) {
	std::string out;
	for (int i = 0; i < level; ++i) out += unit;
	return out;
}

inline std::string serializeToken(const std::string& s) {
	if (s.empty() || s.find(' ') != std::string::npos || s.find('\n') != std::string::npos)
		return '"' + s + '"';
	return s;
}

inline std::string serializeComment(const std::string& comment, int indent, const std::string& unit) {
	if (comment.empty()) return "";
	std::string out;
	std::string line;
	for (char c : comment) {
		if (c == '\n') {
			out += pad(indent, unit) + "# " + line + "\n";
			line.clear();
		} else {
			line += c;
		}
	}
	if (!line.empty())
		out += pad(indent, unit) + "# " + line + "\n";
	return out;
}

} // namespace coda_detail

// ─── node types ─────────────────────────────────────────────────────────────

struct CodaTable {
	OrderedMap<std::string, CodaValue> content;

	const CodaValue& operator[](const std::string& key) const { return content.at(key); }
	CodaValue&       operator[](const std::string& key)       { return content.at(key); }

	auto begin() const { return content.begin(); }
	auto begin()       { return content.begin(); }
	auto end()   const { return content.end(); }
	auto end()         { return content.end(); }

	std::string serializeRow(const std::vector<std::string>& fields, const std::string& unit) const;
	std::string serialize(int indent, const std::string& unit) const;
};

struct CodaBlock {
	OrderedMap<std::string, CodaValue> content;

	const CodaValue& operator[](const std::string& key) const { return content.at(key); }
	CodaValue&       operator[](const std::string& key)       { return content.at(key); }

	auto begin() const { return content.begin(); }
	auto begin()       { return content.begin(); }
	auto end()   const { return content.end(); }
	auto end()         { return content.end(); }

	std::string serialize(int indent, const std::string& unit) const;
};

struct CodaArray {
	std::vector<CodaValue> content;

	const CodaValue& operator[](size_t i) const { return content.at(i); }
	CodaValue&       operator[](size_t i)       { return content.at(i); }

	auto begin() const { return content.begin(); }
	auto begin()       { return content.begin(); }
	auto end()   const { return content.end(); }
	auto end()         { return content.end(); }

	std::string serialize(int indent, const std::string& unit) const;
};

// ─── CodaValue ──────────────────────────────────────────────────────────────

struct CodaValue {
	Variant<
		std::string,
		CodaBlock,
		CodaArray,
		CodaTable
	> content;
	std::string comment;

	CodaValue() : content(std::string("")) {}
	CodaValue(std::string str)  : content(std::move(str))  {}
	CodaValue(CodaBlock  block) : content(std::move(block)) {}
	CodaValue(CodaArray  arr)   : content(std::move(arr))   {}
	CodaValue(CodaTable  table) : content(std::move(table)) {}
	CodaValue(const char* str)  : content(std::string(str)) {}

	CodaValue& operator=(std::string  str)   { content = std::move(str);   return *this; }
	CodaValue& operator=(CodaBlock    block) { content = std::move(block); return *this; }
	CodaValue& operator=(CodaArray    arr)   { content = std::move(arr);   return *this; }
	CodaValue& operator=(CodaTable    table) { content = std::move(table); return *this; }
	CodaValue& operator=(const char*  str)   { content = std::string(str); return *this; }

	const CodaValue& operator[](const std::string& key) const {
		return content.match(
			[&](const CodaBlock& b) -> const CodaValue& { return b[key]; },
			[&](const CodaTable& t) -> const CodaValue& { return t[key]; },
			[](const auto&)         -> const CodaValue& {
				throw std::runtime_error("type does not support string indexing");
			}
		);
	}
	CodaValue& operator[](const std::string& key) {
		return content.match(
			[&](CodaBlock& b) -> CodaValue& { return b[key]; },
			[&](CodaTable& t) -> CodaValue& { return t[key]; },
			[](auto&)         -> CodaValue& {
				throw std::runtime_error("type does not support string indexing");
			}
		);
	}

	const CodaValue& operator[](size_t i) const {
		return content.match(
			[&](const CodaArray& a) -> const CodaValue& { return a[i]; },
			[](const auto&)         -> const CodaValue& {
				throw std::runtime_error("type does not support integer indexing");
			}
		);
	}
	CodaValue& operator[](size_t i) {
		return content.match(
			[&](CodaArray& a) -> CodaValue& { return a[i]; },
			[](auto&)         -> CodaValue& {
				throw std::runtime_error("type does not support integer indexing");
			}
		);
	}

	const std::string& asString() const { return std::get<std::string>(content.value); }
	std::string&       asString()       { return std::get<std::string>(content.value); }

	const CodaBlock& asBlock() const { return std::get<CodaBlock>(content.value); }
	CodaBlock&       asBlock()       { return std::get<CodaBlock>(content.value); }

	const CodaArray& asArray() const { return std::get<CodaArray>(content.value); }
	CodaArray&       asArray()       { return std::get<CodaArray>(content.value); }

	const CodaTable& asTable() const { return std::get<CodaTable>(content.value); }
	CodaTable&       asTable()       { return std::get<CodaTable>(content.value); }

	bool isContainer() const {
		return std::holds_alternative<CodaBlock>(content.value) ||
		       std::holds_alternative<CodaArray>(content.value) ||
		       std::holds_alternative<CodaTable>(content.value);
	}

	operator std::string() const {
		if (auto* s = std::get_if<std::string>(&content.value)) return *s;
		throw std::runtime_error("CodaValue is not a string");
	}
	operator std::string() {
		if (auto* s = std::get_if<std::string>(&content.value)) return *s;
		throw std::runtime_error("CodaValue is not a string");
	}

	operator const CodaBlock&() const {
		if (auto* b = std::get_if<CodaBlock>(&content.value)) return *b;
		throw std::runtime_error("CodaValue: Expected Block");
	}
	operator CodaBlock&() {
		if (auto* b = std::get_if<CodaBlock>(&content.value)) return *b;
		throw std::runtime_error("CodaValue: Expected Block");
	}

	operator const CodaArray&() const {
		if (auto* a = std::get_if<CodaArray>(&content.value)) return *a;
		throw std::runtime_error("CodaValue: Expected Array");
	}
	operator CodaArray&() {
		if (auto* a = std::get_if<CodaArray>(&content.value)) return *a;
		throw std::runtime_error("CodaValue: Expected Array");
	}

	operator const CodaTable&() const {
		if (auto* t = std::get_if<CodaTable>(&content.value)) return *t;
		throw std::runtime_error("CodaValue: Expected Table");
	}
	operator CodaTable&() {
		if (auto* t = std::get_if<CodaTable>(&content.value)) return *t;
		throw std::runtime_error("CodaValue: Expected Table");
	}

	std::string serializeInline(int indent, const std::string& unit) const {
		return content.match(
			[](const std::string& s) -> std::string {
				return coda_detail::serializeToken(s);
			},
			[&](const CodaBlock&  b) -> std::string { return b.serialize(indent, unit); },
			[&](const CodaArray&  a) -> std::string { return a.serialize(indent, unit); },
			[&](const CodaTable&  t) -> std::string { return t.serialize(indent, unit); }
		);
	}

	/// Recursively sort all fields: scalars first (alphabetical),
	/// then containers (alphabetical). Array element order is preserved.
	void order();

	/// Recursively sort all fields by a weight function.
	/// Higher weight → closer to the top. Equal weight → alphabetical.
	/// Array element order is preserved; their children are still sorted.
	void order(const std::function<float(const std::string&)>& weightFn);
};

// ─── CodaFile ────────────────────────────────────────────────────────────────

struct CodaFile {
	OrderedMap<std::string, CodaValue> statements;

	const CodaValue& operator[](const std::string& key) const { return statements.at(key); }
	CodaValue&       operator[](const std::string& key)       { return statements.at(key); }

	bool has(const std::string& key) const { return statements.count(key) > 0; }

	/// Recursively sort all fields: scalars first (alphabetical),
	/// then containers (alphabetical). Array element order is preserved.
	void order();

	/// Recursively sort all fields by a weight function.
	/// Higher weight → closer to the top. Equal weight → alphabetical.
	/// Array element order is preserved; their children are still sorted.
	void order(const std::function<float(const std::string&)>& weightFn);

	std::string serialize(const std::string& unit = "\t") const;
};

// ─── method bodies ──────────────────────────────────────────────────────────

inline std::string serializeMap(
		const OrderedMap<std::string, CodaValue>& m,
		int indent,
		const std::string& unit) {

	std::string out;
	for (const auto& [k, v] : m) {
		if (v.isContainer() && !out.empty())
			out += "\n";
		out += coda_detail::serializeComment(v.comment, indent, unit);
		out += coda_detail::pad(indent, unit) + k + " "
		     + v.serializeInline(indent, unit) + "\n";
	}
	return out;
}

// ── default order helpers ───────────────────────────────────────────────────

inline void orderMap(OrderedMap<std::string, CodaValue>& m) {
	for (auto& [k, v] : m)
		v.order();
	m.sort([](const CodaValue& v) { return v.isContainer(); });
}

inline void CodaValue::order() {
	content.match(
		[](std::string&) {},
		[](CodaBlock& b) { orderMap(b.content); },
		[](CodaArray& a) {
			for (auto& v : a.content)
				v.order();
		},
		[](CodaTable& t) { orderMap(t.content); }
	);
}

inline void CodaFile::order() {
	orderMap(statements);
}

// ── weighted order helpers ──────────────────────────────────────────────────

inline void orderMapWeighted(
		OrderedMap<std::string, CodaValue>& m,
		const std::function<float(const std::string&)>& weightFn) {
	for (auto& [k, v] : m)
		v.order(weightFn);
	m.sortByWeight(weightFn);
}

inline void CodaValue::order(const std::function<float(const std::string&)>& weightFn) {
	content.match(
		[](std::string&) {},
		[&](CodaBlock& b) { orderMapWeighted(b.content, weightFn); },
		[&](CodaArray& a) {
			for (auto& v : a.content)
				v.order(weightFn);
		},
		[&](CodaTable& t) { orderMapWeighted(t.content, weightFn); }
	);
}

inline void CodaFile::order(const std::function<float(const std::string&)>& weightFn) {
	orderMapWeighted(statements, weightFn);
}

// ── CodaBlock ────────────────────────────────────────────────────────────────

inline std::string CodaBlock::serialize(int indent, const std::string& unit) const {
	std::string out = "{\n";
	out += serializeMap(content, indent + 1, unit);
	out += coda_detail::pad(indent, unit) + "}";
	return out;
}

// ── CodaTable ────────────────────────────────────────────────────────────────

inline std::string CodaTable::serializeRow(
		const std::vector<std::string>& fields,
		const std::string& unit) const {
	std::string out;
	for (size_t i = 0; i < fields.size(); ++i) {
		out += coda_detail::serializeToken(content.at(fields[i]).asString());
		if (i < fields.size() - 1) out += " ";
	}
	return out;
}

inline bool isKeyedTable(const CodaTable& t) {
	if (t.content.empty()) return false;
	return std::holds_alternative<CodaTable>(t.content.begin()->second.content.value);
}

inline std::vector<std::string> fieldsOf(const CodaTable& t) {
	std::vector<std::string> fields;
	if (t.content.empty()) return fields;
	for (const auto& [k, _] : t.content.begin()->second.asTable().content)
		fields.push_back(k);
	return fields;
}

inline std::string CodaTable::serialize(int indent, const std::string& unit) const {
	if (content.empty()) return "[]";

	if (isKeyedTable(*this)) {
		auto fields = fieldsOf(*this);
		std::string out = "[\n";
		out += coda_detail::pad(indent + 1, unit) + "key";
		for (const auto& f : fields) out += " " + coda_detail::serializeToken(f);
		out += "\n";

		for (const auto& [rowKey, rowVal] : content) {
			out += coda_detail::serializeComment(rowVal.comment, indent + 1, unit);
			out += coda_detail::pad(indent + 1, unit) + coda_detail::serializeToken(rowKey);
			const CodaTable& row = rowVal.asTable();
			for (const auto& f : fields)
				out += " " + coda_detail::serializeToken(row.content.at(f).asString());
			out += "\n";
		}

		out += coda_detail::pad(indent, unit) + "]";
		return out;
	}

	std::string out;
	for (auto it = content.begin(); it != content.end(); ++it) {
		out += it->second.serializeInline(0, unit);
		if (std::next(it) != content.end()) out += " ";
	}
	return out;
}

// ── CodaArray ────────────────────────────────────────────────────────────────

inline std::string CodaArray::serialize(int indent, const std::string& unit) const {
	if (content.empty()) return "[]";

	std::string out = "[\n";

	if (std::holds_alternative<CodaTable>(content[0].content.value)) {
		const CodaTable& firstRow = content[0].asTable();
		std::vector<std::string> fields;
		for (const auto& [k, _] : firstRow.content) fields.push_back(k);

		out += coda_detail::pad(indent + 1, unit);
		for (size_t i = 0; i < fields.size(); ++i)
			out += fields[i] + (i < fields.size() - 1 ? " " : "");
		out += "\n";

		for (const auto& rowVal : content) {
			out += coda_detail::serializeComment(rowVal.comment, indent + 1, unit);
			out += coda_detail::pad(indent + 1, unit) + rowVal.asTable().serializeRow(fields, unit) + "\n";
		}
	} else {
		for (const auto& v : content) {
			out += coda_detail::serializeComment(v.comment, indent + 1, unit);
			out += coda_detail::pad(indent + 1, unit) + v.serializeInline(indent + 1, unit) + "\n";
		}
	}

	out += coda_detail::pad(indent, unit) + "]";
	return out;
}

// ── CodaFile ─────────────────────────────────────────────────────────────────

inline std::string CodaFile::serialize(const std::string& unit) const {
	return serializeMap(statements, 0, unit);
}
