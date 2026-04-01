#pragma once

#include <variant>

namespace coda {

// Helper for overloading lambdas
template<typename... Ts>
struct overloaded : Ts... {
	using Ts::operator()...;
};

template<typename... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

// Helper function to load a variant at a specific index
template<std::size_t I = 0, typename... Types, typename Archive>
void loadVariantAt(std::size_t idx, std::variant<Types...>& var, Archive& ar) {
	if constexpr (I<sizeof...(Types)) {
		if (I == idx) {
			std::variant_alternative_t<I, std::variant<Types...>> val;
			ar(val);
			var = std::move(val);
		} else {
			loadVariantAt<I + 1>(idx, var, ar);
		}
	}
}

template<typename... Types>
struct Variant {
	std::variant<Types...> value;

	Variant() = default;
	Variant(std::variant<Types...> value)
	: value(std::move(value)) {}

	template<typename T,
	typename = std::enable_if_t<(std::is_same_v<std::decay_t<T>, Types> || ...)>>
	Variant(T&& val) : value(std::forward<T>(val)) {}

	template<typename T,
	typename = std::enable_if_t<(std::is_same_v<std::decay_t<T>, Types> || ...)>>
	Variant& operator=(T&& val) {
		value = std::forward<T>(val);
		return *this;
	}

	template<typename Callback>
	decltype(auto) visit(Callback&& callback) {
		return std::visit(std::forward<Callback>(callback), value);
	}

	template<typename Callback>
	decltype(auto) visit(Callback&& callback) const {
		return std::visit(std::forward<Callback>(callback), value);
	}

	template<typename... Callbacks>
	decltype(auto) match(Callbacks&&... callbacks) {
		return std::visit(overloaded{std::forward<Callbacks>(callbacks)...}, value);
	}

	template<typename... Callbacks>
	decltype(auto) match(Callbacks&&... callbacks) const {
		return std::visit(overloaded{std::forward<Callbacks>(callbacks)...}, value);
	}

	template<typename Archive>
	void save(Archive& ar) const {
		ar(value.index());
		std::visit([&ar](const auto& v) { ar(v); }, value);
	}

	template<typename Archive>
	void load(Archive& ar) {
		std::size_t idx;
		ar(idx);
		loadVariantAt<0>(idx, value, ar);
	}
};

template<template<typename> class Wrapper, typename... Types>
struct WrappingVariant {
	std::variant<Wrapper<Types>...> value;

	WrappingVariant() = default;
	WrappingVariant(std::variant<Types...> value)
	: value(std::move(value)) {}

	template<typename T,
	typename = std::enable_if_t<(std::is_same_v<std::decay_t<T>, Types> || ...)>>
	WrappingVariant(T&& val) : value(std::forward<T>(val)) {}

	template<typename T,
	typename = std::enable_if_t<(std::is_same_v<std::decay_t<T>, Types> || ...)>>
	WrappingVariant& operator=(T&& val) {
		value = std::forward<T>(val);
		return *this;
	}

	template<typename Callback>
	decltype(auto) visit(Callback&& callback) {
		return std::visit(std::forward<Callback>(callback), value);
	}

	template<typename Callback>
	decltype(auto) visit(Callback&& callback) const {
		return std::visit(std::forward<Callback>(callback), value);
	}

	template<typename... Callbacks>
	decltype(auto) match(Callbacks&&... callbacks) {
		return std::visit(overloaded{std::forward<Callbacks>(callbacks)...}, value);
	}

	template<typename... Callbacks>
	decltype(auto) match(Callbacks&&... callbacks) const {
		return std::visit(overloaded{std::forward<Callbacks>(callbacks)...}, value);
	}

	template<typename Archive>
	void save(Archive& ar) const {
		ar(value.index());
		std::visit([&ar](const auto& v) { ar(v); }, value);
	}

