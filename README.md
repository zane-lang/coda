# Coda

```coda
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
```

A compact configuration format designed to be easily read- and writeable. The name comes from music — a coda is the concluding passage that ties a composition together. A `.coda` file is the single, definitive statement of how a project is configured.

Coda aims to be as simple as possible while supporting full data structures. The above file in JSON:

```json
{
    "name": "example",
    "type": "executable",
    "deps": {
        "plot": { "link": "github.com/zane-lang/plot", "version": "4.0.3" },
        "http": { "link": "github.com/zane-lang/http", "version": "2.1.0" }
    },
    "compiler": {
        "debug": "false",
        "optimize": "true",
        "targets": [
            "x86_64-linux",
            "x86_64-windows",
            "aarch64-macos"
        ]
    },
    "meta": {
        "author": "Albert Einstein",
        "description": "a test project"
    }
}
```

---

## Overview

Coda is whitespace-sensitive. Every leaf value is a string — there are no booleans, numbers, or nulls. Type interpretation is left to the consumer. Quotes are optional unless a value contains spaces or special characters.

```coda
name myproject
type executable
author "Albert Einstein"
```

Coda has three structural constructs: **blocks** `{}`, **arrays** `[]`, and **tables**. Tables are not a separate syntax — they emerge from a header row inside an array.

---

## Strings and Escapes

Unquoted strings can contain any characters except whitespace and syntax characters (`{}[]"#`).

```coda
url https://example.com/path?query=1
email user@domain.com
```

Quoted strings support escape sequences:

| Escape | Character |
|--------|-----------|
| `\n` | newline |
| `\t` | tab |
| `\r` | carriage return |
| `\"` | double quote |
| `\\` | backslash |

```coda
message "hello\nworld"
path "C:\\Users\\name"
quote "He said \"hello\""
```

Keys can also be quoted strings:

```coda
"key with spaces" value
"weird-key!" "weird value"
```

---

## Comments

Comments start with `#` and extend to the end of the line. They are preserved and attached to the following node.

```coda
# Project configuration
name myproject

compiler {
    # Enable optimizations for release
    optimize true
    debug false
}
```

Comments may appear before array elements in bare lists, and before rows
in keyed tables. A comment directly before a plain table header is an error —
comments only attach to fields below, meaning there is no node to attach it to. Place the comment before the array instead.

```coda
# Legal — comment is on the array itself
# points data
points [
    x y z
    1 2 3
]

# Illegal — nothing to attach the comment to
points [
    # this will error
    x y z
    1 2 3
]
```

The same restriction applies to keyed tables:

```coda
# Legal
deps [
    key link version
    plot github.com/zane-lang/plot 4.0.3
]

# Illegal
deps [
    # this will also error
    key link version
    plot github.com/zane-lang/plot 4.0.3
]
```

---

## Blocks `{}`

A block is a set of named children. Every child has an explicit key. Nesting is freely allowed — because every child is anchored by name, the hierarchy is always unambiguous.

```coda
compiler {
    debug false
    optimize true
}
```

Blocks can contain arrays, and arrays can contain blocks (with restrictions covered below):

```coda
project {
    name myproject
    targets [
        x86_64-linux
        x86_64-windows
        aarch64-macos
    ]
}
```

Content must begin on a new line after `{`:

```coda
# Legal
compiler {
    debug false
}

# Illegal
compiler { debug false }
```

---

## Arrays `[]`

Arrays have three modes, inferred from their content.

### Bare list

One value per line. Nesting is allowed — elements can be blocks or other arrays.

```coda
targets [
    x86_64-linux
    x86_64-windows
    aarch64-macos
]
```

### Plain table

When the first line has more than one token, it is treated as a header row. Subsequent lines are data rows. Nesting is not allowed inside rows.

```coda
points [
    x y z
    1 2 3
    4 5 6
]
```

### Keyed table

A `key` keyword at the start of the first line turns the array into a map. The first column of each row becomes the lookup key. Nesting is not allowed inside rows.

```coda
deps [
    key link version
    plot github.com/zane-lang/plot 4.0.3
    http github.com/zane-lang/http 2.1.0
]
```

You can access the plot link via `coda["deps"]["plot"]["link"]`.

---

## Nesting rules

**Nesting is allowed when every child has an explicit name. Nesting is forbidden when structure is implied by column position.**

| Context | Nesting allowed |
|---|---|
| Block `{}` | ✅ |
| Bare list `[]` | ✅ |
| Plain table `[]` | ❌ |
| Keyed table `[ key ... ]` | ❌ |

In a plain or keyed table, a cell's meaning comes entirely from its column position. A nested block breaks the visual consistency. The format makes this a hard error rather than a convention.

**Legal:**
```coda
project {
    name myproject
    compiler {
        debug false
    }
}
```

**Illegal:**
```coda
deps [
    key name config
    http myhttp {
        timeout 30
    }
]
```

---

## C++ Library

This repo contains a header-only C++ library for parsing, querying, and serializing Coda files.

### Usage

```cpp
#include "include/coda.hpp"

int main() {
    Coda coda("project.coda");

    // Access values
    std::string name = coda["name"].asString();
    std::string debug = coda["compiler"]["debug"].asString();
    std::string plotLink = coda["deps"]["plot"]["link"].asString();

    // Iterate arrays
    for (const auto& target : coda["compiler"]["targets"].asArray()) {
        std::cout << target.asString() << "\n";
    }

    // Iterate blocks
    for (const auto& [key, value] : coda["compiler"].asBlock()) {
        std::cout << key << ": " << value.asString() << "\n";
    }

    // Modify and save
    coda["name"] = "newproject";
    coda.save("project.coda");

    return 0;
}
```

### Sorting

Fields can be sorted for consistent output:

```cpp
// Alphabetical, scalars before containers
coda.order();

// Custom weight function (higher weight = earlier)
coda.order([](const std::string& key) -> float {
    if (key == "name") return 100;
    if (key == "type") return 90;
    return 0;
});
```
