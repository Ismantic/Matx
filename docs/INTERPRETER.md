# matx Interpreter

`apps/interpreter_ast.cc` provides a native REPL that parses into `mc::runtime` AST nodes and evaluates them directly.

Important: this REPL uses its own hand-written parser/evaluator pipeline and is a standalone runtime subset. It is **not** automatically aligned with the Python-FFI parser subset (`matx/python/ffi_system/parser.py`).

## Build
From the repository root:

```bash
cmake -S matx -B matx/build
cmake --build matx/build
```

## Run
Interactive REPL:

```bash
./matx/build/apps/interpreter_ast
```

Script mode (execute a file):

```bash
./matx/build/apps/interpreter_ast path/to/script.mx
```

## Supported Syntax
- integer and string literals, variables
- arithmetic: `+ - * / %` (string `+` for concatenation)
- comparisons: `== != < <= > >=` (chained comparisons supported)
- boolean: `and`, `or`, `not`, `True`, `False`
- assignment: `x = expr`, `arr[i] = expr`, `dict[key] = expr`
- control flow: `if/else`, `while`
- `for i in range(...)` with 1–3 args
- container literals: list `[a, b]`, dict `{k: v}`, set `{a, b}`
- indexing: `arr[i]`, `dict[key]`
- `return expr` (ends current REPL block or script)

## Not Supported In `interpreter_ast` (Current)
- `class` syntax (`class`, `self`, constructor/method lowering)
- `def` function definitions
- ternary expression (`a if cond else b`)
- Python-style annotation/type-check pipeline from Python-FFI parser

For a detailed capability matrix, see `matx/docs/INTERPRETER_AST_SUBSET.md`.

## Example REPL Session

```text
matx> x = 2
matx> y = 0
matx> for i in range(0, 5):
...>     y = y + i
...>
matx> y
10
matx> if y > x and y < 20:
...>     y
...> else:
...>     0
...>
10
```

Note: `apps/interpreter.cc` is a standalone reference and does not reuse the matx AST/runtime.
