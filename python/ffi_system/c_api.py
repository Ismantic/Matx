import ctypes

from .config import LIBCASE_PATH


def c_str(value: str) -> ctypes.c_char_p:
    return ctypes.c_char_p(value.encode("utf-8"))


_LIB = ctypes.CDLL(LIBCASE_PATH, ctypes.RTLD_GLOBAL)
_LIB.GetError.restype = ctypes.c_char_p

FunctionHandle = ctypes.c_void_p
ModuleHandle = ctypes.c_void_p

_LIB.GetGlobal.argtypes = [ctypes.c_char_p, ctypes.POINTER(FunctionHandle)]
_LIB.GetGlobal.restype = ctypes.c_int

_LIB.GetBackendFunction.argtypes = [
    ModuleHandle,
    ctypes.c_char_p,
    ctypes.c_int,
    ctypes.POINTER(FunctionHandle),
]
_LIB.GetBackendFunction.restype = ctypes.c_int


class Value(ctypes.Structure):
    _fields_ = [
        ("u", ctypes.c_int64),
        ("p", ctypes.c_int32),
        ("t", ctypes.c_int32),
    ]


_LIB.FuncCall_PYTHON_C_API.argtypes = [
    FunctionHandle,
    ctypes.POINTER(Value),
    ctypes.c_int,
    ctypes.POINTER(Value),
]
_LIB.FuncCall_PYTHON_C_API.restype = ctypes.c_int
