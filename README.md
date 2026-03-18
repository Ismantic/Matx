# Matx

Matx is an experimental compiler/runtime project for a small Python-like language. It combines a C++17 runtime, AST rewriting/code generation, dynamic module loading, and a Python-side prototype compiler.

This repository is best read as a working compiler playground, not a full language implementation.

## Highlights

- C++ runtime with value/object/container support
- AST construction, rewriting, and C++ code generation
- Native interpreter / REPL for a Python-like subset
- Python frontend that lowers restricted Python into generated C++
- Dynamic loading of generated shared libraries

## Current Scope

Matx currently supports a constrained subset rather than full Python.

- values: `int`, `float`, `bool`, `str`, `None`
- containers: `list`, `dict`, `set`
- expressions: arithmetic, comparison, boolean ops, indexing
- control flow: `if/else`, `while`, `for ... in range(...)`
- functions: basic typed parameters/returns
- classes: partial lowering for constructors, fields, and instance methods

One important semantic difference: integer `/` is treated as integer division in the native interpreter subset.

## Quick Start

Build from the repository root:

```bash
cmake -S . -B build
cmake --build build
```

Run the main native test binary:

```bash
LD_LIBRARY_PATH=build ./build/apps/test
```

Run the AST interpreter:

```bash
./build/apps/interpreter_ast
./build/apps/interpreter_ast path/to/script.mx
```

## Python Prototype

The Python pipeline parses a restricted subset, lowers it into Matx IR, emits C++, builds a shared object, and loads it back through the runtime.

Build the extension after the C++ runtime:

```bash
g++ -shared -fPIC case_ext.cc -I/usr/include/python3.14 -I. -Lbuild -lcase -o case_ext.so
```

Run the prototype entry:

```bash
python3 -u python/new_ffi.py
```

Useful regression scripts:

```bash
python3 -u python/run_if_test.py
python3 -u python/run_loop_test.py
python3 -u python/run_container_test.py
python3 -u python/run_types_test.py
python3 -u python/run_type_error_test.py
python3 -u python/run_range_neg_test.py
python3 -u python/run_class_test.py
```

If dynamic loading fails, check:

- `build/libcase.so`
- `case_ext.so`

## Repository Layout

```text
.
├── src/      # runtime, AST, parser, rewriter, C API
├── apps/     # native test binaries and interpreters
├── python/   # Python frontend and FFI/compiler experiments
├── docs/     # active documentation
└── Book/     # older design and runtime notes
```

## Status

The project is under active experimentation. Interfaces, supported syntax, and internal structure may change as the compiler/runtime model evolves.

## Documentation

- [docs/README.md](docs/README.md)
- [docs/SYNTAX.md](docs/SYNTAX.md)
- [docs/INTERPRETER.md](docs/INTERPRETER.md)
- [docs/compiler.md](docs/compiler.md)
- [python/CLASS_SUBSET.md](python/CLASS_SUBSET.md)
- [python/RELEASE_CHECKLIST.md](python/RELEASE_CHECKLIST.md)
