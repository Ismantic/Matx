from ffi_system import module_loader, simple_compile


class Counter:
    def __init__(self, start: int) -> None:
        self.value = start

    def inc(self, step: int) -> int:
        self.value = self.value + step
        return self.value


class BadArgCounter:
    def __init__(self, start: int) -> None:
        self.value = start

    def inc(self, step: int) -> int:
        self.value = self.value + step
        return self.value


class BadRetCounter:
    def __init__(self, start: int) -> None:
        self.value = start

    def bad(self) -> int:
        return 1.5


class BadAttrCounter:
    def __init__(self, start: int) -> None:
        self.value = start

    def set_bad(self) -> None:
        self.value = 1.25


class BadArityCounter:
    def __init__(self, start: int) -> None:
        self.value = start

    def inc(self, step: int) -> int:
        self.value = self.value + step
        return self.value


class UnknownMethodCounter:
    def __init__(self, start: int) -> None:
        self.value = start

    def inc(self, step: int) -> int:
        self.value = self.value + step
        return self.value


class MissingSelfCounter:
    def __init__(self, start: int) -> None:
        self.value = start

    def bad(step: int) -> int:
        return step


class FlowCounter:
    def __init__(self, start: int) -> None:
        self.value = start

    def pick(self, step: int) -> int:
        if step > 0:
            return self.value + step
        return self.value - step

    def first_ge(self, n: int) -> int:
        i = 0
        while i < 10:
            if i >= n:
                return i
            i = i + 1
        return -1


class BadFlowRetCounter:
    def __init__(self, start: int) -> None:
        self.value = start

    def bad(self, step: int) -> int:
        if step > 0:
            return 1.5
        return 0


class InternalCallCounter:
    def __init__(self, start: int) -> None:
        self.value = start

    def add(self, n: int) -> int:
        self.value = self.value + n
        return self.value

    def add_twice(self, n: int) -> int:
        x = self.add(n)
        y = self.add(n)
        return x * 10 + y


class BadInternalCallCounter:
    def __init__(self, start: int) -> None:
        self.value = start

    def bad(self, n: int) -> int:
        return self.no_such(n)


class HandleCounter:
    def __init__(self, start: int) -> None:
        self.value = start

    def merge(self, other: handle) -> int:
        self.value = self.value + other.value
        return self.value

    def ret_self(self) -> handle:
        return self


class BadHandleArgCounter:
    def __init__(self, start: int) -> None:
        self.value = start

    def need_int(self, x: int) -> int:
        return self.value + x


class BadHandleRetCounter:
    def __init__(self, start: int) -> None:
        self.value = start

    def bad(self) -> int:
        return self


def test_counter(n: int) -> int:
    c = Counter(10)
    last = 0
    i = 0
    while i < n:
        last = c.inc(2)
        i = i + 1
    if last == c.value:
        return c.value
    return 0


def test_return_call(_: int) -> int:
    c = Counter(7)
    return c.inc(5)


def test_expr_call(_: int) -> int:
    c = Counter(1)
    x = c.inc(2) + 1
    if x == 4 and c.value == 3:
        return 1
    return 0


def test_if_call(_: int) -> int:
    c = Counter(0)
    if c.inc(1) > 0:
        return 1
    return 0


def test_while_call(_: int) -> int:
    c = Counter(0)
    i = 0
    while c.inc(1) < 4:
        i = i + 1
    return i * 10 + c.value


def test_short_circuit(_: int) -> int:
    c = Counter(0)
    if 0 and c.inc(1) > 0:
        return -1
    if 1 or c.inc(10) > 0:
        pass
    return c.value


def test_for_range_call(_: int) -> int:
    c = Counter(0)
    total = 0
    for i in range(c.inc(2), c.inc(5)):
        total = total + i
    return total * 10 + c.value


def test_nested_arg_call(_: int) -> int:
    c = Counter(0)
    x = c.inc(c.inc(1))
    return x * 10 + c.value


def test_ifexp_call(_: int) -> int:
    c = Counter(0)
    x = 10 if c.inc(1) > 0 else 20
    y = c.inc(1) if 0 else c.inc(2)
    return x * 100 + y * 10 + c.value


