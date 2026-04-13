import os
import sys

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

from bindings.python.coda import *

data = Doc.parse("")
data.root().insert("hello", String(data, "hello"))
f = open("tests/catalog/example.coda")
print(f.read())