	template<typename Archive>
	void load(Archive& ar) {
		std::size_t idx;
		ar(idx);
		loadVariantAt<0>(idx, value, ar);
	}
};

} // namespace coda

#include <vector>
#include <unordered_map>

#include <stdexcept>
#include <utility>
#include <algorithm>
#include <functional>

namespace coda {
namespace detail {

/// A map that preserves insertion order.
/// Iteration follows the order elements were inserted.
/// Key lookup is O(1) via an internal hash map.
template <typename K, typename V>
class OrderedMap {
	std::vector<std::pair<K, V>> entries;
	std::unordered_map<K, size_t> index; // key → position in entries

public:
	OrderedMap() = default;

	V& operator[](const K& key) {
		auto it = index.find(key);
		if (it != index.end())
			return entries[it->second].second;
		index[key] = entries.size();
		entries.emplace_back(key, V{});
		return entries.back().second;
	}

	const V& at(const K& key) const {
		auto it = index.find(key);
		if (it == index.end())
			throw std::out_of_range("OrderedMap::at — key not found: " + key);
		return entries[it->second].second;
	}

	V& at(const K& key) {
		auto it = index.find(key);
		if (it == index.end())
			throw std::out_of_range("OrderedMap::at — key not found: " + key);
		return entries[it->second].second;
	}

	bool contains(const K& key) const {
		return index.find(key) != index.end();
	}

	size_t count(const K& key) const {
		return index.count(key);
	}

	size_t size() const { return entries.size(); }
	bool empty() const { return entries.empty(); }

	void clear() {
		entries.clear();
		index.clear();
	}

	void erase(const K& key) {
		auto it = index.find(key);
		if (it == index.end()) return;
		size_t pos = it->second;
		index.erase(it);
		entries.erase(entries.begin() + pos);
		for (size_t i = pos; i < entries.size(); ++i)
			index[entries[i].first] = i;
	}

	using iterator       = typename std::vector<std::pair<K, V>>::iterator;
	using const_iterator = typename std::vector<std::pair<K, V>>::const_iterator;

	iterator       begin()       { return entries.begin(); }
	iterator       end()         { return entries.end(); }
	const_iterator begin() const { return entries.begin(); }
	const_iterator end()   const { return entries.end(); }

	/// Sort entries by key (alphabetical). Rebuilds the index.
	void sort() {
		std::stable_sort(entries.begin(), entries.end(),
			[](const auto& a, const auto& b) { return a.first < b.first; });
		rebuildIndex();
	}

	/// Sort with a predicate that classifies values as containers.
	/// Scalars come first (alphabetical), then containers (alphabetical).
	template <typename Pred>
	void sort(Pred isContainer) {
		std::stable_sort(entries.begin(), entries.end(),
			[&](const auto& a, const auto& b) {
				bool aCont = isContainer(a.second);
				bool bCont = isContainer(b.second);
				if (aCont != bCont) return !aCont;
				return a.first < b.first;
			});
		rebuildIndex();
	}

	/// Sort by a weight function on the key.
	/// Higher weight → closer to the top.
	/// Equal weight → alphabetical by key.
	void sortByWeight(const std::function<float(const K&)>& weightFn) {
		std::stable_sort(entries.begin(), entries.end(),
			[&](const auto& a, const auto& b) {
				float wa = weightFn(a.first);
				float wb = weightFn(b.first);
				if (wa != wb) return wa > wb;
				return a.first < b.first;
			});
		rebuildIndex();
	}

	/// Insert a key-value pair. Returns {iterator, true} if inserted,
	/// {iterator, false} if the key already existed (value unchanged).
	std::pair<iterator, bool> insert(const K& key, V value) {
		auto it = index.find(key);
		if (it != index.end())
			return { entries.begin() + it->second, false };
		index[key] = entries.size();
		entries.emplace_back(key, std::move(value));
		return { entries.end() - 1, true };
	}

private:
	void rebuildIndex() {
		index.clear();
		for (size_t i = 0; i < entries.size(); ++i)
			index[entries[i].first] = i;
	}
};

} // namespace detail
} // namespace coda

