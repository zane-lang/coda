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
	std::vector<std::string> fields;
	if (content.empty()) {
		for (const auto& h : headers)
			fields.push_back(h);
	} else {
		for (const auto& [k, _] : content.begin()->second)
			fields.push_back(k);
	}

	std::string out = "[\n";
	out += detail::serializeComment(headerComment, indent + 1, unit);
	out += detail::pad(indent + 1, unit) + "key";
	for (const auto& f : fields)
		out += " " + detail::serializeToken(f);
	out += "\n";

	for (const auto& [rowKey, row] : content) {
		out += detail::serializeComment(row.getComment(), indent + 1, unit);
		out += detail::pad(indent + 1, unit) + detail::serializeToken(rowKey);
		for (const auto& f : fields)
			out += " " + detail::serializeToken(row[f]);
		out += "\n";
	}

	return out + detail::pad(indent, unit) + "]";
}

inline std::string Table::serialize(int indent, const std::string& unit) const {
	std::vector<std::string> fields;
	if (content.empty()) {
		for (const auto& h : headers)
			fields.push_back(h);
	} else {
		for (const auto& [k, _] : content.front())
			fields.push_back(k);
	}

	std::string out = "[\n";
	out += detail::serializeComment(headerComment, indent + 1, unit);
	out += detail::pad(indent + 1, unit);
	for (size_t i = 0; i < fields.size(); ++i)
		out += detail::serializeToken(fields[i]) + (i < fields.size() - 1 ? " " : "");
	out += "\n";

	for (const auto& row : content) {
		out += detail::serializeComment(row.getComment(), indent + 1, unit);
		out += detail::pad(indent + 1, unit);
		for (size_t i = 0; i < fields.size(); ++i)
			out += detail::serializeToken(row[fields[i]]) + (i < fields.size() - 1 ? " " : "");
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
		if (c == '\n') {
			++line_; lineStart = pos;
		} else if (c == '\r') {
			if (pos < src.size() && src[pos] == '\n') ++pos;
			++line_; lineStart = pos;
		}
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

	// ── token helpers ───────────────────────────────────────────────────

	void checkNotError() {
		if (current.type != TokenType::Error) return;
		coda::ParseErrorCode code;
		if (current.value.find("unterminated") != std::string::npos)
			code = coda::ParseErrorCode::UnterminatedString;
		else if (current.value.find("escape") != std::string::npos)
			code = coda::ParseErrorCode::InvalidEscape;
		else
			code = coda::ParseErrorCode::UnexpectedToken;
		fatalError(code, current.value, current.loc);
	}

	Token advance() {
		checkNotError();
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

	[[noreturn]]
	void fatalError(coda::ParseErrorCode code,
	                const std::string& msg,
	                const SourceLoc& loc)
	{
		throw coda::ParseError(code, loc, msg, filename, extractLine(loc.lineStart));
	}

	// ── comment handling ────────────────────────────────────────────────

	std::string takeComment() {
		std::string c = std::move(pendingComment);
		pendingComment.clear();
		return c;
	}

	// ── duplicate-key guard for Block ───────────────────────────────────

	// Inserts directly into the Block's underlying map so we can check for
	// duplicates.  Block::operator[] auto-inserts without a duplicate check,
	// and Block::insert() doesn't report whether the key already existed, so
	// we access getContent() directly here.
	void blockInsertChecked(coda::Block& block,
	                        const std::string& key,
	                        coda::detail::Value value,
	                        const SourceLoc& loc)
	{
		auto& map = block.getContent();
		if (map.count(key))
			fatalError(coda::ParseErrorCode::DuplicateKey,
			           "duplicate key '" + key + "'", loc);
		map[key] = std::make_unique<coda::detail::Value>(std::move(value));
	}

	// Same guard for KeyedTable rows.
	void keyedTableInsertChecked(coda::KeyedTable& table,
	                             const std::string& key,
	                             coda::Row row,
	                             const SourceLoc& loc)
	{
		auto& map = table.getContent();
		if (map.count(key))
			fatalError(coda::ParseErrorCode::DuplicateKey,
			           "duplicate key '" + key + "'", loc);
		map[key] = std::move(row);
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

	coda::detail::Value parseValue() {
		std::string comment = takeComment();

		checkNotError();

		coda::detail::Value v;
		if (current.type == TokenType::LBrace) {
			v = coda::detail::Value(parseBlock());
		} else if (current.type == TokenType::LBracket) {
			v = parseArray();
		} else if (current.type == TokenType::Ident
		        || current.type == TokenType::String
		        || current.type == TokenType::Key) {
			// TokenType::Key ('key') is reserved as a table header marker, but
			// when it appears in a value position it is just the string "key".
			v = coda::detail::Value(advance().value);
		} else {
			fatalError(coda::ParseErrorCode::UnexpectedToken,
			           "expected value (string, identifier, block, or array), got "
			           + tokenToString.at(current.type),
			           current.loc);
		}

		v.setComment(std::move(comment));
		return v;
	}

	coda::Block parseBlock() {
		expect(TokenType::LBrace);
		expectLineEnd();

		coda::Block block;
		while (current.type != TokenType::RBrace && current.type != TokenType::Eof) {
			if (current.type == TokenType::Key)
				fatalError(coda::ParseErrorCode::KeyInBlock,
				           "'key' header not allowed inside block — use [] for tables",
				           current.loc);

			Token keyTok = expectKey();
			coda::detail::Value val = parseValue();
			blockInsertChecked(block, keyTok.value, std::move(val), keyTok.loc);
			skipNewlines();
		}

		expect(TokenType::RBrace);
		return block;
	}

	// ── array / table parsing ───────────────────────────────────────────

	coda::detail::Value parseArray() {
		expect(TokenType::LBracket);
		expectLineEnd();

		if (current.type == TokenType::Key) {
			std::string headerComment = takeComment();
			return parseKeyedTable(std::move(headerComment));
		}
		if (current.type == TokenType::LBrace || current.type == TokenType::LBracket)
			return parseNestedList();
		return parseAutoList();
	}

	// Produces a KeyedTable: rows are indexed by their first token ("key" column).
	coda::detail::Value parseKeyedTable(std::string headerComment) {
		advance(); // consume 'key'

		std::vector<Token> fieldToks;
		while (current.type == TokenType::Ident || current.type == TokenType::String)
			fieldToks.push_back(advance());
		checkUniqueFields(fieldToks);
		skipNewlines();

		std::set<std::string> headerSet;
		for (const auto& tok : fieldToks) headerSet.insert(tok.value);
		coda::KeyedTable table(std::move(headerSet));
		table.setHeaderComment(std::move(headerComment));

		while (current.type != TokenType::RBracket && current.type != TokenType::Eof) {
			std::string comment = takeComment();
			auto rowTokens = collectFlatRow();
			skipNewlines();
			if (rowTokens.empty()) continue;

			if (rowTokens.size() - 1 != fieldToks.size())
				fatalError(
					coda::ParseErrorCode::RaggedRow,
					"row '" + rowTokens[0].value + "' has "
					+ std::to_string(rowTokens.size() - 1) + " value(s), expected "
					+ std::to_string(fieldToks.size()),
					rowTokens[0].loc
				);

			coda::Row row;
			row.setComment(std::move(comment));
			for (size_t i = 0; i < fieldToks.size(); ++i)
				row[fieldToks[i].value] = rowTokens[i + 1].value;

			keyedTableInsertChecked(table, rowTokens[0].value, std::move(row), rowTokens[0].loc);
		}

		expect(TokenType::RBracket);
		return coda::detail::Value(std::move(table));
	}

	// Produces an Array of nested blocks/arrays.
	coda::detail::Value parseNestedList() {
		coda::Array array;
		while (current.type != TokenType::RBracket && current.type != TokenType::Eof) {
			skipNewlines();
			if (current.type == TokenType::RBracket) break;
			array.append(parseValue());
			skipNewlines();
		}

		expect(TokenType::RBracket);
		return coda::detail::Value(std::move(array));
	}

	// Dispatches to either parsePlainTable (multi-column header row) or
	// parseBareList (single-column / no header).
	coda::detail::Value parseAutoList() {
		std::string firstComment = takeComment();
		auto firstRow = collectFlatRow();
		skipNewlines();

		if (firstRow.size() > 1)
			return parsePlainTable(std::move(firstRow), std::move(firstComment));
		return parseBareList(std::move(firstRow), std::move(firstComment));
	}

	// Produces a Table: the first row is the column-name header; subsequent
	// rows become Row objects appended in order.
	coda::detail::Value parsePlainTable(std::vector<Token> header, std::string headerComment) {
		checkUniqueFields(header);

		std::set<std::string> headerSet;
		for (const auto& tok : header) headerSet.insert(tok.value);
		coda::Table table(std::move(headerSet));
		table.setHeaderComment(std::move(headerComment));

		while (current.type != TokenType::RBracket && current.type != TokenType::Eof) {
			std::string comment = takeComment();
			auto rowTokens = collectFlatRow();
			skipNewlines();
			if (rowTokens.empty()) continue;

			if (rowTokens.size() != header.size())
				fatalError(
					coda::ParseErrorCode::RaggedRow,
					"row has " + std::to_string(rowTokens.size())
					+ " value(s), expected " + std::to_string(header.size()),
					rowTokens[0].loc
				);

			coda::Row row;
			row.setComment(std::move(comment));
			for (size_t i = 0; i < header.size(); ++i)
				row[header[i].value] = rowTokens[i].value;

			table.append(std::move(row));
		}

		expect(TokenType::RBracket);
		return coda::detail::Value(std::move(table));
	}

	// Produces an Array of string Values (bare list, one token per line).
	coda::detail::Value parseBareList(std::vector<Token> firstRow, std::string firstComment) {
		coda::Array array;

		if (!firstRow.empty()) {
			coda::detail::Value firstVal(firstRow[0].value);
			firstVal.setComment(std::move(firstComment));
			array.append(std::move(firstVal));
		}

		while (current.type != TokenType::RBracket && current.type != TokenType::Eof) {
			skipNewlines();
			if (current.type == TokenType::RBracket) break;
			array.append(parseValue());
			skipNewlines();
		}

		expect(TokenType::RBracket);
		return coda::detail::Value(std::move(array));
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

	coda::Block parse() {
		coda::Block root;
		skipNewlines();
		while (current.type != TokenType::Eof) {
			Token keyTok            = expectKey();
			coda::detail::Value val = parseValue();
			blockInsertChecked(root, keyTok.value, std::move(val), keyTok.loc);
			skipNewlines();
		}
		return root;
	}

};

} // namespace detail

} // namespace coda

#include <fstream>
#include <sstream>
#include <string>
#include <functional>

namespace coda {

class Doc {
	std::string indentUnit = "\t";
	Block rootBlock;

public:
	Doc() = default;
	Doc(const std::string& path) {
		std::ifstream f(path, std::ios::binary);
		if (!f) throw std::runtime_error("could not open: " + path);
		std::ostringstream ss;
		ss << f.rdbuf();
		rootBlock = detail::Parser(ss.str(), path).parse();
	}

	static Doc parse(std::string content, std::string filename = "") {
		Doc doc;
		doc.rootBlock = detail::Parser(content, filename).parse();
		return doc;
	}

	void useTabs()              { indentUnit = "\t"; }
	void useSpaces(int count)   { indentUnit = std::string(count, ' '); }

	/// Recursively sort all fields: scalars first (alphabetical),
	/// then containers (alphabetical). Array element order is preserved.
	void order() { rootBlock.order(); }

	/// Recursively sort all fields by a weight function.
	/// Higher weight → closer to the top. Equal weight → alphabetical.
	/// Array element order is preserved; their children are still sorted.
	///
	/// Example:
	///   doc.order([](const std::string& field) -> float {
	///       if (field == "name") return 100;
	///       if (field == "type") return 50;
	///       return 0; // everything else alphabetical at the bottom
	///   });
	void order(const std::function<float(const std::string&)>& weightFn) {
		rootBlock.order(weightFn);
	}

	void save(const std::string& path) const {
		std::ofstream f(path);
		if (!f) throw std::runtime_error("could not open: " + path);
		f << rootBlock.serialize(indentUnit);
	}

	void save(const std::string& path, const std::string& unit) {
		indentUnit = unit;
		save(path);
	}

	std::string serialize() const {
		return rootBlock.serialize(indentUnit);
	}

	const Block& root() const { return rootBlock; }
	Block&       root()       { return rootBlock; }
};

} // namespace coda
