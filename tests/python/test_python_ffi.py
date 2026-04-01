#!/usr/bin/env python3
"""
Tests for the Python Coda FFI bindings.
"""

import sys
import os
import tempfile

# Add the tests/ffi directory to the path so we can import coda_ffi
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..")))

from bindings.python.coda import (
    CodaDoc,
    CodaException,
    CodaParseError,
    get_abi_version,
    run_catalog_tests,
)


def test_basic_parsing():
    """Test basic document parsing."""
    print("  ✓ Basic parsing")
    
    text = "name example\ntype executable\n"
    
    with CodaDoc.parse(text) as doc:
        # Check we can access fields
        assert doc["name"].asString() == "example"
        assert doc["type"].asString() == "executable"


def test_nested_blocks():
    """Test nested block access."""
    print("  ✓ Nested blocks")
    
    text = """
compiler {
    debug false
    optimize true
}
"""
    
    with CodaDoc.parse(text) as doc:
        assert doc["compiler"]["debug"].asString() == "false"
        assert doc["compiler"]["optimize"].asString() == "true"


def test_arrays():
    """Test array iteration."""
    print("  ✓ Arrays")
    
    text = """
targets [
    x86_64-linux
    x86_64-windows
    aarch64-macos
]
"""
    
    with CodaDoc.parse(text) as doc:
        targets = list(doc["targets"].asArray())
        assert len(targets) == 3
        assert targets[0].asString() == "x86_64-linux"
        assert targets[1].asString() == "x86_64-windows"
        assert targets[2].asString() == "aarch64-macos"


def test_block_iteration():
    """Test block iteration."""
    print("  ✓ Block iteration")
    
    text = """
meta {
    author "Albert Einstein"
    description "a test project"
}
"""
    
    with CodaDoc.parse(text) as doc:
        entries = dict(doc["meta"].asBlock())
        assert len(entries) == 2
        assert entries["author"].asString() == "Albert Einstein"
        assert entries["description"].asString() == "a test project"


def test_keyed_table():
    """Test keyed table access."""
    print("  ✓ Keyed tables")
    
    text = """
deps [
    key link version
    plot github.com/zane-lang/plot 4.0.3
    http github.com/zane-lang/http 2.1.0
]
"""
    
    with CodaDoc.parse(text) as doc:
        assert doc["deps"]["plot"]["link"].asString() == "github.com/zane-lang/plot"
        assert doc["deps"]["plot"]["version"].asString() == "4.0.3"
        assert doc["deps"]["http"]["link"].asString() == "github.com/zane-lang/http"
        assert doc["deps"]["http"]["version"].asString() == "2.1.0"


def test_quoted_strings():
    """Test quoted string parsing."""
    print("  ✓ Quoted strings")
    
    text = '''
author "Albert Einstein"
message "Hello\\nWorld"
path "C:\\\\Users\\\\name"
'''
    
    with CodaDoc.parse(text) as doc:
        assert doc["author"].asString() == "Albert Einstein"
        assert doc["message"].asString() == "Hello\nWorld"
        assert doc["path"].asString() == "C:\\Users\\name"


def test_serialization():
    """Test document serialization."""
    print("  ✓ Serialization")
    
    text = "name example\ntype executable\n"
    
    with CodaDoc.parse(text) as doc:
        serialized = doc.serialize()
        assert "name" in serialized
        assert "example" in serialized
        assert "type" in serialized
        assert "executable" in serialized


def test_modification():
    """Test document modification."""
    print("  ✓ Modification")
    
    text = "name example\n"
    
    with CodaDoc.parse(text) as doc:
        # Modify existing field
        doc["name"] = "newproject"
        assert doc["name"].asString() == "newproject"
        
        # Add new field
        doc["type"] = "library"
        assert doc["type"].asString() == "library"


