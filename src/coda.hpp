#pragma once
#include "ast.hpp"
#include "parser.hpp"
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
		doc.rootBlock = detail::Parser(std::move(content), std::move(filename)).parse();
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
		save(path, indentUnit);
	}

	/// Save using an explicit indent unit. Does NOT mutate the document's
	/// default indent (`useTabs`/`useSpaces` set that). Pure, const operation.
	void save(const std::string& path, const std::string& unit) const {
		std::ofstream f(path);
		if (!f) throw std::runtime_error("could not open: " + path);
		f << rootBlock.serialize(unit);
	}

	std::string serialize() const {
		return rootBlock.serialize(indentUnit);
	}

	const Block& root() const { return rootBlock; }
	Block&       root()       { return rootBlock; }
};

} // namespace coda
