import os
import sys

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..")))

from bindings.python.coda import Coda, CodaDoc

data = CodaDoc.parse("")
data.root().as_block()["hello"] = Coda(data, 3)
f = open("tests/catalog/example.coda")
print(f.read())
