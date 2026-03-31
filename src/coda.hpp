#pragma once
#include "ast.hpp"
#include "parser.hpp"
#include <fstream>
#include <sstream>
#include <string>

class Coda {
	std::string indentUnit = "\t";

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

	void useTabs()              { indentUnit = "\t"; }
	void useSpaces(int count)   { indentUnit = std::string(count, ' '); }

	void save(const std::string& path) const {
		std::ofstream f(path);
		if (!f) throw std::runtime_error("could not open: " + path);
		f << file.serialize(indentUnit);
	}

	void save(const std::string& path, const std::string& unit) {
		indentUnit = unit;
		save(path);
	}

	const CodaValue& operator[](const std::string& key) const { return file[key]; }
	CodaValue&       operator[](const std::string& key)       { return file[key]; }
};
