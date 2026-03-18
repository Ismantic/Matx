# Documentation Index

This folder contains the actively maintained user-facing and implementation-facing documentation. Core runtime background notes that overlap with these topics live in `Book/` at the repo root.

## Core Docs
- `SYNTAX.md`: Supported language syntax and semantics (Python-like subset).
- `INTERPRETER.md`: Interpreter (AST REPL) usage and behavior.
- `INTERPRETER_AST_SUBSET.md`: Current capability matrix for the AST interpreter.
- `compiler.md`: Python frontend to C++ codegen pipeline notes.

## Runtime, Loading, And FFI
- `Runtime.md`: Runtime overview.
- `LibLoader.md`: Dynamic library/module loading notes.
- `C_EXT.md`: Python C extension notes for `case_ext.cc`.
- `PythonCE.md`: Python C-API notes.

## Class Support
- `class_syntax_scope.md`: Current class syntax subset and type-checking scope.
- `HowToUserClass.md`: Detailed class lowering design and implementation notes.

## Older Background Notes In `Book/`
- `Book/Object.md`: Runtime object model.
- `Book/Value.md`: Runtime value system.
- `Book/Container.md`: Runtime container system.
- `Book/Ast.md`: AST/IR structure.
- `Book/Function.md`: Function system.
- `Book/Visitor.md`: Visitor infrastructure.
- `Book/FFI.md`: FFI architecture background.
