#pragma once
#include "types.hpp"
#include "zane_ptr.hpp"
#include <map>
#include <string>
#include <vector>
#include <stdexcept>

struct CodaValue;
struct CodaBlock;
struct CodaArray;
struct CodaTable;

struct CodaTable {
	std::map<std::string, CodaValue> content;

	const CodaValue& operator[](const std::string& key) const {
		return content.at(key);
	}
	CodaValue& operator[](const std::string& key) {
		return content.at(key);
	}

	auto begin() const { return content.begin(); }
	auto begin() { return content.begin(); }

    auto end() const { return content.end(); }
    auto end() { return content.end(); }
};

struct CodaBlock {
	std::map<std::string, CodaValue> content;

	const CodaValue& operator[](const std::string& key) const {
		return content.at(key);
	}
	CodaValue& operator[](const std::string& key) {
		return content.at(key);
	}

	auto begin() const { return content.begin(); }
	auto begin() { return content.begin(); }

    auto end() const { return content.end(); }
    auto end() { return content.end(); }
};

struct CodaArray {
	std::vector<CodaValue> content;

	const CodaValue& operator[](size_t i) const {
		return content.at(i);
	}
	CodaValue& operator[](size_t i) {
		return content.at(i);
	}

	auto begin() const { return content.begin(); }
	auto begin() { return content.begin(); }

	auto end() const { return content.end(); }
	auto end() { return content.end(); }
};

struct CodaValue {
	Variant<
		std::string,
		CodaBlock,
		CodaArray,
		CodaTable
	> content;
	std::string comment;

	CodaValue() : content(std::string("")) {}
	CodaValue(std::string str)  : content(std::move(str)) {}
	CodaValue(CodaBlock block)  : content(std::move(block)) {}
	CodaValue(CodaArray arr)	: content(std::move(arr)) {}
	CodaValue(CodaTable table)  : content(std::move(table)) {}
	CodaValue(const char* str)  : content(std::string(str)) {}

	CodaValue& operator=(std::string str)   { content = std::move(str);   return *this; }
	CodaValue& operator=(CodaBlock block)   { content = std::move(block); return *this; }
	CodaValue& operator=(CodaArray arr)	 { content = std::move(arr);   return *this; }
	CodaValue& operator=(CodaTable table)   { content = std::move(table); return *this; }
	CodaValue& operator=(const char* str)   { content = std::string(str); return *this; }

	const CodaValue& operator[](const std::string& key) const {
		return content.match(
			[&](const CodaBlock& b) -> const CodaValue& { return b[key]; },
			[&](const CodaTable& t) -> const CodaValue& { return t[key]; },
			[](const auto&) -> const CodaValue& {
				throw std::runtime_error("type does not support string indexing");
			}
		);
	}
	CodaValue& operator[](const std::string& key) {
		return content.match(
			[&](CodaBlock& b) -> CodaValue& { return b[key]; },
			[&](CodaTable& t) -> CodaValue& { return t[key]; },
			[](auto&) -> CodaValue& {
				throw std::runtime_error("type does not support string indexing");
			}
		);
	}

	const CodaValue& operator[](size_t i) const {
		return content.match(
			[&](const CodaArray& a) -> const CodaValue& { return a[i]; },
			[](const auto&) -> const CodaValue& {
				throw std::runtime_error("type does not support integer indexing");
			}
		);
	}
	CodaValue& operator[](size_t i) {
		return content.match(
			[&](CodaArray& a) -> CodaValue& { return a[i]; },
			[](auto&) -> CodaValue& {
				throw std::runtime_error("type does not support integer indexing");
			}
		);
	}

	const std::string& asString() const { return std::get<std::string>(content.value); }
	std::string& asString() { return std::get<std::string>(content.value); }

	const CodaBlock& asBlock() const { return std::get<CodaBlock>(content.value); }
	CodaBlock& asBlock() { return std::get<CodaBlock>(content.value); }

	const CodaArray& asArray() const { return std::get<CodaArray>(content.value); }
	CodaArray& asArray() { return std::get<CodaArray>(content.value); }

	const CodaTable& asTable() const { return std::get<CodaTable>(content.value); }
	CodaTable& asTable() { return std::get<CodaTable>(content.value); }

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
		if (auto b = std::get_if<CodaBlock>(&content.value))
			return *b;
		throw std::runtime_error("CodaValue: Expected Block");
	}
	operator CodaBlock&() {
		if (auto b = std::get_if<CodaBlock>(&content.value))
			return *b;
		throw std::runtime_error("CodaValue: Expected Block");
	}

	operator const CodaArray&() const {
		if (auto a = std::get_if<CodaArray>(&content.value))
			return *a;
		throw std::runtime_error("CodaValue: Expected Array");
	}
	operator CodaArray&() {
		if (auto a = std::get_if<CodaArray>(&content.value))
			return *a;
		throw std::runtime_error("CodaValue: Expected Array");
	}

	operator const CodaTable&() const {
		if (auto t = std::get_if<CodaTable>(&content.value))
			return *t;
		throw std::runtime_error("CodaValue: Expected Table");
	}
	operator CodaTable&() {
		if (auto t = std::get_if<CodaTable>(&content.value))
			return *t;
		throw std::runtime_error("CodaValue: Expected Table");
	}
};

struct CodaFile {
	std::map<std::string, CodaValue> statements;
	const CodaValue& operator[](const std::string& key) const {
		return statements.at(key);
	}
	CodaValue& operator[](const std::string& key) {
		return statements.at(key);
	}

	bool has(const std::string& key) const {
		return statements.count(key) > 0;
	}
};
