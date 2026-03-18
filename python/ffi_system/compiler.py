import ast
import inspect
import os
import subprocess
import textwrap

from .config import BUILD_DIR, SRC_DIR
from .parser import SimpleParser
from .runtime import Array, Str, matx_script_api


def simple_compile(target, dso_path):
    func_name = target.__name__

    source_code = textwrap.dedent(inspect.getsource(target))
    target_tree = ast.parse(source_code)

    class_names = set()
    for n in ast.walk(target_tree):
        if isinstance(n, ast.Call) and isinstance(n.func, ast.Name):
            class_names.add(n.func.id)

    source_parts = []
    global_ns = getattr(target, "__globals__", {})
    for cname in sorted(class_names):
        obj = global_ns.get(cname)
        if inspect.isclass(obj):
            source_parts.append(textwrap.dedent(inspect.getsource(obj)))

    source_parts.append(source_code)
    merged_source = "\n\n".join(source_parts)
    ast_tree = ast.parse(merged_source)

    parser = SimpleParser(entry_func_name=func_name)
    func_ir = parser.visit(ast_tree)
    all_funcs = [parser.functions[name] for name in parser.function_order]

    print(f"Generated IR for {func_name}")

    to_source = matx_script_api.GetGlobal("rewriter.BuildFunctions", True)
    code = to_source(Array(all_funcs), Str("fn"))
    cpp_code = code.data

    print(f"Generated C++ code for {func_name}:")
    print(cpp_code)

    cpp_filename = dso_path.replace(".so", ".cpp")
    with open(cpp_filename, "w", encoding="utf-8") as f:
        f.write(cpp_code)

    compile_cmd = [
        "g++",
        "-shared",
        "-fPIC",
        "-O2",
        cpp_filename,
        "-I" + SRC_DIR,
        "-L" + BUILD_DIR,
        "-lcase",
        "-o",
        dso_path,
    ]
    print("Compiling:", " ".join(compile_cmd))

    result = subprocess.run(compile_cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(result.stdout)
        print(result.stderr)
        return False

    if os.path.exists(dso_path):
        print(f"Successfully compiled to {dso_path}")
        return True

    print(f"Compilation failed for {dso_path}")
    return False
