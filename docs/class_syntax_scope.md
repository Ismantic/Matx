# Class Syntax Scope (Current)

This document defines the current class subset supported by Matx frontends (`matx/src/parser.cc` and `matx/python/ffi_system/parser.py`).

## Supported

- `class Name:` definitions.
- Instance constructor `__init__(self, ...)`.
- Instance methods `def m(self, ...)`.
- Method calls `obj.m(...)`.
- Attribute read/write via `self.attr` and `obj.attr`.
- Method body control flow:
  - `if/else`
  - `while`
  - `for ... in range(...)`
  - `return` (including nested in control flow)
- Type annotations used by current checks:
  - `int` (`int64`)
  - `float` (`float64`)
  - `bool`
  - `None/str/handle` mapped to `handle`
- Basic compile-time checks:
  - argument count
  - argument type
  - return type
  - known class attribute assignment type (inferred from `__init__`)

## Explicitly Unsupported

- Inheritance semantics (`class D(Base): ...`) and `super()`.
- `@staticmethod` / `@classmethod`.
- Default arguments and keyword arguments.
- `*args` / `**kwargs`.
- Decorator-driven method semantics.
- Nested class semantics.
- Advanced Python object model behavior (descriptor protocol, MRO, metaclass behavior, etc.).

## Notes

- Some unsupported syntax may still parse in one frontend but is not guaranteed to be portable or semantically valid.
- Treat any behavior outside the supported list above as undefined unless it is promoted into this scope with tests.
