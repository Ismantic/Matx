from ffi_system import module_loader, simple_compile


def test_types(a: int) -> int:
    x = 1.5
    y = 2.0
    z = x + y
    b = True
    n = None
    if z > 3.4 and b and n == None:
        return 1
    return 0


def run():
    ok = simple_compile(test_types, "test_types.so")
    if not ok:
        raise RuntimeError("compile failed")

    mod = module_loader("./test_types.so")
    func = mod.get_function("test_types")
    print("result:", func(0))


if __name__ == "__main__":
    run()
