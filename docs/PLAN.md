# FFI System Plan

## Current Snapshot
**C++ (`matx/src/`)**
- AST/IR supports core expr/stmt nodes (e.g., `PrimAdd`, `PrimMul`, `PrimNot`, `PrimCall`, `ClassGetItem`, `List/Dict/SetLiteral`, container get/set/method call).
- Rewriter (`rewriter.h/.cc`) can emit C++ code for supported AST and generates C API wrappers.
- Ops are registered in `ops.cc` but only a small subset is exposed from Python.

**Apps (`matx/apps/test.cc`)**
- Exercises AST construction, printer, rewriter, container features, class compilation path.

**Python (`matx/python/ffi_system/`)**
- Modular FFI: `config`, `c_api`, `runtime`, `parser`, `compiler`, `module`.
- `parser` is minimal: function-only, `int` annotations, `+`, `=` and `return`.
- `new_ffi.py` is a thin entrypoint using the modular system.

**case_ext.cc**
- Python extension for packed functions and object bridging.
- Functional but thin; no rich type conversions or error diagnostics beyond basics.

## Problems to Solve (Agreed Priority)
1. **Syntax support is too small.** Python front-end can only express trivial arithmetic.
2. **C++ AST features are under-used.** Existing nodes in C++ are not reachable from Python.
3. **Limited diagnostics.** Errors during parse/compile/load are hard to interpret.

## Next-Step Strategy (C++ First)
### Phase 1: Expand Core AST & Rewriter Coverage (C++)
- Add missing expression/statement nodes in C++ where needed:
  - Arithmetic: `- / %`
  - Comparisons: `== != < <= > >=`
  - Boolean ops: `and/or/not`
  - Control flow: `If`, `While` (and `For` via lowering)
  - Function calls & globals: `PrimCall` usage from Python
- Ensure `printer` and `rewriter` cover all new nodes.
- Add tests in `matx/apps/test.cc` for each new node.

### Phase 2: Expose New Nodes to Python FFI
- Extend `ffi_system/runtime.py` to bind:
  - New AST node constructors
  - New ops and literal helpers
- Keep Python API minimal but complete.

### Phase 3: Expand Python Parser
- Map Python AST → C++ AST for:
  - Binary operators (`+ - * / %`)
  - Comparisons (`== != < <= > >=`)
  - Boolean ops (`and/or`)
  - `if` statements (and ternary if useful)
  - Basic `while`/`for` (range lowered to while)
- Add parser tests (a few targeted examples).

### Phase 4: Improve Diagnostics
- Standardize error messages:
  - Stage: `parse`, `rewrite`, `compile`, `load`, `invoke`
  - Location: function name, line, node type
- Optionally expose `GetError()` from `case_ext` for richer Python exceptions.

## Proposed Milestone Order
1. C++ AST nodes + rewriter + tests for comparisons and if/else. (in progress)
2. Python bindings to these nodes. (in progress)
3. Python parser support for comparisons and if/else. (in progress)
4. Diagnostics improvements.

## Open Questions
- Do we want a typed IR (explicit type inference) or continue with annotation-driven types?
- Should control-flow be lowered in Python (e.g., to SSA) or modeled directly in C++ AST?
