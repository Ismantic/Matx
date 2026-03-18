from ffi_system import module_loader, simple_compile


def test_containers(a: int) -> int:
    s = "hi"
    t = s + "!"
    nums = [1, 2, 3]
    nums.append(4)
    nums[1] = 9
    d = {"a": 1, "b": 2}
    d["c"] = 5
    if t == "hi!" and nums[1] == 9 and d["c"] == 5 and nums[3] == 4:
        return 1
    return 0


def run():
    ok = simple_compile(test_containers, "test_containers.so")
    if not ok:
        raise RuntimeError("compile failed")

    mod = module_loader("./test_containers.so")
    func = mod.get_function("test_containers")
    print("result:", func(0))


if __name__ == "__main__":
    run()
