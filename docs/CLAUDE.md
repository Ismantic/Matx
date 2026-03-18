# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

This is a CMake-based C++ project with Python bindings:

```bash
# Build the project
mkdir -p build && cd build
cmake .. && make -j$(nproc)

# Build test executable
cd build && make test
```

## Project Architecture

This is a hybrid C++/Python runtime system that appears to be a compiler infrastructure or DSL framework called "MC" (MatX Compiler). The architecture follows these key patterns:

### Core Components

- **C++ Runtime Core** (`src/`): Object system, type system, and runtime infrastructure
  - `object.h/cc`: Reference-counted object system with `object_t` base class and `object_p<T>` smart pointers
  - `datatype.h/cc`: Type system with `DataType` and `Dt` enums  
  - `runtime_*.cc`: Runtime value system, string handling, and module management
  - `c_api.h/cc`: C API layer for Python FFI with `Value` structs and function handles

- **Python Frontend**: Python modules that interface with the C++ runtime
  - `ffi.py`: FFI loader that loads shared libraries (`libmatx.so`, `matx_script_api.so`)
  - `register.py`: Registration system for objects and functions between Python and C++
  - `runtime.py`: Python runtime API initialization
  - `mast.py`, `mast_*.py`: AST/IR representation modules

### Key Design Patterns

- **Object System**: Custom reference-counted objects with type indices and runtime type checking
- **FFI Bridge**: C API layer with `Value` unions for cross-language data exchange
- **Module System**: Dynamic function registration and global function lookup
- **AST/IR**: Multi-level intermediate representation system

### Build Structure

- Main library: `libcase.so` (built from `src/` sources)
- Test executable: `test` (built from `apps/test.cc`)
- Python modules dynamically load shared libraries via ctypes

## Development Notes

- The project uses C++17 with CMake 3.24+
- Python FFI is implemented via ctypes loading of shared libraries
- Object lifetime managed through reference counting in the C++ layer
- Type system uses runtime type indices for dynamic dispatch