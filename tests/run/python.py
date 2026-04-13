import os
import sys

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..")))

from tests.harness.test_harness import run_catalog_tests

catalog_path = os.path.join(os.path.dirname(__file__), "..", "catalog", "catalog.coda")
run_catalog_tests(os.path.abspath(catalog_path))
