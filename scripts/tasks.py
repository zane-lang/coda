#!/usr/bin/env python3

from __future__ import annotations

import argparse
import shlex
import shutil
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
SCRIPTS = ROOT / "scripts"
BUILD_DIR = ROOT / "build"
DIST_DIR = ROOT / "dist"

ZIG_CXX = SCRIPTS / "zig-cxx"
ZIG_AR = SCRIPTS / "zig-ar"

TEST_FLAGS = ["-O0", "-g", "-std=c++20"]
FLAGS = ["-O3", "-std=c++20"]
FLAGS_CROSS = ["-O3", "-std=c++20"]
INC = ["-Iinclude"]

TARGETS = [
	"x86_64-linux-gnu",
	"x86_64-linux-musl",
	"aarch64-linux-gnu",
	"aarch64-linux-musl",
	"x86_64-windows-gnu",
	"aarch64-windows-gnu",
]

ARTIFACTS = [
	BUILD_DIR,
	DIST_DIR,
	ROOT / "libcoda_ffi.a",
	ROOT / "coda_ffi.o",
]


def run_cmd(*command: str, cwd: Path = ROOT) -> None:
	print(shlex.join(command))
	subprocess.run(command, check=True, cwd=cwd)


def ensure_dir(path: Path) -> None:
	path.mkdir(parents=True, exist_ok=True)


def generate() -> None:
	run_cmd("quom", "src/coda.hpp", "include/coda.hpp")


def build() -> None:
	ensure_dir(BUILD_DIR)
	run_cmd(
		str(ZIG_CXX),
		*FLAGS,
		*INC,
		"-fPIC",
		"-shared",
		"-o",
		"build/libcoda_ffi.so",
		"ffi/coda_ffi.cpp",
	)


def test_cpp() -> None:
	ensure_dir(BUILD_DIR)
	run_cmd(
		str(ZIG_CXX),
		*TEST_FLAGS,
		"-I.",
		"tests/cpp/test_main.cpp",
		"-o",
		"build/tests",
	)
	run_cmd("./build/tests")


def test_c_ffi() -> None:
	ensure_dir(BUILD_DIR)
	run_cmd(
		str(ZIG_CXX),
		*TEST_FLAGS,
		"-I.",
		"-c",
		"-fPIC",
		"ffi/coda_ffi.cpp",
		"-o",
		"build/coda_ffi.o",
	)
	run_cmd(str(ZIG_AR), "rcs", "build/libcoda_ffi_native.a", "build/coda_ffi.o")
	run_cmd(
		str(ZIG_CXX),
		*TEST_FLAGS,
		"-I.",
		"tests/cpp/test_c_ffi.cpp",
		"build/libcoda_ffi_native.a",
		"-o",
		"build/test_c_ffi",
	)
	run_cmd("./build/test_c_ffi")


def test_py_ffi() -> None:
	run_cmd("python3", "tests/python/test_python_ffi.py")


def run_sample() -> None:
	ensure_dir(BUILD_DIR)
	run_cmd(
		str(ZIG_CXX),
		*TEST_FLAGS,
		"-I.",
		"examples/cpp.cpp",
		"-o",
		"build/run",
	)
	run_cmd("./build/run")


def cross(target: str) -> None:
	target_dir = DIST_DIR / target
	ensure_dir(target_dir)

	run_cmd(
		str(ZIG_CXX),
		*FLAGS_CROSS,
		*INC,
		"-target",
		target,
		"-fPIC",
		"-c",
		"-o",
		f"dist/{target}/coda_ffi.o",
		"ffi/coda_ffi.cpp",
	)

	if "windows" in target:
		run_cmd(
			str(ZIG_AR),
			"rcs",
			f"dist/{target}/libcoda_ffi.lib",
			f"dist/{target}/coda_ffi.o",
		)
		run_cmd(
			str(ZIG_CXX),
			"-target",
			target,
			"-shared",
			"-Wl,--whole-archive",
			f"dist/{target}/libcoda_ffi.lib",
			"-Wl,--no-whole-archive",
			"-lkernel32",
			"-lws2_32",
			"-o",
			f"dist/{target}/libcoda_ffi.dll",
		)
		print(f"→ dist/{target}/libcoda_ffi.lib")
		print(f"→ dist/{target}/libcoda_ffi.dll")
	else:
		run_cmd(
			str(ZIG_AR),
			"rcs",
			f"dist/{target}/libcoda_ffi.a",
			f"dist/{target}/coda_ffi.o",
		)
		run_cmd(
			str(ZIG_CXX),
			"-target",
			target,
			"-shared",
			"-fPIC",
			"-o",
			f"dist/{target}/libcoda_ffi.so",
			f"dist/{target}/coda_ffi.o",
			"-lpthread",
		)
		print(f"→ dist/{target}/libcoda_ffi.a")
		print(f"→ dist/{target}/libcoda_ffi.so")

	(target_dir / "coda_ffi.o").unlink(missing_ok=True)


def cross_all() -> None:
	clean()
	for target in TARGETS:
		cross(target)
	print("Cross build done. Artifacts are in dist/.")


def install(libdir: str) -> None:
	artifact = DIST_DIR / "x86_64-linux-gnu" / "libcoda_ffi.so"
	if not artifact.exists():
		raise FileNotFoundError("dist/x86_64-linux-gnu/libcoda_ffi.so not found; run `just cross-all` first")

	run_cmd("sudo", "install", "-d", libdir)
	run_cmd(
		"sudo",
		"install",
		"-m",
		"755",
		str(artifact),
		f"{libdir}/",
	)


def clean() -> None:
	for artifact in ARTIFACTS:
		if artifact.is_dir():
			shutil.rmtree(artifact, ignore_errors=True)
		else:
			artifact.unlink(missing_ok=True)


def test() -> None:
	test_cpp()
	test_c_ffi()
	test_py_ffi()


def main() -> None:
	parser = argparse.ArgumentParser()
	subparsers = parser.add_subparsers(dest="command", required=True)

	subparsers.add_parser("generate")
	subparsers.add_parser("build")
	subparsers.add_parser("test-cpp")
	subparsers.add_parser("test-c-ffi")
	subparsers.add_parser("test-py-ffi")
	subparsers.add_parser("test")
	subparsers.add_parser("run")
	subparsers.add_parser("cross-all")
	subparsers.add_parser("clean")

	cross_parser = subparsers.add_parser("cross")
	cross_parser.add_argument("target")

	install_parser = subparsers.add_parser("install")
	install_parser.add_argument("libdir", nargs="?", default="/usr/local/lib")

	args = parser.parse_args()

	if args.command == "generate":
		generate()
	elif args.command == "build":
		build()
	elif args.command == "test-cpp":
		test_cpp()
	elif args.command == "test-c-ffi":
		test_c_ffi()
	elif args.command == "test-py-ffi":
		test_py_ffi()
	elif args.command == "test":
		test()
	elif args.command == "run":
		run_sample()
	elif args.command == "cross":
		cross(args.target)
	elif args.command == "cross-all":
		cross_all()
	elif args.command == "install":
		install(args.libdir)
	elif args.command == "clean":
		clean()


if __name__ == "__main__":
	main()