def test_subscript_call(_: int) -> int:
    c = Counter(0)
    arr = [10, 20, 30, 40]
    v = arr[c.inc(1)]
    return v * 10 + c.value


def test_literal_call(_: int) -> int:
    c = Counter(0)
    lst = [c.inc(1), c.inc(2)]
    st = {c.inc(1), c.inc(1)}
    d = {"k": c.inc(1)}
    return lst[0] * 100 + lst[1] * 10 + d["k"] + c.value + d["k"] - d["k"]


def test_bad_arg_type(_: int) -> int:
    c = BadArgCounter(0)
    return c.inc(1.5)


def test_bad_return_type(_: int) -> int:
    c = BadRetCounter(0)
    return c.bad()


def test_bad_attr_assign_type(_: int) -> int:
    c = BadAttrCounter(0)
    c.set_bad()
    return c.value


def test_bad_arity(_: int) -> int:
    c = BadArityCounter(0)
    return c.inc()


def test_unknown_method(_: int) -> int:
    c = UnknownMethodCounter(0)
    return c.no_such_method(1)


def test_missing_self(_: int) -> int:
    c = MissingSelfCounter(0)
    return c.bad(1)


def test_flow_return_if(_: int) -> int:
    c = FlowCounter(10)
    a = c.pick(2)
    b = c.pick(-3)
    return a * 100 + b


def test_flow_return_while(_: int) -> int:
    c = FlowCounter(0)
    a = c.first_ge(3)
    b = c.first_ge(12)
    return a * 10 + b


def test_bad_flow_return_type(_: int) -> int:
    c = BadFlowRetCounter(0)
    return c.bad(1)


def test_internal_method_call(_: int) -> int:
    c = InternalCallCounter(0)
    return c.add_twice(3)


def test_bad_internal_method_call(_: int) -> int:
    c = BadInternalCallCounter(0)
    return c.bad(1)


def test_handle_arg_and_ret(_: int) -> int:
    a = HandleCounter(2)
    b = HandleCounter(5)
    x = a.merge(b)
    c = a.ret_self()
    return x * 10 + c.value


def test_bad_handle_arg_type(_: int) -> int:
    c = BadHandleArgCounter(1)
    return c.need_int(c)


def test_bad_handle_return_type(_: int) -> int:
    c = BadHandleRetCounter(1)
    return c.bad()


def expect_compile_error(func, dso_name: str, must_contain: str) -> None:
    try:
        ok = simple_compile(func, dso_name)
    except Exception as exc:
        msg = str(exc)
        if must_contain not in msg:
            raise RuntimeError(
                f"unexpected error for {func.__name__}: {msg}, expect contains: {must_contain}"
            )
        print(f"expected error for {func.__name__}: {msg}")
        return
    if ok:
        raise RuntimeError(f"{func.__name__} should fail, but compile succeeded")
    raise RuntimeError(f"{func.__name__} should fail with parser error, but got compile False")


