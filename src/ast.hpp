#pragma once
#include "types.hpp"
#include "zane_ptr.hpp"
#include <map>
#include <string>
#include <vector>
#include <stdexcept>

struct ZifValue;
struct ZifBlock;
struct ZifArray;
struct ZifTable;

struct ZifTable {
	std::map<std::string, ZifValue> content;

	const ZifValue& operator[](const std::string& key) const {
		return content.at(key);
	}
	ZifValue& operator[](const std::string& key) {
		return content.at(key);
	}
};

struct ZifBlock {
    Variant<
        std::map<std::string, Ptr<ZifValue>>,  // struct mode — nesting allowed
        std::map<std::string, ZifValue>         // table mode — ZifValue wraps ZifTable
    > content;
    const ZifValue& operator[](const std::string& key) const;
};

struct ZifArray {
    Variant<
        std::map<std::string, ZifValue>,  // keyed table — ZifValue wraps ZifTable
        std::vector<ZifValue>             // bare list or plain table rows
    > content;
    const ZifValue& operator[](const std::string& key) const;
    const ZifValue& operator[](size_t i) const;
};

struct ZifValue {
    Variant<
        std::string,
        ZifBlock,
        ZifArray,
        ZifTable
    > content;

	ZifValue() : content(std::string("")) {}
	ZifValue(std::string str) : content(std::move(str)) {}
	ZifValue(ZifBlock block)   : content(std::move(block)) {}
	ZifValue(ZifArray arr)     : content(std::move(arr)) {}
	ZifValue(ZifTable table)   : content(std::move(table)) {}
	ZifValue(const char* str)  : content(std::string(str)) {}

	ZifValue& operator=(std::string str) {
		content = std::move(str);
		return *this;
	}

	ZifValue& operator=(ZifBlock block) {
		content = std::move(block);
		return *this;
	}

	ZifValue& operator=(ZifArray arr) {
		content = std::move(arr);
		return *this;
	}

	ZifValue& operator=(ZifTable table) {
		content = std::move(table);
		return *this;
	}

	ZifValue& operator=(const char* str) {
		content = std::string(str);
		return *this;
	}

    const ZifValue& operator[](const std::string& key) const {
        return content.visit(overloaded{
            [&](const ZifBlock& b) -> const ZifValue& { return b[key]; },
            [&](const ZifArray& a) -> const ZifValue& { return a[key]; },
			[&](const ZifTable& t) -> const ZifValue& { return t[key]; },
            [](const auto&) -> const ZifValue& {
                throw std::runtime_error("type does not support string indexing");
            }
        });
    }
    const ZifValue& operator[](size_t i) const {
        return content.visit(overloaded{
            [&](const ZifArray& a) -> const ZifValue& { return a[i]; },
            [](const auto&) -> const ZifValue& {
                throw std::runtime_error("type does not support integer indexing");
            }
        });
    }

	ZifValue& operator[](const std::string& key) {
		return const_cast<ZifValue&>(static_cast<const ZifValue&>(*this)[key]);
	}

	ZifValue& operator[](size_t i) {
		return const_cast<ZifValue&>(static_cast<const ZifValue&>(*this)[i]);
	}

    const std::string& asString() const { return std::get<std::string>(content.value); }
    const ZifBlock&    asBlock()  const { return std::get<ZifBlock>(content.value); }
    const ZifArray&    asArray()  const { return std::get<ZifArray>(content.value); }
    const ZifTable&    asTable()  const { return std::get<ZifTable>(content.value); }

	bool isContainer() const {
		return std::holds_alternative<ZifBlock>(content.value) ||
			std::holds_alternative<ZifArray>(content.value);
	}
};

inline const ZifValue& ZifBlock::operator[](const std::string& key) const {
    return content.visit(overloaded{
        [&](const std::map<std::string, Ptr<ZifValue>>& m) -> const ZifValue& {
            return *m.at(key);
        },
        [&](const std::map<std::string, ZifValue>& m) -> const ZifValue& {
            return m.at(key);
        }
    });
}

inline const ZifValue& ZifArray::operator[](const std::string& key) const {
    return content.visit(overloaded{
        [&](const std::map<std::string, ZifValue>& m) -> const ZifValue& {
            return m.at(key);
        },
        [](const auto&) -> const ZifValue& {
            throw std::runtime_error("not a keyed table");
        }
    });
}

inline const ZifValue& ZifArray::operator[](size_t i) const {
    return content.visit(overloaded{
        [&](const std::vector<ZifValue>& v) -> const ZifValue& {
            return v.at(i);
        },
        [](const auto&) -> const ZifValue& {
            throw std::runtime_error("not a list");
        }
    });
}

struct ZifFile {
    std::map<std::string, Ptr<ZifValue>> statements;
    const ZifValue& operator[](const std::string& key) const { return *statements.at(key); }
    ZifValue&       operator[](const std::string& key)       { return *statements.at(key); }
    bool has(const std::string& key) const { return statements.count(key) > 0; }
};
