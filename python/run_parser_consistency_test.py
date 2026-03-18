import ast
import os
import subprocess
import tempfile
import textwrap

from ffi_system.parser import SimpleParser


def _run_cpp_frontend(script: str):
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    interp = os.path.join(root, "build", "apps", "interpreter_ast")
    if not os.path.exists(interp):
        raise RuntimeError(f"interpreter_ast not found: {interp}")
    with tempfile.NamedTemporaryFile("w", suffix=".py", delete=False, encoding="utf-8") as fp:
        fp.write(script)
        path = fp.name
    try:
        proc = subprocess.run([interp, path], capture_output=True, text=True)
        if proc.returncode == 0:
            return True, proc.stdout.strip()
        msg = (proc.stderr or proc.stdout).strip()
        return False, msg
    finally:
        os.remove(path)


def _run_py_frontend(body: str):
    wrapped = "def entry(_: int) -> int:\n" + textwrap.indent(body, "    ")
    tree = ast.parse(wrapped)
    parser = SimpleParser(entry_func_name="entry")
    try:
        parser.visit(tree)
        return True, ""
    except Exception as exc:
        return False, str(exc)


def _check_case(case):
    name = case["name"]
    script = textwrap.dedent(case["script"]).strip() + "\n"
    py_body = textwrap.dedent(case.get("py_body", case["script"])).strip() + "\n"
    cpp_ok, cpp_msg = _run_cpp_frontend(script)
    py_ok, py_msg = _run_py_frontend(py_body)
    if cpp_ok != py_ok:
        raise RuntimeError(
            f"[{name}] consistency mismatch: cpp_ok={cpp_ok}, py_ok={py_ok}, "
            f"cpp_msg={cpp_msg}, py_msg={py_msg}"
        )
    if not cpp_ok:
        cpp_expect = case.get("cpp_error_contains", "")
        py_expect = case.get("py_error_contains", cpp_expect)
        if cpp_expect and cpp_expect not in cpp_msg:
            raise RuntimeError(f"[{name}] cpp error mismatch: {cpp_msg}")
        if py_expect and py_expect not in py_msg:
            raise RuntimeError(f"[{name}] py error mismatch: {py_msg}")
    print(f"[PASS] {name}")


