#pragma once
#include "ast.hpp"
#include "parser.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include <functional>

class Coda {
	std::string indentUnit = "\t";
	CodaFile file;

public:
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

	const CodaValue& operator[](const std::string& key) const { return file[key]; }
	CodaValue&       operator[](const std::string& key)       { return file[key]; }
};
