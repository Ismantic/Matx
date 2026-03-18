def bad_assign(a: int) -> int:
    x = 1
    x = "oops"
    return 0


if __name__ == "__main__":
    from ffi_system import simple_compile

    try:
        simple_compile(bad_assign, "bad_assign.so")
    except Exception as exc:
        print("expected error:", exc)