def run():
    cases = [
        {
            "name": "class_basic_success",
            "script": """
class Counter:
    def __init__(self, start: int):
        self.value = start
    def inc(self, step: int) -> int:
        self.value = self.value + step
        return self.value
c = Counter(10)
x = c.inc(2)
y = c.inc(3)
x * 100 + y
""",
            "py_body": """
class Counter:
    def __init__(self, start: int):
        self.value = start
    def inc(self, step: int) -> int:
        self.value = self.value + step
        return self.value
c = Counter(10)
x = c.inc(2)
y = c.inc(3)
return x * 100 + y
""",
        },
        {
            "name": "class_flow_return_success",
            "script": """
class C:
    def __init__(self, x: int):
        self.x = x
    def f(self, n: int) -> int:
        i = 0
        while i < 10:
            if i >= n:
                return i
            i = i + 1
        return -1
c = C(0)
a = c.f(3)
b = c.f(12)
a * 10 + b
""",
            "py_body": """
class C:
    def __init__(self, x: int):
        self.x = x
    def f(self, n: int) -> int:
        i = 0
        while i < 10:
            if i >= n:
                return i
            i = i + 1
        return -1
c = C(0)
a = c.f(3)
b = c.f(12)
return a * 10 + b
""",
        },
        {
            "name": "bad_arg_type",
            "script": """
class C:
    def __init__(self, x: int):
        self.x = x
    def inc(self, n: int) -> int:
        return self.x + n
c = C(1)
c.inc(1.5)
""",
            "py_body": """
class C:
    def __init__(self, x: int):
        self.x = x
    def inc(self, n: int) -> int:
        return self.x + n
c = C(1)
return c.inc(1.5)
""",
            "cpp_error_contains": "Type mismatch in C.inc arg 1",
            "py_error_contains": "Type mismatch in C.inc arg 1",
        },
        {
            "name": "bad_return_type_in_if",
            "script": """
class C:
    def __init__(self, x: int):
        self.x = x
    def bad(self, n: int) -> int:
        if n > 0:
            return 1.5
        return 0
c = C(0)
c.bad(1)
""",
            "py_body": """
class C:
    def __init__(self, x: int):
        self.x = x
    def bad(self, n: int) -> int:
        if n > 0:
            return 1.5
        return 0
c = C(0)
return c.bad(1)
""",
            "cpp_error_contains": "Type mismatch in return of C.bad",
            "py_error_contains": "Type mismatch in return of C.bad",
        },
        {
            "name": "unknown_method",
            "script": """
class C:
    def __init__(self, x: int):
        self.x = x
c = C(0)
c.no_such(1)
""",
            "py_body": """
class C:
    def __init__(self, x: int):
        self.x = x
c = C(0)
return c.no_such(1)
""",
            "cpp_error_contains": "Unknown method: C.no_such",
            "py_error_contains": "Unknown method: C.no_such",
        },
        {
            "name": "self_internal_call_success",
            "script": """
class C:
    def __init__(self, x: int):
        self.x = x
    def add(self, n: int) -> int:
        self.x = self.x + n
        return self.x
    def add_twice(self, n: int) -> int:
        a = self.add(n)
        b = self.add(n)
        return a * 10 + b
c = C(0)
c.add_twice(3)
""",
            "py_body": """
class C:
    def __init__(self, x: int):
        self.x = x
    def add(self, n: int) -> int:
        self.x = self.x + n
        return self.x
    def add_twice(self, n: int) -> int:
        a = self.add(n)
        b = self.add(n)
        return a * 10 + b
c = C(0)
return c.add_twice(3)
""",
        },
        {
            "name": "self_internal_call_unknown_method",
            "script": """
class C:
    def __init__(self, x: int):
        self.x = x
    def bad(self, n: int) -> int:
        return self.no_such(n)
c = C(0)
c.bad(1)
""",
            "py_body": """
class C:
    def __init__(self, x: int):
        self.x = x
    def bad(self, n: int) -> int:
        return self.no_such(n)
c = C(0)
return c.bad(1)
""",
            "cpp_error_contains": "Unknown method: C.no_such",
            "py_error_contains": "Unknown method: C.no_such",
        },
        {
            "name": "handle_arg_and_return_success",
            "script": """
class C:
    def __init__(self, x: int):
        self.x = x
    def merge(self, other: handle) -> int:
        self.x = self.x + other.x
        return self.x
    def ret_self(self) -> handle:
        return self
a = C(2)
b = C(5)
x = a.merge(b)
c = a.ret_self()
x * 10 + c.x
""",
            "py_body": """
class C:
    def __init__(self, x: int):
        self.x = x
    def merge(self, other: handle) -> int:
        self.x = self.x + other.x
        return self.x
    def ret_self(self) -> handle:
        return self
a = C(2)
b = C(5)
x = a.merge(b)
c = a.ret_self()
return x * 10 + c.x
""",
        },
        {
            "name": "handle_arg_to_int_param_fail",
            "script": """
class C:
    def __init__(self, x: int):
        self.x = x
    def need_int(self, n: int) -> int:
        return self.x + n
c = C(1)
c.need_int(c)
""",
            "py_body": """
class C:
    def __init__(self, x: int):
        self.x = x
    def need_int(self, n: int) -> int:
        return self.x + n
c = C(1)
return c.need_int(c)
""",
            "cpp_error_contains": "Type mismatch in C.need_int arg 1",
            "py_error_contains": "Type mismatch in C.need_int arg 1",
        },
        {
            "name": "handle_return_to_int_decl_fail",
            "script": """
class C:
    def __init__(self, x: int):
        self.x = x
    def bad(self) -> int:
        return self
c = C(0)
c.bad()
""",
            "py_body": """
class C:
    def __init__(self, x: int):
        self.x = x
    def bad(self) -> int:
        return self
c = C(0)
return c.bad()
""",
            "cpp_error_contains": "Type mismatch in return of C.bad",
            "py_error_contains": "Type mismatch in return of C.bad",
        },
        {
            "name": "static_method_not_supported",
            "script": """
class C:
    def __init__(self, x: int):
        self.x = x
    @staticmethod
    def f(x: int) -> int:
        return x
c = C(0)
c.f(1)
""",
            "py_body": """
class C:
    def __init__(self, x: int):
        self.x = x
    @staticmethod
    def f(x: int) -> int:
        return x
c = C(0)
return c.f(1)
""",
            "cpp_error_contains": "Method C.f must have self",
            "py_error_contains": "Method C.f must have self",
        },
        {
            "name": "default_arg_not_supported",
            "script": """
class C:
    def __init__(self, x: int):
        self.x = x
    def inc(self, n: int = 1) -> int:
        return self.x + n
c = C(0)
c.inc()
""",
            "py_body": """
class C:
    def __init__(self, x: int):
        self.x = x
    def inc(self, n: int = 1) -> int:
        return self.x + n
c = C(0)
return c.inc()
""",
            "cpp_error_contains": "C.inc expects 1 args",
            "py_error_contains": "C.inc expects 1 args",
        },
        {
            "name": "keyword_arg_not_supported",
            "script": """
class C:
    def __init__(self, x: int):
        self.x = x
    def inc(self, n: int) -> int:
        return self.x + n
c = C(0)
c.inc(n=1)
""",
            "py_body": """
class C:
    def __init__(self, x: int):
        self.x = x
    def inc(self, n: int) -> int:
        return self.x + n
c = C(0)
return c.inc(n=1)
""",
        },
        {
            "name": "nested_class_not_supported",
            "script": """
class Outer:
    class Inner:
        pass
o = Outer()
""",
            "py_body": """
class Outer:
    class Inner:
        pass
o = Outer()
return 0
""",
        },
    ]
    for case in cases:
        _check_case(case)
    print("parser consistency check passed")


if __name__ == "__main__":
    run()
