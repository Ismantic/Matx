from ffi_system import module_loader, simple_compile


def test_func(a: int, b: int) -> int:
    if a < b and b != 0:
        return a
    else:
        return b


def run():
    ok = simple_compile(test_func, "test_func.so")
    if not ok:
        return

    mod = module_loader("./test_func.so")
    fn = mod.get_function("test_func")
    if fn is None:
        print("Failed to get function")
        return

    cases = [
        (1, 2, 1),
        (5, 2, 2),
        (3, 0, 0),
        (7, 9, 7),
    ]
    for a, b, expected in cases:
        got = fn(a, b)
        status = "✓" if got == expected else "✗"
        print(f"{status} test_func({a}, {b}) = {got} (expected {expected})")


if __name__ == "__main__":
    run()
