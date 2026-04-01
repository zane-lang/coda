# Coda

```coda
# Release notes
title "Coda 1.2"
date 2026-04-01
"release manager" "Terry Davis"

changes [
	type desc
	feat "HolyC-style map ordering"
	feat "Script runner: once-a-boot"
	fix "comment attachment in tables"
]

contributors [
	key role handle
	terry maintainer td
]

notes {
	summary "Blessed boot and brisk builds"
	impact "No breaking changes"
}
```

A compact configuration format designed to be easily read- and writeable. The name comes from music — a coda is the concluding passage that ties a composition together. A `.coda` file is the single, definitive statement of how a project is configured.

The above file in JSON:

```json
{
	"title": "Coda 1.2",
	"date": "2026-04-01",
	"release manager": "Terry Davis",
	"changes": [
		{ "type": "feat", "desc": "HolyC-style map ordering" },
		{ "type": "feat", "desc": "Script runner: once-a-boot" },
		{ "type": "fix", "desc": "comment attachment in tables" }
	],
	"contributors": {
		"terry": { "role": "maintainer", "handle": "td" }
	},
	"notes": {
		"summary": "Blessed boot and brisk builds",
		"impact": "No breaking changes"
	}
}
```

---

## Overview

- Whitespace-sensitive, line-oriented.
- Every leaf value is a string; interpretation is left to the consumer.
- Quotes are optional unless a value contains whitespace or syntax characters (`{}[]"#`).

```coda
name myproject
type executable
author "Albert Einstein"
```

Coda has three structural constructs: **blocks** `{}`, **arrays** `[]`, and **tables** (inferred from array headers).

---

## Strings and escapes

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

Comments start with `#` and extend to the end of the line. They are preserved and attach to the following node.

```coda
# Project configuration
name myproject

compiler {
	# Enable optimizations for release
	optimize true
	debug false
}
```

Comments may appear before array elements in bare lists, and before rows in keyed tables. A comment directly before a plain or keyed table header is an error — there is no node to attach it to. Place the comment on the array instead.

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

Rows must always contain as many elements as the header defines in both plain and keyed tables.

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

## Access semantics

- Parsing never inserts defaults.
- Const lookup on a missing key throws.
- Non-const `operator[]` inserts an empty string node so you can assign to it later.
- For keyed tables, non-const row access inserts an empty row; const access throws.

---

## Errors & semantics

- Type errors throw: string-indexing a scalar, int-indexing a block, or calling `asArray`/`asBlock`/`asTable` on the wrong kind.
- Array index out of range throws.
- Inline blocks are illegal: content must start on a new line after `{`.
- `key` is reserved in table headers; `"key"` is allowed as a normal key.
- Comments attach to the following node, including array elements and table rows.
- Ordering: `order()` sorts scalars before containers, then alphabetically; weighted order puts higher weights earlier.
- Keyed table row iteration preserves insertion order.
- Parse → serialize → parse → serialize is stable for core constructs.

---

## C++ library

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
