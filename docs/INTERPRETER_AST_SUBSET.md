# interpreter_ast Capability Matrix

This matrix describes the current behavior of `matx/apps/interpreter_ast.cc`.

## Positioning

- `interpreter_ast` is a standalone REPL/script interpreter.
- It parses source with a custom C++ lexer/parser and evaluates AST directly.
- It does not reuse the Python-FFI lowering path in `matx/python/ffi_system/parser.py`.

## Supported

- Expressions
  - literals: int/float/string/bool/None
  - arithmetic: `+ - * / %`
  - comparisons: `== != < <= > >=` (including chained comparisons)
  - boolean: `and/or/not` with short-circuit behavior
  - container literals: list/dict/set
  - indexing: `obj[idx]`
  - container methods:
    - list: `append`, `clear`
    - set: `add`, `discard`, `clear`
    - dict: `get`, `clear`
- Statements
  - assignment
  - indexed assignment
  - expression statement
  - `if/else`
  - `while`
  - `for i in range(...)` (1-3 args)
  - `return`

## Not Supported

- Python class system:
  - `class`, `self`, constructor/method lowering
  - class method type checks
  - class attribute typing rules
- Python function definitions (`def`)
- Ternary expression (`a if cond else b`)
- Full parity with Python-FFI subset tests in `matx/python/run_class_test.py`

## Recommendation

When discussing language support, treat `interpreter_ast` and Python-FFI parser as two separate subsets unless an explicit unification plan is implemented.
