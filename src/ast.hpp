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
};

struct CodaBlock {
    Variant<
        std::map<std::string, Ptr<CodaValue>>,  // struct mode — nesting allowed
        std::map<std::string, CodaValue>         // table mode — CodaValue wraps CodaTable
    > content;
    const CodaValue& operator[](const std::string& key) const;
};

struct CodaArray {
    Variant<
        std::map<std::string, CodaValue>,  // keyed table — CodaValue wraps CodaTable
        std::vector<CodaValue>             // bare list or plain table rows
    > content;
    const CodaValue& operator[](const std::string& key) const;
    const CodaValue& operator[](size_t i) const;
};

struct CodaValue {
    Variant<
        std::string,
        CodaBlock,
        CodaArray,
        CodaTable
    > content;

	CodaValue() : content(std::string("")) {}
	CodaValue(std::string str) : content(std::move(str)) {}
	CodaValue(CodaBlock block)   : content(std::move(block)) {}
	CodaValue(CodaArray arr)     : content(std::move(arr)) {}
	CodaValue(CodaTable table)   : content(std::move(table)) {}
	CodaValue(const char* str)  : content(std::string(str)) {}

	CodaValue& operator=(std::string str) {
		content = std::move(str);
		return *this;
	}

	CodaValue& operator=(CodaBlock block) {
		content = std::move(block);
		return *this;
	}

	CodaValue& operator=(CodaArray arr) {
		content = std::move(arr);
		return *this;
	}

	CodaValue& operator=(CodaTable table) {
		content = std::move(table);
		return *this;
	}

	CodaValue& operator=(const char* str) {
		content = std::string(str);
		return *this;
	}

    const CodaValue& operator[](const std::string& key) const {
        return content.visit(overloaded{
            [&](const CodaBlock& b) -> const CodaValue& { return b[key]; },
            [&](const CodaArray& a) -> const CodaValue& { return a[key]; },
			[&](const CodaTable& t) -> const CodaValue& { return t[key]; },
            [](const auto&) -> const CodaValue& {
                throw std::runtime_error("type does not support string indexing");
            }
        });
    }
    const CodaValue& operator[](size_t i) const {
        return content.visit(overloaded{
            [&](const CodaArray& a) -> const CodaValue& { return a[i]; },
            [](const auto&) -> const CodaValue& {
                throw std::runtime_error("type does not support integer indexing");
            }
        });
    }

	CodaValue& operator[](const std::string& key) {
		return const_cast<CodaValue&>(static_cast<const CodaValue&>(*this)[key]);
	}

	CodaValue& operator[](size_t i) {
		return const_cast<CodaValue&>(static_cast<const CodaValue&>(*this)[i]);
	}

    const std::string& asString() const { return std::get<std::string>(content.value); }
    const CodaBlock&    asBlock()  const { return std::get<CodaBlock>(content.value); }
    const CodaArray&    asArray()  const { return std::get<CodaArray>(content.value); }
    const CodaTable&    asTable()  const { return std::get<CodaTable>(content.value); }

	bool isContainer() const {
		return std::holds_alternative<CodaBlock>(content.value) ||
			std::holds_alternative<CodaArray>(content.value);
	}
};

inline const CodaValue& CodaBlock::operator[](const std::string& key) const {
    return content.visit(overloaded{
        [&](const std::map<std::string, Ptr<CodaValue>>& m) -> const CodaValue& {
            return *m.at(key);
        },
        [&](const std::map<std::string, CodaValue>& m) -> const CodaValue& {
            return m.at(key);
        }
    });
}

inline const CodaValue& CodaArray::operator[](const std::string& key) const {
    return content.visit(overloaded{
        [&](const std::map<std::string, CodaValue>& m) -> const CodaValue& {
            return m.at(key);
        },
        [](const auto&) -> const CodaValue& {
            throw std::runtime_error("not a keyed table");
        }
    });
}

inline const CodaValue& CodaArray::operator[](size_t i) const {
    return content.visit(overloaded{
        [&](const std::vector<CodaValue>& v) -> const CodaValue& {
            return v.at(i);
        },
        [](const auto&) -> const CodaValue& {
            throw std::runtime_error("not a list");
        }
    });
}

struct CodaFile {
    std::map<std::string, Ptr<CodaValue>> statements;
    const CodaValue& operator[](const std::string& key) const { return *statements.at(key); }
    CodaValue&       operator[](const std::string& key)       { return *statements.at(key); }
    bool has(const std::string& key) const { return statements.count(key) > 0; }
};
