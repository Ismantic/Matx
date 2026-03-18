#pragma once

#include "runtime_port.h"
#include "datatype.h"

#ifdef __cplusplus
extern "C" {
#endif
#include <stddef.h>
#include <stdint.h>

using mc::runtime::Dt;
using mc::runtime::DataType;

union Union {
  int64_t v_int;
  double  v_float;
  char*   v_str;
  void*   v_pointer;
  Dt      v_datatype;
};

struct Value {
  Union u{};
  int32_t p{0}; // str
  int32_t t{0};
};


typedef void* ModuleHandle;
typedef void* FunctionHandle;
typedef void* ValueHandle;
typedef void* ObjectHandle;

DLL void SetError(const char* s);
DLL const char* GetError(void);

DLL int FuncFree(FunctionHandle fn);
DLL int FuncCall_PYTHON_C_API(FunctionHandle fn, Value* vs, int n, Value* r);

DLL int DataTypeToStr(Dt t, char* str, int* size);

DLL int RegisterGlobal(const char* name, FunctionHandle fn);
DLL int GetGlobal(const char* name, FunctionHandle* fn);
DLL int ListGlobalNames(int* out_size, const char*** out_array);

DLL int ObjectRetain(ObjectHandle o);
DLL int ObjectFree(ObjectHandle o);

DLL int GetIndex(const char* name, unsigned* index);

typedef int(*BackendFunc)(Value* args, int num_args, Value* rv, void* re);

DLL int BackendRegisterSystemLibSymbol(const char* name, void* ptr);

typedef struct FuncRegistry {
  const char* names;
  const BackendFunc* funcs;
} FuncRegistry;

DLL int GetBackendFunction(ModuleHandle m, 
                           const char* func_name, 
                           int query,
                           FunctionHandle* out);

#ifdef __cplusplus
}  // EXTERN_C
#endif
