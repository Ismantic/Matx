import ctypes

from .c_api import _LIB, c_str, FunctionHandle, ModuleHandle
from .runtime import matx_script_api


class Module:
    def __init__(self, handle):
        self.handle = handle

    def get_function(self, name):
        ret_handle = FunctionHandle()
        status = _LIB.GetBackendFunction(
            ModuleHandle(self.handle),
            c_str(name),
            0,
            ctypes.byref(ret_handle),
        )
        if status != 0 or ret_handle.value is None:
            print(f"Failed to get function: {name}, status: {status}")
            return None
        return matx_script_api.PackedFuncBase(ret_handle.value, 1)


def _conv(mod: Module):
    return matx_script_api.make_any(1, 0, mod.handle, 0)


matx_script_api.register_object(1, Module)
matx_script_api.register_input_callback(Module, _conv)

module_loader = matx_script_api.GetGlobal("runtime.ModuleLoader", True)
