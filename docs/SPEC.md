# Coda Language Specification

This document describes the Coda configuration language syntax in full.

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

Unicode escapes (`\uXXXX`) are **not** supported.

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

## Reserved word: `key`

`key` is a reserved keyword used in keyed table headers.

* Bare `key` is not a normal identifier token.
* If you want the literal key/value `key`, write `"key"`.

```coda
"key" value
value "key"
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

### Header comments in tables

You can place comments directly before a plain or keyed table header. Since there is no node _for_ the header row itself, these comments are stored separately as a **header comment** on the table container.

Plain table (array with a multi-column header row):

```coda
points [
	# units: meters
	x y z
	1 2 3
]
```

Keyed table (header row begins with `key`):

```coda
deps [
	# optional deps
	key link version
	plot github.com/zane-lang/plot 4.0.3
]
```

Multi-line header comments are preserved as a `\n`-joined string (one line per `# ...` line).

---

## Blocks `{}`

A block is a set of named children. Every child has an explicit key. Nesting is freely allowed — because every child is anchored by name, the hierarchy is always unambiguous.

```coda
compiler {
	debug false
	optimize true
}
```

Blocks can contain arrays, and arrays can contain blocks (with restrictions covered in the Nesting rules section):

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

Rows must always contain as many elements as the header defines. A mismatch is a parse error.

### Keyed table

A `key` keyword at the start of the first line turns the array into a map. The first column of each row becomes the lookup key. Nesting is not allowed inside rows.

```coda
deps [
	key link version
	plot github.com/zane-lang/plot 4.0.3
	http github.com/zane-lang/http 2.1.0
]
```

Row values can be accessed by key name: `deps["plot"]["link"]`.

Rows must always contain as many elements as the header defines. Non-const row access on a missing key inserts an empty row; const access throws.

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

## Access semantics and errors

- Parsing never inserts defaults.
- Const lookup on a missing key throws.
- Non-const `operator[]` on a block inserts an empty string node so you can assign to it later.
- For keyed tables, non-const row access inserts an empty row; const access throws.
- Type errors throw: string-indexing a scalar, int-indexing a block, or calling `asArray`/`asBlock`/`asTable` on the wrong kind.
- Array index out of range throws.
- Inline blocks are illegal: content must start on a new line after `{`.
- `key` is reserved for keyed table headers; `"key"` is allowed as a normal key/value.
- Comments attach to the following node, including array elements and table rows.
- Table headers can carry a `headerComment` (comments directly before the header row).
- Ordering: `order()` sorts scalars before containers, then alphabetically; weighted order puts higher weights earlier.
- Keyed table row iteration preserves insertion order.
- Parse → serialize → parse → serialize is stable for core constructs (including quoting rules).