def test_file_operations():
    """Test file parsing and saving."""
    print("  ✓ File operations")
    
    text = "name test\nversion 1.0.0\n"
    
    # Create a temporary file
    with tempfile.NamedTemporaryFile(mode='w', suffix='.coda', delete=False) as f:
        f.write(text)
        temp_path = f.name
    
    try:
        # Parse from file
        with CodaDoc.parse_file(temp_path) as doc:
            assert doc["name"].asString() == "test"
            assert doc["version"].asString() == "1.0.0"
            
            # Modify and save
            doc["name"] = "modified"
            doc.save(temp_path)
        
        # Parse again to verify save worked
        with CodaDoc.parse_file(temp_path) as doc:
            assert doc["name"].asString() == "modified"
    finally:
        os.unlink(temp_path)


def test_error_handling():
    """Test error handling for invalid input."""
    print("  ✓ Error handling")
    
    # Test parse error
    invalid_text = "name example\ntype {\n"  # Unclosed block
    
    try:
        doc = CodaDoc.parse(invalid_text)
        assert False, "Should have raised CodaParseError"
    except CodaParseError as e:
        assert e.line > 0
        assert len(str(e)) > 0
    
    # Test key error
    text = "name example\n"
    with CodaDoc.parse(text) as doc:
        try:
            doc["nonexistent"]
            assert False, "Should have raised KeyError"
        except KeyError:
            pass


def test_comments():
    """Test comment preservation."""
    print("  ✓ Comments")
    
    text = """
# Project configuration
name example

compiler {
    # Enable optimizations
    optimize true
}
"""
    
    with CodaDoc.parse(text) as doc:
        # Comments are preserved during parsing
        serialized = doc.serialize()
        assert "Project configuration" in serialized
        assert "Enable optimizations" in serialized


def test_complex_document():
    """Test parsing a complex document with all features."""
    print("  ✓ Complex document")
    
    text = """
name example
type executable

deps [
    key link version
    plot github.com/zane-lang/plot 4.0.3
    http github.com/zane-lang/http 2.1.0
]

compiler {
    debug false
    optimize true
    targets [
        x86_64-linux
        x86_64-windows
        aarch64-macos
    ]
}

meta {
    author "Albert Einstein"
    description "a test project"
}
"""
    
    with CodaDoc.parse(text) as doc:
        # Top-level fields
        assert doc["name"].asString() == "example"
        assert doc["type"].asString() == "executable"
        
        # Keyed table
        assert doc["deps"]["plot"]["link"].asString() == "github.com/zane-lang/plot"
        
        # Nested block
        assert doc["compiler"]["debug"].asString() == "false"
        
        # Nested array
        targets = list(doc["compiler"]["targets"].asArray())
        assert len(targets) == 3
        
        # Block iteration
        meta = dict(doc["meta"].asBlock())
        assert len(meta) == 2
        
        # Serialization round-trip
        serialized = doc.serialize()
        
    # Parse the serialized version
    with CodaDoc.parse(serialized) as doc2:
        assert doc2["name"].asString() == "example"
        assert doc2["deps"]["plot"]["link"].asString() == "github.com/zane-lang/plot"


def test_context_manager():
    """Test context manager properly frees resources."""
    print("  ✓ Context manager")
    
    text = "name example\n"
    
    doc = CodaDoc.parse(text)
    assert doc._ptr is not None
    
    with doc:
        assert doc["name"].asString() == "example"
    
    # After exiting context, should be freed
    assert doc._ptr is None
    
    # Accessing after free should raise error
    try:
        doc["name"]
        assert False, "Should have raised CodaException"
    except CodaException:
        pass


def test_abi_version():
    """Test ABI version function."""
    print("  ✓ ABI version")
    
    version = get_abi_version()
    assert version > 0
    assert isinstance(version, int)


def test_get_with_default():
    """Test get method with default value."""
    print("  ✓ Get with default")
    
    text = "name example\n"
    
    with CodaDoc.parse(text) as doc:
        root = doc.root()
        
        # Existing key
        assert root.get("name") == "example"
        
        # Non-existent key with default
        assert root.get("nonexistent", "default") == "default"
        
        # Non-existent key without default
        assert root.get("nonexistent") is None


def run_tests():
    """Run catalog-driven tests."""
    print("\nCoda Python FFI Tests")
    print("=====================\n")
    catalog_path = os.path.join(os.path.dirname(__file__), "..", "catalog", "catalog.coda")
    run_catalog_tests(catalog_path)


if __name__ == "__main__":
    run_tests()