#include <string>
#include <vector>
#include <functional>
#include <stdexcept>

namespace coda {

struct CodaValue;
struct CodaBlock;
struct CodaArray;
struct CodaTable;

// ─── helpers ────────────────────────────────────────────────────────────────

namespace detail {

inline std::string pad(int level, const std::string& unit) {
	std::string out;
	for (int i = 0; i < level; ++i) out += unit;
	return out;
}

inline std::string serializeToken(const std::string& s) {
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
	if (!needsQuotes) {
		for (unsigned char c : s) {
			if (!isBareChar(c)) { needsQuotes = true; break; }
		}
	}

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
	out += '"';
	return out;
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

inline std::string serializeMap(
		const OrderedMap<std::string, CodaValue>& m,
		int indent,
		const std::string& unit);

inline bool isKeyedTable(const CodaTable& t);
inline std::vector<std::string> fieldsOf(const CodaTable& t);

inline void orderMap(OrderedMap<std::string, CodaValue>& m);
inline void orderMapWeighted(
		OrderedMap<std::string, CodaValue>& m,
		const std::function<float(const std::string&)>& weightFn);

} // namespace detail

// ─── node types ─────────────────────────────────────────────────────────────

struct CodaTable {
	detail::OrderedMap<std::string, CodaValue> content;

	const CodaValue& operator[](const std::string& key) const;
	CodaValue&       operator[](const std::string& key);

	auto begin() const;
	auto begin();
	auto end() const;
	auto end();

	std::string serializeRow(const std::vector<std::string>& fields) const;
	std::string serialize(int indent, const std::string& unit) const;
};

struct CodaBlock {
	detail::OrderedMap<std::string, CodaValue> content;

	const CodaValue& operator[](const std::string& key) const;
	CodaValue&       operator[](const std::string& key);

	auto begin() const;
	auto begin();
	auto end() const;
	auto end();

	std::string serialize(int indent, const std::string& unit) const;
};

struct CodaArray {
	std::vector<CodaValue> content;

	const CodaValue& operator[](size_t i) const;
	CodaValue&       operator[](size_t i);

	auto begin() const;
	auto begin();
	auto end() const;
	auto end();

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
	CodaValue& operator=(const char* str)   { content = std::string(str); return *this; }

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
				return detail::serializeToken(s);
			},
			[&](const CodaBlock&  b) -> std::string { return b.serialize(indent, unit); },
			[&](const CodaArray&  a) -> std::string { return a.serialize(indent, unit); },
			[&](const CodaTable&  t) -> std::string { return t.serialize(indent, unit); }
		);
	}

	void order();
	void order(const std::function<float(const std::string&)>& weightFn);
};

// ─── CodaFile ────────────────────────────────────────────────────────────────

struct CodaFile {
	detail::OrderedMap<std::string, CodaValue> statements;

	const CodaValue& operator[](const std::string& key) const { return statements.at(key); }
	CodaValue&       operator[](const std::string& key) { return statements[key]; }

	bool has(const std::string& key) const { return statements.count(key) > 0; }

	void order();
	void order(const std::function<float(const std::string&)>& weightFn);

	std::string serialize(const std::string& unit = "\t") const;
};

// ─── detail method bodies ────────────────────────────────────────────────────

