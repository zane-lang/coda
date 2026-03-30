#pragma once

#include "ast.hpp"
#include "parser.hpp"
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <map>

class Coda {
	std::string indentUnit = "\t"; 

	std::string pad(int level) const {
		std::string out;
		for (int i = 0; i < level; ++i) out += indentUnit;
		return out;
	}

	static std::string serializeToken(const std::string& s) {
		if (s.empty() || s.find(' ') != std::string::npos || s.find('\n') != std::string::npos)
			return '"' + s + '"';
		return s;
	}

	std::string serializeTableRow(const CodaTable& t, const std::vector<std::string>& fields) const {
		std::string out;
		for (size_t i = 0; i < fields.size(); ++i) {
			out += serializeValue(t.content.at(fields[i]), 0); 
			if (i < fields.size() - 1) out += " ";
		}
		return out;
	}

	// Helper to check if a CodaTable is a "Keyed Table" (a collection of rows)
	bool isKeyedTable(const CodaTable& t) const {
		if (t.content.empty()) return false;
		// If the values inside this table are themselves tables, it's a keyed collection
		return std::holds_alternative<CodaTable>(t.content.begin()->second.content.value);
	}

	// Helper to extract field names from the first row of a keyed table
	std::vector<std::string> fieldsOfKeyed(const CodaTable& t) const {
		std::vector<std::string> fields;
		if (t.content.empty()) return fields;
		
		// The first value in the table is the first row
		const CodaTable& firstRow = t.content.begin()->second.asTable();
		for (const auto& [k, _] : firstRow.content) {
			fields.push_back(k);
		}
		return fields;
	}

	std::string serializeKeyedTable(const CodaTable& t, int indent) const {
		if (t.content.empty()) return "[]";
		
		std::vector<std::string> fields = fieldsOfKeyed(t);
		std::string out = "[\n";

		// Header: key field1 field2 ...
		out += pad(indent + 1) + "key";
		for (const auto& f : fields) out += " " + serializeToken(f);
		out += "\n";

		// Rows: rowKey val1 val2 ...
		for (const auto& [rowKey, rowVal] : t.content) {
			out += pad(indent + 1) + serializeToken(rowKey);
			const CodaTable& row = rowVal.asTable();
			for (const auto& f : fields) {
				out += " " + serializeValue(row.content.at(f), 0);
			}
			out += "\n";
		}

		out += pad(indent) + "]";
		return out;
	}

	std::string serializeValue(const CodaValue& val, int indent) const {
		return val.content.match(
			[](const std::string& s) -> std::string { return serializeToken(s); },
			[&](const CodaBlock& b)   -> std::string { return serializeBlock(b, indent); },
			[&](const CodaArray& a)   -> std::string { return serializeArray(a, indent); },
			[&](const CodaTable& t)   -> std::string {
				if (isKeyedTable(t)) {
					return serializeKeyedTable(t, indent);
				}
				// Fallback for "Plain Table" rows (inline serialization)
				std::string out;
				for (auto it = t.content.begin(); it != t.content.end(); ++it) {
					out += serializeValue(it->second, 0);
					if (std::next(it) != t.content.end()) out += " ";
				}
				return out;
			}
		);
	}

	std::string serializeBlock(const CodaBlock& block, int indent) const {
		std::string out = "{\n";
		out += serializeSortedMap(block.content, indent + 1);
		out += pad(indent) + "}";
		return out;
	}

	template<typename T>
	std::string serializeSortedMap(const std::map<std::string, T>& m, int indent) const {
		std::vector<std::string> scalars;
		std::vector<std::string> containers;

		for (const auto& [k, v] : m) {
			if (v.isContainer()) containers.push_back(k);
			else scalars.push_back(k);
		}

		std::string out;
		for (const auto& k : scalars) {
			out += pad(indent) + k + " " + serializeValue(m.at(k), indent) + "\n";
		}

		for (const auto& k : containers) {
			out += "\n"; 
			out += pad(indent) + k + " " + serializeValue(m.at(k), indent) + "\n";
		}
		return out;
	}

	std::string serializeArray(const CodaArray& array, int indent) const {
		if (array.content.empty()) return "[]";
		
		std::string out = "[\n";
		
		// Check if this is a "Plain Table" array (array of CodaTables)
		if (std::holds_alternative<CodaTable>(array[0].content.value)) {
			// Get fields from the first row
			const CodaTable& firstRow = array[0].asTable();
			std::vector<std::string> fields;
			for (const auto& [k, _] : firstRow.content) fields.push_back(k);

			// Header row
			out += pad(indent + 1);
			for (size_t i = 0; i < fields.size(); ++i) {
				out += fields[i] + (i < fields.size() - 1 ? " " : "");
			}
			out += "\n";

			// Data rows
			for (const auto& row : array) {
				out += pad(indent + 1) + serializeTableRow(row.asTable(), fields) + "\n";
			}
		}
		else {
			// Bare list (array of scalars or nested blocks)
			for (const auto& v : array) {
				out += pad(indent + 1) + serializeValue(v, indent + 1) + "\n";
			}
		}
		
		out += pad(indent) + "]";
		return out;
	}

public:
	CodaFile file;

	Coda() = default;
	Coda(const std::string& path) {
		std::ifstream f(path);
		if (!f) throw std::runtime_error("could not open: " + path);
		std::ostringstream ss;
		ss << f.rdbuf();
		file = Parser(ss.str()).parseFile();
	}

	void useTabs() { indentUnit = "\t"; }
	void useSpaces(int count) { indentUnit = std::string(count, ' '); }

	void save(const std::string& path) const {
		std::ofstream f(path);
		if (!f) throw std::runtime_error("could not open: " + path);
		f << serializeSortedMap(file.statements, 0);
	}

	void save(const std::string& path, std::string indentUnit) {
		this->indentUnit = indentUnit;
		save(path);
	}

	const CodaValue& operator[](const std::string& key) const { return file[key]; }
	CodaValue& operator[](const std::string& key) { return file[key]; }
};
