# Matx Class/Function/Expression Subset Release Checklist

Date: 2026-02-07

## 1. Feature Coverage

### 1.1 Expression

- Supported
  - Arithmetic: `+ - * / %`
  - Compare: `== != < <= > >=`
  - BoolOp with short-circuit lowering: `and/or`
  - Unary: `not`, unary `+/-`
  - Ternary: `a if cond else b`
  - Subscript get/set
  - Container literals: `list/dict/set`
  - Class method call in expression positions (including nested args)
- Not in current subset
  - Slice
  - General builtin/global function calls (e.g. `len`)

### 1.2 Function

- Supported
  - Typed args and return annotations
  - Local assignment and type compatibility checks
  - `if/while/for(range)`
  - `return`, `pass`
- Not in current subset
  - kwargs/default args/varargs
  - closures/nested functions as full feature

### 1.3 Class

- Supported
  - Constructor lowering: `x = Class(...)`
  - Instance attribute read/write
  - Instance method call in stmt/expr/control-flow
  - Method arg/return type checks (annotation-based)
  - Attribute assignment type checks (from `__init__` inferred fields)
- Not in current subset
  - Inheritance
  - `@staticmethod` / `@classmethod`
  - `super()`
  - Full Python object model behavior

Reference: `matx/python/CLASS_SUBSET.md`

## 2. Risks And Gaps

- Error location is message-level, not precise source span diagnostics.
- Class attribute type inference relies on patterns in `__init__`; dynamic/late-added fields are intentionally outside strict guarantees.
- Builtin/global call coverage is limited; users must stay within the subset.

## 3. Test Matrix

- Main positive/negative class matrix:
  - `matx/python/run_class_test.py`
  - Covers positive execution for ctor/method/if/while/for/ifexp/subscript/literal/nested-call.
  - Covers negative compile checks for:
    - method arg type mismatch
    - method return type mismatch
    - attribute assignment type mismatch
    - wrong method arity
    - unknown method
    - method missing `self`
- Additional regression:
  - `matx/python/run_if_test.py`

## 4. Release Decision

Decision: **Conditional Go (有条件封板)**

Conditions to treat as release-ready for this phase:

1. Publicly declare this as a **language subset release**, not full Python compatibility.
2. Keep `CLASS_SUBSET.md` as the contract and require new syntax/features to add tests first.
3. Gate future changes by keeping `run_class_test.py` and `run_if_test.py` green in CI.

If these conditions are accepted, this phase can be considered closed.
