# Matx

Matx is an experimental compiler/runtime project with three main pieces:

- a C++17 runtime and AST/rewriter pipeline in `src/`
- native test and REPL applications in `apps/`
- a Python-side FFI/compiler prototype in `python/`

The repository currently focuses on direct AST construction, code generation to C++, dynamic module loading, and a small Python-like interpreter subset.

## Repository Layout

```text
.
├── CMakeLists.txt
├── apps/          # native test drivers and REPL binaries
├── src/           # runtime, AST, parser, rewriter, FFI support
├── python/        # Python parser/compiler/FFI experiments
├── docs/          # design notes and usage docs
└── Book/          # object model and runtime notes
```

## Build

From the repository root:

```bash
cmake -S . -B build
cmake --build build
```

Main native test:

```bash
LD_LIBRARY_PATH=build ./build/apps/test
```

Native AST interpreter:

```bash
./build/apps/interpreter_ast
./build/apps/interpreter_ast path/to/script.mx
```

## Python Pipeline

The Python prototype compiles a restricted Python subset into generated C++, builds a shared object, then loads it through the native runtime.

Build the C++ runtime first, then build the Python extension:

```bash
g++ -shared -fPIC case_ext.cc -I/usr/include/python3.14 -I. -Lbuild -lcase -o case_ext.so
```

Run the main pipeline:

```bash
python3 -u python/new_ffi.py
```

Targeted Python checks:

```bash
python3 -u python/run_if_test.py
python3 -u python/run_loop_test.py
python3 -u python/run_container_test.py
python3 -u python/run_types_test.py
python3 -u python/run_type_error_test.py
python3 -u python/run_range_neg_test.py
```

If runtime loading fails, confirm these artifacts exist:

- `build/libcase.so`
- `case_ext.so`

## Language Notes

The interpreter and Python frontend are intentionally limited. Current support includes:

- integers, floats, bools, strings, and `None`
- list/dict/set literals
- indexing and indexed assignment
- arithmetic and comparison expressions
- boolean operators
- `if/else`, `while`, and `for ... in range(...)`
- user functions and parts of user-defined class lowering

This is a Python-like language, not full Python. For example, integer `/` is integer division in the native interpreter subset.

## Documentation

- [docs/README.md](docs/README.md)
- [docs/INTERPRETER.md](docs/INTERPRETER.md)
- [docs/SYNTAX.md](docs/SYNTAX.md)
- [python/RELEASE_CHECKLIST.md](python/RELEASE_CHECKLIST.md)
