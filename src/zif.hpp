#pragma once
#include "ast.hpp"
#include "parser.hpp"
#include <fstream>
#include <sstream>

class Zif {
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

	static std::string serializeTableRow(const ZifTable& t, const std::vector<std::string>& fields) {
		std::string out;
		for (size_t i = 0; i < fields.size(); ++i) {
			out += serializeToken(t.content.at(fields[i]).asString()); // <- Add .asString()
			if (i < fields.size() - 1) out += " ";
		}
		return out;
	}

	static std::vector<std::string> fieldsOf(const ZifTable& t) {
		std::vector<std::string> fields;
		for (const auto& [k, _] : t.content) fields.push_back(k);
		return fields;
	}

	std::string serializeValue(const ZifValue& val, int indent) const {
		return val.content.visit(overloaded{
			[](const std::string& s) { return serializeToken(s); },
			[&](const ZifBlock& b)   { return serializeBlock(b, indent); },
			[&](const ZifArray& a)   { return serializeArray(a, indent); },
			[](const ZifTable& t) {
				std::string out;
				for (auto it = t.content.begin(); it != t.content.end(); ++it) {
					out += serializeToken(it->second.asString()); // <- Add .asString()
					if (std::next(it) != t.content.end()) out += " ";
				}
				return out;
			}
		});
	}

	std::string serializeBlock(const ZifBlock& block, int indent) const {
		std::string out = "{\n";
		out += block.content.visit(overloaded{
			[&](const std::map<std::string, Ptr<ZifValue>>& children) {
				return serializeSortedMap(children, indent + 1);
			},
			[&](const std::map<std::string, ZifValue>& table) {
				std::string inner;
				if (table.empty()) return inner;
				auto fields = fieldsOf(table.begin()->second.asTable());
				inner += pad(indent + 1) + "key";
				for (const auto& f : fields) inner += " " + f;
				inner += "\n";
				for (const auto& [rowKey, rowVal] : table) {
					inner += pad(indent + 1) + serializeToken(rowKey) + " "
						+ serializeTableRow(rowVal.asTable(), fields) + "\n";
				}
				return inner;
			}
		});
		out += pad(indent) + "}";
		return out;
	}

	template<typename T>
	std::string serializeSortedMap(const std::map<std::string, T>& m, int indent) const {
		std::vector<std::string> scalars;
		std::vector<std::string> containers;

		for (const auto& [k, v] : m) {
			if constexpr (std::is_same_v<T, Ptr<ZifValue>>) {
				if (v->isContainer()) containers.push_back(k);
				else scalars.push_back(k);
			} else {
				if (v.isContainer()) containers.push_back(k);
				else scalars.push_back(k);
			}
		}

		std::string out;
		for (const auto& k : scalars) {
			if constexpr (std::is_same_v<T, Ptr<ZifValue>>) 
				out += pad(indent) + k + " " + serializeValue(*m.at(k), indent) + "\n";
			else 
				out += pad(indent) + k + " " + serializeValue(m.at(k), indent) + "\n";
		}

		for (const auto& k : containers) {
			out += "\n"; // Padding above container
			if constexpr (std::is_same_v<T, Ptr<ZifValue>>) 
				out += pad(indent) + k + " " + serializeValue(*m.at(k), indent) + "\n";
			else 
				out += pad(indent) + k + " " + serializeValue(m.at(k), indent) + "\n";
		}
		return out;
	}

	std::string serializeArray(const ZifArray& array, int indent) const {
		std::string out = "[\n";
		out += array.content.visit(overloaded{
			[&](const std::map<std::string, ZifValue>& table) {
				std::string inner;
				if (table.empty()) return inner;
				auto fields = fieldsOf(table.begin()->second.asTable());
				inner += pad(indent + 1) + "key";
				for (const auto& f : fields) inner += " " + f;
				inner += "\n";
				for (const auto& [rowKey, rowVal] : table) {
					inner += pad(indent + 1) + serializeToken(rowKey) + " "
						+ serializeTableRow(rowVal.asTable(), fields) + "\n";
				}
				return inner;
			},
			[&](const std::vector<ZifValue>& list) {
				std::string inner;
				if (list.empty()) return inner;
				if (std::get_if<ZifTable>(&list[0].content.value)) {
					auto fields = fieldsOf(list[0].asTable());
					inner += pad(indent + 1);

					for (size_t i = 0; i < fields.size(); ++i) {
						inner += fields[i];
						if (i < fields.size() - 1) inner += " ";
					}

					inner += "\n";
					for (const auto& row : list)
					inner += pad(indent + 1) + serializeTableRow(row.asTable(), fields) + "\n";
				} else {
					for (const auto& v : list)
					inner += pad(indent + 1) + serializeValue(v, indent + 1) + "\n";
				}
				return inner;
			}
		});
		out += pad(indent) + "]";
		return out;
	}

public:
	ZifFile file;

	Zif() = default;
	Zif(const std::string& path) {
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

	const ZifValue& operator[](const std::string& key) const { return file[key]; }
	ZifValue& operator[](const std::string& key)	   { return file[key]; }
};
