# Matx Class Syntax Subset (Current)

This document defines the current `class` support boundary in `matx/python/ffi_system/parser.py`.

## Supported

- Class construction in assignments:
  - `c = Counter(0)`
- Instance attributes stored on object container:
  - `self.value = ...`
  - `x = c.value`
  - `c.value = ...`
- Instance method calls:
  - Statement: `c.inc(1)`
  - Expression: `x = c.inc(1) + 2`
  - Nested args: `c.inc(c.inc(1))`
- Class-method calls in control-flow expressions:
  - `if c.inc(1) > 0: ...`
  - `while c.inc(1) < 10: ...`
  - `for i in range(c.inc(1), c.inc(3)): ...`
  - `x = a if c.inc(1) > 0 else b`
- Class-method calls in expression containers:
  - Subscript index: `arr[c.inc(1)]`
  - Literals: `[c.inc(1)]`, `{"k": c.inc(1)}`, `{c.inc(1)}`

## Type Rules

- Method parameter type checks use method annotations.
- Method return type checks use method return annotation.
- Attribute assignment type checks apply when attribute type is inferred from `__init__`.
- `handle` is treated as a permissive top type.

## Explicit Errors (Current)

- Unknown method:
  - `Unknown method: <Class>.<method>`
- Wrong method arity:
  - `<Class>.<method> expects N args`
- Method missing `self`:
  - `Method <Class>.<method> must have self`
- Type mismatch:
  - Method args/return and class attribute assignment mismatches raise `Type mismatch ...`

## Not Supported Yet

- Inheritance
- `@staticmethod` / `@classmethod`
- Class variables and metaclass features
- `super()`
- Keyword-only/default/varargs method call semantics
- Full Python object model behavior (descriptor/properties/MRO/dynamic dispatch)

## Test Entry

See `matx/python/run_class_test.py` for positive and negative cases.

## Release Gate

Run the unified pre-release checks:

```bash
python3 matx/python/run_release_gate.py
```

This runs:
- `run_parser_consistency_test.py`
- `run_class_test.py`
- `run_interpreter_ast_class_test.py`

## Debug Logs

Runtime debug logs are disabled by default.
Enable them when needed:

```bash
MATX_DEBUG_LOG=1 python3 matx/python/run_release_gate.py
```