def run():
    ok = simple_compile(test_counter, "test_counter.so")
    ok2 = simple_compile(test_return_call, "test_return_call.so")
    ok3 = simple_compile(test_expr_call, "test_expr_call.so")
    ok4 = simple_compile(test_if_call, "test_if_call.so")
    ok5 = simple_compile(test_while_call, "test_while_call.so")
    ok6 = simple_compile(test_short_circuit, "test_short_circuit.so")
    ok7 = simple_compile(test_for_range_call, "test_for_range_call.so")
    ok8 = simple_compile(test_nested_arg_call, "test_nested_arg_call.so")
    ok9 = simple_compile(test_ifexp_call, "test_ifexp_call.so")
    ok10 = simple_compile(test_subscript_call, "test_subscript_call.so")
    ok11 = simple_compile(test_literal_call, "test_literal_call.so")
    ok12 = simple_compile(test_flow_return_if, "test_flow_return_if.so")
    ok13 = simple_compile(test_flow_return_while, "test_flow_return_while.so")
    ok14 = simple_compile(test_internal_method_call, "test_internal_method_call.so")
    ok15 = simple_compile(test_handle_arg_and_ret, "test_handle_arg_and_ret.so")
    if (
        not ok
        or not ok2
        or not ok3
        or not ok4
        or not ok5
        or not ok6
        or not ok7
        or not ok8
        or not ok9
        or not ok10
        or not ok11
        or not ok12
        or not ok13
        or not ok14
        or not ok15
    ):
        raise RuntimeError("compile failed")

    mod = module_loader("./test_counter.so")
    func = mod.get_function("test_counter")
    print("test_counter(3) =", func(3))

    mod2 = module_loader("./test_return_call.so")
    func2 = mod2.get_function("test_return_call")
    print("test_return_call(0) =", func2(0))

    mod3 = module_loader("./test_expr_call.so")
    func3 = mod3.get_function("test_expr_call")
    print("test_expr_call(0) =", func3(0))

    mod4 = module_loader("./test_if_call.so")
    func4 = mod4.get_function("test_if_call")
    print("test_if_call(0) =", func4(0))

    mod5 = module_loader("./test_while_call.so")
    func5 = mod5.get_function("test_while_call")
    print("test_while_call(0) =", func5(0))

    mod6 = module_loader("./test_short_circuit.so")
    func6 = mod6.get_function("test_short_circuit")
    print("test_short_circuit(0) =", func6(0))

    mod7 = module_loader("./test_for_range_call.so")
    func7 = mod7.get_function("test_for_range_call")
    print("test_for_range_call(0) =", func7(0))

    mod8 = module_loader("./test_nested_arg_call.so")
    func8 = mod8.get_function("test_nested_arg_call")
    print("test_nested_arg_call(0) =", func8(0))

    mod9 = module_loader("./test_ifexp_call.so")
    func9 = mod9.get_function("test_ifexp_call")
    print("test_ifexp_call(0) =", func9(0))

    mod10 = module_loader("./test_subscript_call.so")
    func10 = mod10.get_function("test_subscript_call")
    print("test_subscript_call(0) =", func10(0))

    mod11 = module_loader("./test_literal_call.so")
    func11 = mod11.get_function("test_literal_call")
    print("test_literal_call(0) =", func11(0))

    mod12 = module_loader("./test_flow_return_if.so")
    func12 = mod12.get_function("test_flow_return_if")
    print("test_flow_return_if(0) =", func12(0))

    mod13 = module_loader("./test_flow_return_while.so")
    func13 = mod13.get_function("test_flow_return_while")
    print("test_flow_return_while(0) =", func13(0))

    mod14 = module_loader("./test_internal_method_call.so")
    func14 = mod14.get_function("test_internal_method_call")
    print("test_internal_method_call(0) =", func14(0))

    mod15 = module_loader("./test_handle_arg_and_ret.so")
    func15 = mod15.get_function("test_handle_arg_and_ret")
    print("test_handle_arg_and_ret(0) =", func15(0))

    expect_compile_error(
        test_bad_arg_type,
        "test_bad_arg_type.so",
        "Type mismatch in BadArgCounter.inc arg 1",
    )
    expect_compile_error(
        test_bad_return_type,
        "test_bad_return_type.so",
        "Type mismatch in return of BadRetCounter.bad",
    )
    expect_compile_error(
        test_bad_attr_assign_type,
        "test_bad_attr_assign_type.so",
        "Type mismatch for BadAttrCounter.value",
    )
    expect_compile_error(
        test_bad_arity,
        "test_bad_arity.so",
        "BadArityCounter.inc expects 1 args",
    )
    expect_compile_error(
        test_unknown_method,
        "test_unknown_method.so",
        "Unknown method: UnknownMethodCounter.no_such_method",
    )
    expect_compile_error(
        test_missing_self,
        "test_missing_self.so",
        "Method MissingSelfCounter.bad must have self",
    )
    expect_compile_error(
        test_bad_flow_return_type,
        "test_bad_flow_return_type.so",
        "Type mismatch in return of BadFlowRetCounter.bad",
    )
    expect_compile_error(
        test_bad_internal_method_call,
        "test_bad_internal_method_call.so",
        "Unknown method: BadInternalCallCounter.no_such",
    )
    expect_compile_error(
        test_bad_handle_arg_type,
        "test_bad_handle_arg_type.so",
        "Type mismatch in BadHandleArgCounter.need_int arg 1",
    )
    expect_compile_error(
        test_bad_handle_return_type,
        "test_bad_handle_return_type.so",
        "Type mismatch in return of BadHandleRetCounter.bad",
    )


if __name__ == "__main__":
    run()
