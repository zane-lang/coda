# examples/python.py — Coda Python API tour
#
# Run from the repository root:
#   python examples/python.py
#
# To use the installed package instead, replace the sys.path block below with:
#   import coda
#   from coda import Doc, Block, Array, Table, KeyedTable, Row, ParseError

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from bindings.python.coda import Doc, Block, Array, Table, KeyedTable, Row, ParseError

# ─── Sample document ─────────────────────────────────────────────────────────

SOURCE = """\
# project configuration
name myproject
version 1.0.0

compiler {
\tdebug false
\toptimize true
}

targets [
\tx86_64-linux
\tx86_64-windows
\taarch64-macos
]

releases [
\tversion date
\t1.0.0   2025-01-01
\t1.1.0   2025-06-15
]

# dependency table
deps [
\t# optional
\tkey link version
\tplot github.com/zane-lang/plot 4.0.3
\thttp github.com/zane-lang/http 2.1.0
]
"""

# ─── 1. Parse & read ─────────────────────────────────────────────────────────

def demo_read():
    with Doc.parse(SOURCE, filename="example.coda") as doc:
        root = doc.root()

        # Scalar strings
        print("name:   ", root["name"].as_string().value)
        print("version:", root["version"].as_string().value)

        # Membership test
        print("has author:", "author" in root)

        # Block
        compiler = root["compiler"].as_block()
        print("compiler:")
        for key, node in compiler:
            print(f"  {key} = {node.as_string().value}")

        # Array
        targets = root["targets"].as_array()
        print(f"targets ({len(targets)}):")
        for item in targets:
            print(f"  {item.as_string().value}")

        # Plain table
        releases = root["releases"].as_table()
        print("releases:")
        for row in releases:
            print(f"  {row['version']}  {row['date']}")

        # Keyed table
        deps = root["deps"].as_keyed_table()
        print("deps comment:       ", root["deps"].comment)
        print("deps header comment:", deps.header_comment)
        print("plot link:", deps["plot"]["link"])
        print("all deps:")
        for key, row in deps:
            print(f"  {key} → {row['link']} @ {row['version']}")

# ─── 2. Build a document from scratch ────────────────────────────────────────

def demo_build():
    doc = Doc.new()
    root = doc.root()

    # Scalars
    root["name"]    = "myproject"
    root["version"] = "1.0.0"

    # Block — insert into doc first, then set fields
    compiler = Block()
    root["compiler"] = compiler   # materializes the block
    compiler["debug"]    = "false"
    compiler["optimize"] = "true"

    # Array
    root["targets"] = Array()
    targets = root["targets"].as_array()
    targets.header_comment = "supported build targets"
    targets.append("x86_64-linux").append("x86_64-windows").append("aarch64-macos")

    # Plain table — columns are set before appending rows
    root["releases"] = Table(["version", "date"])
    releases = root["releases"].as_table()
    r1 = Row()
    r1["version"] = "1.0.0"
    r1["date"]    = "2025-01-01"
    releases.append(r1)
    r2 = Row()
    r2["version"] = "1.1.0"
    r2["date"]    = "2025-06-15"
    releases.append(r2)

    # Keyed table
    root["deps"] = KeyedTable(["link", "version"])
    root["deps"].comment = "dependency table"
    deps = root["deps"].as_keyed_table()
    deps.header_comment = "optional"
    plot = Row()
    plot["link"]    = "github.com/zane-lang/plot"
    plot["version"] = "4.0.3"
    deps.insert("plot", plot)
    http = Row()
    http["link"]    = "github.com/zane-lang/http"
    http["version"] = "2.1.0"
    deps.insert("http", http)

    # Sort: name and version first, then everything else alphabetically
    doc.order_weighted([("name", 100), ("version", 90)])

    print(doc.serialize(indent="  "))

# ─── 3. Modify an existing document ──────────────────────────────────────────

def demo_modify():
    with Doc.parse(SOURCE) as doc:
        root = doc.root()

        # Change a scalar in-place
        root["version"].as_string().value = "2.0.0"

        # Add a new key
        root["author"] = "zane"

        # Append to an array
        root["targets"].as_array().append("wasm32-wasi")

        # Update a keyed table row
        root["deps"].as_keyed_table()["plot"]["version"] = "5.0.0"

        print(doc.serialize())

# ─── 4. Error handling ───────────────────────────────────────────────────────

def demo_errors():
    # Inline blocks are not allowed
    try:
        Doc.parse("compiler { debug false }", filename="bad.coda")
    except ParseError as e:
        print(f"parse error: {e}")
        print(f"  line={e.line} col={e.col} offset={e.offset}")

    # Type mismatch at runtime
    try:
        with Doc.parse(SOURCE) as doc:
            doc.root()["name"].as_block()  # "name" is a string, not a block
    except TypeError as e:
        print(f"type error: {e}")

# ─────────────────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    print("=== Reading ===")
    demo_read()

    print("\n=== Building ===")
    demo_build()

    print("\n=== Modifying ===")
    demo_modify()

    print("\n=== Errors ===")
    demo_errors()

