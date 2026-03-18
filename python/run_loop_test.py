from ffi_system import module_loader, simple_compile


def test_func(n: int) -> int:
    total = 0
    i = 0
    while i < n:
        total = total + i
        i = i + 1

    acc = 0
    for j in range(1, n, 2):
        acc = acc + j

    if 0 < n < 10:
        return total + acc
    else:
        return total


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
        (0, 0),
        (1, 0),
        (5, 14),
        (10, 45),
    ]
    for n, expected in cases:
        got = fn(n)
        status = "✓" if got == expected else "✗"
        print(f"{status} test_func({n}) = {got} (expected {expected})")


if __name__ == "__main__":
    run()
