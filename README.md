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

A configuration format designed to be written by tooling and read by humans. The name comes from music — a coda is the concluding passage that ties a composition together. A `.coda` file is the single, definitive statement of how a project is configured.

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

Coda is whitespace-sensitive. Every leaf value is a string — there are no booleans, numbers, or nulls. Type interpretation is left to the consumer. Quotes are optional unless a value contains spaces.

```coda
name myproject
type executable
author "Alber Einstein"
```

Coda has three structural constructs: **blocks** `{}`, **arrays** `[]`, and **tables**. Tables are not a separate syntax — they emerge from a header row inside an array.

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

This repo contains a C++ library for parsing, querying, and serializing Coda files.
