// examples/cpp.cpp — Coda C++ API tour
//
// Build and run from the repository root:
//   rake run
//
// Or compile directly:
//   c++ -std=c++17 -I. examples/cpp.cpp -o build/run && ./build/run

#include "include/coda.hpp"
#include <iostream>

// ─── Sample document ─────────────────────────────────────────────────────────

static const char* SOURCE = R"coda(
# project configuration
name myproject
version 1.0.0

compiler {
	debug false
	optimize true
}

targets [
	x86_64-linux
	x86_64-windows
	aarch64-macos
]

releases [
	version date
	1.0.0   2025-01-01
	1.1.0   2025-06-15
]

# dependency table
deps [
	# optional
	key link version
	plot github.com/zane-lang/plot 4.0.3
	http github.com/zane-lang/http 2.1.0
]
)coda";

// ─── 1. Parse & read ─────────────────────────────────────────────────────────

static void demo_read() {
	coda::Doc doc = coda::Doc::parse(SOURCE, "example.coda");
	const coda::Block& root = doc.root();

	// Scalar strings
	std::cout << "name:    " << root["name"].asString() << "\n";
	std::cout << "version: " << root["version"].asString() << "\n";

	// Membership test
	std::cout << "has author: " << std::boolalpha << root.has("author") << "\n";

	// Block
	const coda::Block& compiler = root["compiler"].asBlock();
	std::cout << "compiler:\n";
	for (const auto& [key, valPtr] : compiler)
		std::cout << "  " << key << " = " << valPtr->asString() << "\n";

	// Array
	const coda::Array& targets = root["targets"].asArray();
	std::cout << "targets (" << targets.size() << "):\n";
	for (size_t i = 0; i < targets.size(); ++i)
		std::cout << "  " << targets[i].asString() << "\n";

	// Plain table
	const coda::Table& releases = root["releases"].asTable();
	std::cout << "releases:\n";
	for (const auto& row : releases)
		std::cout << "  " << row["version"] << "  " << row["date"] << "\n";

	// Keyed table
	const coda::KeyedTable& deps = root["deps"].asKeyedTable();
	std::cout << "deps comment:        " << root["deps"].getComment() << "\n";
	std::cout << "deps header comment: " << deps.getHeaderComment() << "\n";
	std::cout << "plot link: " << deps["plot"]["link"] << "\n";
	std::cout << "all deps:\n";
	for (const auto& [key, row] : deps)
		std::cout << "  " << key << " → " << row["link"] << " @ " << row["version"] << "\n";
}

// ─── 2. Build a document from scratch ────────────────────────────────────────

static void demo_build() {
	coda::Doc doc;
	coda::Block& root = doc.root();

	// Scalars
	root["name"]    = "myproject";
	root["version"] = "1.0.0";

	// Block
	coda::Block compiler;
	compiler["debug"]    = "false";
	compiler["optimize"] = "true";
	root["compiler"] = std::move(compiler);

	// Array
	coda::Array targets;
	targets.setHeaderComment("supported build targets");
	targets.append("x86_64-linux")
	       .append("x86_64-windows")
	       .append("aarch64-macos");
	root["targets"] = std::move(targets);

	// Plain table
	coda::Table releases({"version", "date"});
	releases.append(coda::Row().insert("version", "1.0.0").insert("date", "2025-01-01"));
	releases.append(coda::Row().insert("version", "1.1.0").insert("date", "2025-06-15"));
	root["releases"] = std::move(releases);

	// Keyed table
	coda::KeyedTable deps({"link", "version"});
	deps.setHeaderComment("optional");
	deps.insert("plot", coda::Row()
		.insert("link",    "github.com/zane-lang/plot")
		.insert("version", "4.0.3"));
	deps.insert("http", coda::Row()
		.insert("link",    "github.com/zane-lang/http")
		.insert("version", "2.1.0"));
	root["deps"] = std::move(deps);
	root["deps"].setComment("dependency table");

	// Sort: name and version first, then everything else alphabetically
	doc.order([](const std::string& key) -> float {
		if (key == "name")    return 100;
		if (key == "version") return  90;
		return 0;
	});

	doc.useSpaces(2);
	std::cout << doc.serialize();
}

// ─── 3. Modify an existing document ──────────────────────────────────────────

static void demo_modify() {
	coda::Doc doc = coda::Doc::parse(SOURCE, "example.coda");
	coda::Block& root = doc.root();

	// Change a scalar in-place
	root["version"].asString() = "2.0.0";

	// Add a new key
	root["author"] = "zane";

	// Append to an array
	root["targets"].asArray().append("wasm32-wasi");

	// Update a keyed table row
	root["deps"].asKeyedTable()["plot"]["version"] = "5.0.0";

	std::cout << doc.serialize();
}

// ─── 4. Error handling ───────────────────────────────────────────────────────

static void demo_errors() {
	// Inline blocks are not allowed
	try {
		coda::Doc doc = coda::Doc::parse("compiler { debug false }", "bad.coda");
	} catch (const coda::ParseError& e) {
		std::cerr << "parse error: " << e.message << "\n";
		std::cerr << "  at " << e.filename << ":" << e.loc.line << ":" << e.loc.col << "\n";
	}

	// Type mismatch at runtime
	try {
		coda::Doc doc = coda::Doc::parse(SOURCE);
		doc.root()["name"].asBlock();  // "name" is a string, not a block
	} catch (const std::runtime_error& e) {
		std::cerr << "type error: " << e.what() << "\n";
	}
}

// ─────────────────────────────────────────────────────────────────────────────

int main() {
	std::cout << "=== Reading ===\n";
	demo_read();

	std::cout << "\n=== Building ===\n";
	demo_build();

	std::cout << "\n=== Modifying ===\n";
	demo_modify();

	std::cout << "\n=== Errors ===\n";
	demo_errors();

	return 0;
}

