#pragma once
#include "types.hpp"
#include <map>
#include <string>
#include <vector>
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

// Emit a (possibly multi-line) comment block, each line prefixed with "# ".
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
	std::map<std::string, CodaValue> content;

	const CodaValue& operator[](const std::string& key) const { return content.at(key); }
	CodaValue&       operator[](const std::string& key)       { return content.at(key); }

	auto begin() const { return content.begin(); }
	auto begin()       { return content.begin(); }
	auto end()   const { return content.end(); }
	auto end()         { return content.end(); }

	// Serializes a single plain-table row (values only, space-separated).
	std::string serializeRow(const std::vector<std::string>& fields, const std::string& unit) const;

	// Full bracket serialization (used by CodaValue::serialize).
	// Called with the knowledge of whether this is a keyed table or a plain-table row —
	// determined by the caller (CodaArray / CodaValue).
	// This overload is for keyed tables standing alone as a value.
	std::string serialize(int indent, const std::string& unit) const;
};

struct CodaBlock {
	std::map<std::string, CodaValue> content;

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

	// Serialize just the value part (no key, no leading comment).
	// Used when embedding a value inside a parent container's serialization.
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
};

// ─── CodaFile ────────────────────────────────────────────────────────────────

struct CodaFile {
	std::map<std::string, CodaValue> statements;

	const CodaValue& operator[](const std::string& key) const { return statements.at(key); }
	CodaValue&       operator[](const std::string& key)       { return statements.at(key); }

	bool has(const std::string& key) const { return statements.count(key) > 0; }

	std::string serialize(const std::string& unit = "\t") const;
};

// ─── method bodies (need full CodaValue definition) ─────────────────────────

// Helper: serialize a map of key→CodaValue pairs, scalars first then containers.
// Emits comments stored on each CodaValue before its key line.
inline std::string serializeSortedMap(
		const std::map<std::string, CodaValue>& m,
		int indent,
		const std::string& unit) {

	std::vector<const std::string*> scalars, containers;
	for (const auto& [k, v] : m) {
		if (v.isContainer()) containers.push_back(&k);
		else                 scalars.push_back(&k);
	}

	std::string out;

	for (const auto* kp : scalars) {
		const CodaValue& v = m.at(*kp);
		out += coda_detail::serializeComment(v.comment, indent, unit);
		out += coda_detail::pad(indent, unit) + *kp + " " + v.serializeInline(indent, unit) + "\n";
	}

	for (const auto* kp : containers) {
		const CodaValue& v = m.at(*kp);
		out += "\n";
		out += coda_detail::serializeComment(v.comment, indent, unit);
		out += coda_detail::pad(indent, unit) + *kp + " " + v.serializeInline(indent, unit) + "\n";
	}

	return out;
}

// ── CodaBlock ────────────────────────────────────────────────────────────────

inline std::string CodaBlock::serialize(int indent, const std::string& unit) const {
	std::string out = "{\n";
	out += serializeSortedMap(content, indent + 1, unit);
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
		// Keyed table — emit "key field1 field2 ..." header then one row per entry.
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

	// Plain-table row (inline, space-separated scalar values).
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
		// Plain table — first element provides the header.
		const CodaTable& firstRow = content[0].asTable();
		std::vector<std::string> fields;
		for (const auto& [k, _] : firstRow.content) fields.push_back(k);

		// Header line.
		out += coda_detail::pad(indent + 1, unit);
		for (size_t i = 0; i < fields.size(); ++i)
			out += fields[i] + (i < fields.size() - 1 ? " " : "");
		out += "\n";

		// Data rows.
		for (const auto& rowVal : content) {
			out += coda_detail::serializeComment(rowVal.comment, indent + 1, unit);
			out += coda_detail::pad(indent + 1, unit) + rowVal.asTable().serializeRow(fields, unit) + "\n";
		}
	} else {
		// Bare list — one element per line, nesting supported.
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
	return serializeSortedMap(statements, 0, unit);
}
