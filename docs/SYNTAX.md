# Syntax Support Matrix

This document keeps the Python-FFI parser and `interpreter_ast` REPL aligned.

## Summary
- **Python-FFI**: parses a single `def` function from Python source into matx AST, then rewrites to C++.
- **interpreter_ast**: parses REPL/script text into matx AST and evaluates directly.

## Semantics Note
This project currently implements a **Python-like subset**, not full Python semantics:
- `/` on integers performs integer division (C++ behavior).
- `and/or` returns `bool` (`0/1`) instead of returning operand values.

These choices keep the runtime simple and should be treated as a language definition, not Python compatibility.

## Supported (Both)
- Integer literals and variables
- Float literals
- None literal
- String literals
- Arithmetic: `+ - * / %` (string `+` concatenation)
- Comparisons: `== != < <= > >=` (chain comparisons supported)
- Boolean ops: `and`, `or`, `not`, `True`, `False`
- Control flow: `if/else`, `while`
- `for i in range(...)` with 1–3 args (lowered to `while`)
- Container literals: list `[a, b]`, dict `{k: v}`, set `{a, b}`
- Indexing: `x[i]`
- Indexed assignment: `x[i] = v`
- Container method calls: `list.append(x)`, `list.clear()`, `set.add(x)`, `set.discard(x)`, `set.clear()`, `dict.get(k, default)`, `dict.clear()`
- `return expr`

## Type Rules (Python-FFI)
- `bool` literals map to `bool`.
- `int` literals map to `int64`.
- `float` literals map to `float64`.
- `str`, containers, `None` map to `handle`.
- Numeric binary ops (`+ - * / %`) promote to `float64` if any operand is float, otherwise `int64`.
- Comparisons and boolean ops yield `bool`.

## Python-FFI Only
- Function definition via `def` with `int` annotations
- Only one top-level function per module

## interpreter_ast Only
- REPL execution with expression results printed
- Script mode (execute a `.mx` file without `def`)

## Not Supported Yet (Both)
- Floats
- Function calls (except `range(...)`)
- Method calls on containers (e.g. `list.append`, `dict.get`)
- Slices (e.g. `x[1:3]`)
- Classes and user-defined types
