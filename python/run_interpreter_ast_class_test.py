import os
import subprocess
import tempfile
import textwrap


def _interpreter_path() -> str:
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    path = os.path.join(root, "build", "apps", "interpreter_ast")
    if not os.path.exists(path):
        raise RuntimeError(f"interpreter_ast not found: {path}")
    return path


def _run_script(script: str):
    interp = _interpreter_path()
    source = textwrap.dedent(script).strip() + "\n"
    with tempfile.NamedTemporaryFile("w", suffix=".py", delete=False, encoding="utf-8") as fp:
        fp.write(source)
        path = fp.name
    try:
        proc = subprocess.run([interp, path], capture_output=True, text=True)
        stdout_lines = [ln.strip() for ln in proc.stdout.splitlines() if ln.strip()]
        last_stdout = stdout_lines[-1] if stdout_lines else ""
        err_text = (proc.stderr or "").strip()
        return proc.returncode, last_stdout, err_text
    finally:
        os.remove(path)


def _expect_success(name: str, script: str, expected_last_stdout: str):
    code, last_stdout, err = _run_script(script)
    if code != 0:
        raise RuntimeError(f"[{name}] expected success, got code={code}, err={err}")
    if last_stdout != expected_last_stdout:
        raise RuntimeError(
            f"[{name}] unexpected output: got={last_stdout}, expect={expected_last_stdout}"
        )
    print(f"[PASS] {name}")


def _expect_fail(name: str, script: str, must_contain: str):
    code, _last_stdout, err = _run_script(script)
    if code == 0:
        raise RuntimeError(f"[{name}] expected failure, but succeeded")
    if must_contain not in err:
        raise RuntimeError(f"[{name}] unexpected error: {err}")
    print(f"[PASS] {name}")


def run():
    _expect_success(
        "self_method_call",
        """
class Counter:
    def __init__(self, start: int):
        self.value = start
    def add(self, n: int) -> int:
        self.value = self.value + n
        return self.value
    def add_twice(self, n: int) -> int:
        x = self.add(n)
        y = self.add(n)
        return x * 10 + y

c = Counter(0)
c.add_twice(3)
""",
        "36",
    )

    _expect_success(
        "handle_arg_and_return",
        """
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
        "77",
    )

    _expect_fail(
        "bad_arg_type",
        """
class C:
    def __init__(self, x: int):
        self.x = x
    def need_int(self, n: int) -> int:
        return self.x + n

c = C(1)
c.need_int(1.5)
""",
        "Type mismatch in C.need_int arg 1",
    )

    _expect_fail(
        "bad_return_type_in_if",
        """
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
        "Type mismatch in return of C.bad",
    )

    _expect_fail(
        "unknown_method",
        """
class C:
    def __init__(self, x: int):
        self.x = x

c = C(0)
c.no_such(1)
""",
        "Unknown method: C.no_such",
    )

    print("interpreter_ast class tests passed")


if __name__ == "__main__":
    run()
