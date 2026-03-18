from ffi_system import module_loader, simple_compile


def test_func(a: int, b: int) -> int:
    c = a + b
    return c


def test_simple_function_compile():
    print("\n=== test_simple_function_compile ===")

    success = simple_compile(test_func, "test_func.so")
    if not success:
        return False

    print("\nLoading compiled function...")

    module = module_loader("./test_func.so")
    test_func_handle = module.get_function("test_func")
    if test_func_handle is None:
        print("Failed to get test_func function")
        return False

    test_cases = [
        (3, 4, 7),
        (10, 20, 30),
        (-5, 15, 10),
        (0, 0, 0),
    ]

    for a, b, expected in test_cases:
        result = test_func_handle(a, b)
        ok = result == expected
        status = "✓" if ok else "✗"
        print(f"  {status} test_func({a}, {b}) = {result} (expected {expected})")
        if not ok:
            return False

    print("\nAll function tests passed!")
    return True


if __name__ == "__main__":
    test_simple_function_compile()
