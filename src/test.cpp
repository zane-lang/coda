#include "zif.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>

static std::string readFile(const std::string& path) {
	std::ifstream f(path);
	if (!f) throw std::runtime_error("could not open: " + path);
	std::ostringstream ss;
	ss << f.rdbuf();
	return ss.str();
}

void test() {
	std::string src = readFile("test.zif");
	Parser parser(std::move(src));
	ZifFile file = parser.parseFile();

	// ── name ────────────────────────────────────────────────────────────
	assert(file.statements.count("name"));
	assert(file.statements.at("name")->asString() == "myproject");
	std::cout << "PASS: name\n";

	// ── type ────────────────────────────────────────────────────────────
	assert(file.statements.count("type"));
	assert(file.statements.at("type")->asString() == "executable");
	std::cout << "PASS: type\n";

	// ── deps (keyed array table) ─────────────────────────────────────────
	assert(file.statements.count("deps"));
	auto deps = file.statements.at("deps")->asArray();
	const auto& depsTable = std::get<std::map<std::string, ZifTable>>(deps.content.value);

	assert(depsTable.count("plot"));
	assert(depsTable.at("plot").content.at("link")    == "github.com/zane-lang/plot");
	assert(depsTable.at("plot").content.at("version") == "4.0.3");

	assert(depsTable.count("http"));
	assert(depsTable.at("http").content.at("link")    == "github.com/zane-lang/http");
	assert(depsTable.at("http").content.at("version") == "2.1.0");
	std::cout << "PASS: deps\n";

	// ── compiler (struct-like block) ─────────────────────────────────────
	assert(file.statements.count("compiler"));
	auto compiler = file.statements.at("compiler")->asBlock();
	const auto& compilerChildren = std::get<std::map<std::string, Ptr<ZifValue>>>(compiler.content.value);

	assert(compilerChildren.count("debug"));
	assert(compilerChildren.at("debug")->asString() == "false");

	assert(compilerChildren.count("optimize"));
	assert(compilerChildren.at("optimize")->asString() == "true");
	std::cout << "PASS: compiler\n";

	// ── targets (bare list) ──────────────────────────────────────────────
	assert(file.statements.count("targets"));
	auto targets = file.statements.at("targets")->asArray();
	const auto& targetList = std::get<std::vector<std::string>>(targets.content.value);

	assert(targetList.size() == 3);
	assert(targetList[0] == "x86_64-linux");
	assert(targetList[1] == "x86_64-windows");
	assert(targetList[2] == "aarch64-macos");
	std::cout << "PASS: targets\n";

	// ── meta (struct-like block) ─────────────────────────────────────────
	assert(file.statements.count("meta"));
	auto meta = file.statements.at("meta")->asBlock();
	const auto& metaChildren = std::get<std::map<std::string, Ptr<ZifValue>>>(meta.content.value);

	assert(metaChildren.count("author"));
	assert(metaChildren.at("author")->asString() == "The Lazy Cat");

	assert(metaChildren.count("description"));
	assert(metaChildren.at("description")->asString() == "a test project");
	std::cout << "PASS: meta\n";

	std::cout << "\nAll tests passed.\n";
}

int main() {
	return 0;
}
