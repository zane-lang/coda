require "rake"
require "fileutils"
include FileUtils

ZIG_CC = File.join(Dir.pwd, "scripts/zig-cc")
ZIG_CXX = File.join(Dir.pwd, "scripts/zig-cxx")
ZIG_AR = File.join(Dir.pwd, "scripts/zig-ar")

TEST_FLAGS = "-O0 -g -std=c++20"
FLAGS = "-O3 -std=c++20"
FLAGS_CROSS = "-O3 -std=c++20"

INC = "-Iinclude"

TARGETS = %w[
	x86_64-linux-gnu
	x86_64-linux-musl
	aarch64-linux-gnu
	aarch64-linux-musl
	x86_64-windows-gnu
	aarch64-windows-gnu
].freeze

# @param command [String]
# @return [void]
def sh!(command)
	sh command
end

desc "List available tasks"
task :list do
	sh! "rake -T"
end

task default: :list

desc "Run C++ tests"
task :"test-cpp" do
	mkdir_p "build"
	sh! "#{ZIG_CXX} #{TEST_FLAGS} -I. tests/cpp/test_main.cpp -o build/tests"
	sh! "./build/tests"
end

desc "Run C FFI tests"
task :"test-c-ffi" do
	mkdir_p "build"
	sh! "#{ZIG_CXX} #{TEST_FLAGS} -I. -c -fPIC ffi/coda_ffi.cpp -o build/coda_ffi.o"
	sh! "#{ZIG_AR} rcs build/libcoda_ffi_native.a build/coda_ffi.o"
	sh! "#{ZIG_CXX} #{TEST_FLAGS} -I. tests/cpp/test_c_ffi.cpp build/libcoda_ffi_native.a -o build/test_c_ffi"
	sh! "./build/test_c_ffi"
end

desc "Run Python FFI tests"
task :"test-py-ffi" do
	sh! "python3 tests/python/test_python_ffi.py"
end

desc "Run all tests"
task test: :"cross-all" do
	system("rake test-cpp")
	system("rake test-c-ffi")
	system("rake test-py-ffi")
end

desc "Build and run sample"
task run: :generate do
	mkdir_p "build"
	sh! "#{ZIG_CXX} #{TEST_FLAGS} -I. examples/cpp.cpp -o build/run"
	sh! "./build/run"
end

desc "Generate headers"
task :generate do
	sh! "quom src/coda.hpp include/coda.hpp"
end

desc "Build host shared library"
task :build do
	mkdir_p "build"
	sh! "#{ZIG_CXX} #{FLAGS} #{INC} -fPIC -shared -o build/libcoda_ffi.so ffi/coda_ffi.cpp"
end

desc "Cross build for one target"
task :cross, [:target] do |_t, args|
	target = args[:target]
	raise "target is required" if target.nil? || target.empty?

	mkdir_p "dist/#{target}"

	sh! "#{ZIG_CXX} #{FLAGS_CROSS} #{INC} -target #{target} -fPIC -c -o dist/#{target}/coda_ffi.o ffi/coda_ffi.cpp"

	if target.include?("windows")
		sh! "#{ZIG_AR} rcs dist/#{target}/libcoda_ffi.lib dist/#{target}/coda_ffi.o"
		sh! "#{ZIG_CXX} -target #{target} -shared -Wl,--whole-archive dist/#{target}/libcoda_ffi.lib -Wl,--no-whole-archive -lkernel32 -lws2_32 -o dist/#{target}/libcoda_ffi.dll"
		puts "→ dist/#{target}/libcoda_ffi.lib"
		puts "→ dist/#{target}/libcoda_ffi.dll"
	else
		sh! "#{ZIG_AR} rcs dist/#{target}/libcoda_ffi.a dist/#{target}/coda_ffi.o"
		sh! "#{ZIG_CXX} -target #{target} -shared -fPIC -o dist/#{target}/libcoda_ffi.so dist/#{target}/coda_ffi.o -lpthread"
		puts "→ dist/#{target}/libcoda_ffi.a"
		puts "→ dist/#{target}/libcoda_ffi.so"
	end

	rm_f "dist/#{target}/coda_ffi.o"
end

desc "Cross build for all targets"
task :"cross-all" => :generate do
	Rake::Task["clean"].invoke
	TARGETS.each do |target|
		sh! "rake cross[#{target}]"
	end
	puts "Cross build done. Artifacts are in dist/."
	Rake::Task["install"].invoke
end

desc "Install shared library"
task :install, [:libdir] do |_t, args|
	libdir = args[:libdir] || "/usr/local/lib"
	sh! "sudo install -d \"#{libdir}\""
	sh! "sudo install -m 755 dist/x86_64-linux-gnu/libcoda_ffi.so \"#{libdir}/\""
end

desc "Clean build artifacts"
task :clean do
	rm_rf %w[build dist libcoda_ffi.a coda_ffi.o]
end