namespace detail {

inline std::string serializeMap(
		const OrderedMap<std::string, CodaValue>& m,
		int indent,
		const std::string& unit) {

	std::string out;
	for (const auto& [k, v] : m) {
		if (v.isContainer() && !out.empty())
			out += "\n";
		out += serializeComment(v.comment, indent, unit);
		out += pad(indent, unit) + k + " "
			+ v.serializeInline(indent, unit) + "\n";
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

inline void orderMap(OrderedMap<std::string, CodaValue>& m) {
	for (auto& [k, v] : m)
		v.order();
	m.sort([](const CodaValue& v) { return v.isContainer(); });
}

inline void orderMapWeighted(
		OrderedMap<std::string, CodaValue>& m,
		const std::function<float(const std::string&)>& weightFn) {
	for (auto& [k, v] : m)
		v.order(weightFn);
	m.sortByWeight(weightFn);
}

} // namespace detail

// iterator helpers (defined here so CodaValue is complete for libc++)
inline auto CodaTable::begin() const { return content.begin(); }
inline auto CodaTable::begin()       { return content.begin(); }
inline auto CodaTable::end()   const { return content.end(); }
inline auto CodaTable::end()         { return content.end(); }

inline auto CodaBlock::begin() const { return content.begin(); }
inline auto CodaBlock::begin()       { return content.begin(); }
inline auto CodaBlock::end()   const { return content.end(); }
inline auto CodaBlock::end()         { return content.end(); }

inline auto CodaArray::begin() const { return content.begin(); }
inline auto CodaArray::begin()       { return content.begin(); }
inline auto CodaArray::end()   const { return content.end(); }
inline auto CodaArray::end()         { return content.end(); }

// ─── CodaValue method bodies ─────────────────────────────────────────────────

inline void CodaValue::order() {
	content.match(
		[](std::string&) {},
		[](CodaBlock& b) { detail::orderMap(b.content); },
		[](CodaArray& a) {
			for (auto& v : a.content)
				v.order();
		},
		[](CodaTable& t) { detail::orderMap(t.content); }
	);
}

inline void CodaValue::order(const std::function<float(const std::string&)>& weightFn) {
	content.match(
		[](std::string&) {},
		[&](CodaBlock& b) { detail::orderMapWeighted(b.content, weightFn); },
		[&](CodaArray& a) {
			for (auto& v : a.content)
				v.order(weightFn);
		},
		[&](CodaTable& t) { detail::orderMapWeighted(t.content, weightFn); }
	);
}

// ─── CodaBlock method bodies ──────────────────────────────────────────────────

inline const CodaValue& CodaBlock::operator[](const std::string& key) const { return content.at(key); }
inline CodaValue&       CodaBlock::operator[](const std::string& key)       { return content[key]; }

inline std::string CodaBlock::serialize(int indent, const std::string& unit) const {
	std::string out = "{\n";
	out += detail::serializeMap(content, indent + 1, unit);
	out += detail::pad(indent, unit) + "}";
	return out;
}

// ─── CodaTable method bodies ──────────────────────────────────────────────────

inline const CodaValue& CodaTable::operator[](const std::string& key) const { return content.at(key); }
inline CodaValue&       CodaTable::operator[](const std::string& key)       { return content[key]; }

inline std::string CodaTable::serializeRow(
		const std::vector<std::string>& fields) const {
	std::string out;
	for (size_t i = 0; i < fields.size(); ++i) {
		out += detail::serializeToken(content.at(fields[i]).asString());
		if (i < fields.size() - 1) out += " ";
	}
	return out;
}

inline std::string CodaTable::serialize(int indent, const std::string& unit) const {
	if (content.empty()) return "[]";

	if (detail::isKeyedTable(*this)) {
		auto fields = detail::fieldsOf(*this);
		std::string out = "[\n";
		out += detail::pad(indent + 1, unit) + "key";
		for (const auto& f : fields) out += " " + detail::serializeToken(f);
		out += "\n";

		for (const auto& [rowKey, rowVal] : content) {
			out += detail::serializeComment(rowVal.comment, indent + 1, unit);
			out += detail::pad(indent + 1, unit) + detail::serializeToken(rowKey);
			const CodaTable& row = rowVal.asTable();
			for (const auto& f : fields)
				out += " " + detail::serializeToken(row.content.at(f).asString());
			out += "\n";
		}

		out += detail::pad(indent, unit) + "]";
		return out;
	}

	std::string out;
	for (auto it = content.begin(); it != content.end(); ++it) {
		out += it->second.serializeInline(0, unit);
		if (std::next(it) != content.end()) out += " ";
	}
	return out;
}

// ─── CodaArray method bodies ──────────────────────────────────────────────────

inline const CodaValue& CodaArray::operator[](size_t i) const { return content.at(i); }
inline CodaValue& CodaArray::operator[](size_t i) { return content.at(i); }

inline std::string CodaArray::serialize(int indent, const std::string& unit) const {
	if (content.empty()) return "[]";

	std::string out = "[\n";

	if (std::holds_alternative<CodaTable>(content[0].content.value)) {
		const CodaTable& firstRow = content[0].asTable();
		std::vector<std::string> fields;
		for (const auto& [k, _] : firstRow.content) fields.push_back(k);

		out += detail::pad(indent + 1, unit);
		for (size_t i = 0; i < fields.size(); ++i)
			out += fields[i] + (i < fields.size() - 1 ? " " : "");
		out += "\n";

		for (const auto& rowVal : content) {
			out += detail::serializeComment(rowVal.comment, indent + 1, unit);
			out += detail::pad(indent + 1, unit) + rowVal.asTable().serializeRow(fields) + "\n";
		}
	} else {
		for (const auto& v : content) {
			out += detail::serializeComment(v.comment, indent + 1, unit);
			out += detail::pad(indent + 1, unit) + v.serializeInline(indent + 1, unit) + "\n";
		}
	}

	out += detail::pad(indent, unit) + "]";
	return out;
}

// ─── CodaFile method bodies ───────────────────────────────────────────────────

inline void CodaFile::order() {
	detail::orderMap(statements);
}

inline void CodaFile::order(const std::function<float(const std::string&)>& weightFn) {
	detail::orderMapWeighted(statements, weightFn);
}

inline std::string CodaFile::serialize(const std::string& unit) const {
	return detail::serializeMap(statements, 0, unit);
}

} // namespace coda

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

#include <fstream>
#include <sstream>
#include <string>
#include <functional>

class Coda {
	std::string indentUnit = "\t";
	coda::CodaFile file;

public:
	Coda() = default;
	Coda(const std::string& path) {
		std::ifstream f(path);
		if (!f) throw std::runtime_error("could not open: " + path);
		std::ostringstream ss;
		ss << f.rdbuf();
		file = coda::detail::Parser(ss.str(), path).parse();
	}

