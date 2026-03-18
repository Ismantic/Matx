import os
import subprocess
import sys


def _python_dir() -> str:
    return os.path.dirname(os.path.abspath(__file__))


def _run(script_name: str):
    py_dir = _python_dir()
    script_path = os.path.join(py_dir, script_name)
    cmd = [sys.executable, script_path]
    print(f"[RUN] {script_name}")
    proc = subprocess.run(cmd, cwd=py_dir)
    if proc.returncode != 0:
        raise RuntimeError(f"{script_name} failed with exit code {proc.returncode}")
    print(f"[PASS] {script_name}")


def run():
    checks = [
        "run_parser_consistency_test.py",
        "run_class_test.py",
        "run_interpreter_ast_class_test.py",
    ]
    for check in checks:
        _run(check)
    print("release gate passed")


if __name__ == "__main__":
    run()
