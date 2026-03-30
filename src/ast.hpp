#pragma once

#include "types.hpp"
#include "zane_ptr.hpp"

#include <map>
#include <string>
#include <vector>

struct ZifValue;

struct ZifFile {
	std::map<std::string, Ptr<ZifValue>> statements;
};

struct ZifTable {
	std::map<std::string, std::string> content;
};


// {} — either struct-like children or a keyed table
struct ZifBlock {
	Variant<
	std::map<std::string, Ptr<ZifValue>>,
	std::map<std::string, ZifTable>
	> content;
};

// [] — keyed table, plain table, or bare list
struct ZifArray {
	Variant<
	std::map<std::string, ZifTable>,
	std::vector<std::string>,
	std::vector<ZifTable>
	> content;
};

struct ZifValue {
	Variant<
		std::string,
		ZifBlock,
		ZifArray
	> content;

    const std::string& asString() const {
        return std::get<std::string>(content.value);
    }
    const ZifBlock& asBlock() const {
        return std::get<ZifBlock>(content.value);
    }
    const ZifArray& asArray() const {
        return std::get<ZifArray>(content.value);
    }
};