	static Coda parse(std::string content, std::string filename = "") {
		Coda coda;
		coda.file = coda::detail::Parser(content, filename).parse();
		return coda;
	}

	void useTabs()              { indentUnit = "\t"; }
	void useSpaces(int count)   { indentUnit = std::string(count, ' '); }

	/// Recursively sort all fields: scalars first (alphabetical),
	/// then containers (alphabetical). Array element order is preserved.
	void order() { file.order(); }

	/// Recursively sort all fields by a weight function.
	/// Higher weight → closer to the top. Equal weight → alphabetical.
	/// Array element order is preserved; their children are still sorted.
	///
	/// Example:
	///   coda.order([](const std::string& field) -> float {
	///       if (field == "name") return 100;
	///       if (field == "type") return 50;
	///       return 0; // everything else alphabetical at the bottom
	///   });
	void order(const std::function<float(const std::string&)>& weightFn) {
		file.order(weightFn);
	}

	void save(const std::string& path) const {
		std::ofstream f(path);
		if (!f) throw std::runtime_error("could not open: " + path);
		f << file.serialize(indentUnit);
	}

	void save(const std::string& path, const std::string& unit) {
		indentUnit = unit;
		save(path);
	}
	
	std::string serialize() const {
		return file.serialize();
	}

	const coda::CodaValue& operator[](const std::string& key) const { return file[key]; }
	coda::CodaValue&       operator[](const std::string& key) { return file[key]; }
};
