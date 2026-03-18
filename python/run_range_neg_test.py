from ffi_system import module_loader, simple_compile


def test_range_neg(a: int) -> int:
    total = 0
    for i in range(5, 0, -1):
        total = total + i
    if total == 15:
        return 1
    return 0


def run():
    ok = simple_compile(test_range_neg, "test_range_neg.so")
    if not ok:
        raise RuntimeError("compile failed")

    mod = module_loader("./test_range_neg.so")
    func = mod.get_function("test_range_neg")
    print("result:", func(0))


if __name__ == "__main__":
    run()
