// case_ext.cc
#include <Python.h>
#include <structmember.h>

#include "src/c_api.h"

// g++ -shared -fPIC case_ext.cc -I/usr/include/python3.12 -L./build -lcase -o case_ext.so

static PyObject* PACKEDFUNC_CREATOR = NULL;

static PyObject* RETURN_SWITCH = NULL;

static PyObject* DEFAULT_CLASS_OBJECT = NULL;

#define MAX_OBJECT_CALLBACK_NUM 128
typedef struct {
    long long index;
    PyObject* callback;
} ObjectCallback;

static ObjectCallback OBJECT_CALLBACK_TABLE[MAX_OBJECT_CALLBACK_NUM];
static int OBJECT_CALLBACK_CUR_IDX = 0;

#define MAX_INPUT_INSTANCE_CALLBACK_NUM 100
static PyObject* INPUT_INSTANCE_CALLBACK[MAX_INPUT_INSTANCE_CALLBACK_NUM][2];
static int INPUT_INSTANCE_CALLBACK_CUR = 0;

typedef struct PyPackedFuncBase {
    PyObject_HEAD;
    FunctionHandle handle;
    int is_global;
} PyPackedFuncBase;

typedef struct PyObjectBase {
    PyObject_HEAD;
    ObjectHandle handle;
    int32_t type_code;
} PyObjectBase;

typedef struct PyAny {
    PyObject_HEAD;
    Value value;
} PyAny;

static PyMemberDef PyPackedFuncBase_members[] = {
    {
        "handle",                               /* name */
        T_ULONGLONG,                           /* type */
        offsetof(PyPackedFuncBase, handle),    /* offset */
        0,                                     /* flags */
        "Handle to the underlying function"     /* docstring */
    },
    {
        "is_global",                           /* name */
        T_INT,                                 /* type */
        offsetof(PyPackedFuncBase, is_global), /* offset */
        0,                                     /* flags */
        "Whether this is a global function"     /* docstring */
    },
    {NULL}  /* Sentinel */
};

// ObjectBase 的成员定义
static PyMemberDef PyObjectBase_members[] = {
    {
        "handle",                               /* name */
        T_ULONGLONG,                           /* type */
        offsetof(PyObjectBase, handle),         /* offset */
        0,                                     /* flags */
        "Object handle"                         /* docstring */
    },
    {
        "type_code",                           /* name */
        T_INT,                                 /* type */
        offsetof(PyObjectBase, type_code),     /* offset */
        0,                                     /* flags */
        "Type code"                            /* docstring */
    },
    {NULL}  /* Sentinel */
};


// 函数声明
static PyObject* PyPackedFuncBase_new(PyTypeObject* type, PyObject* args, PyObject* kwargs);
static int PyPackedFuncBase_init(PyObject* self, PyObject* args, PyObject* kwargs);
static void PyPackedFuncBase_finalize(PyObject* self);
static PyObject* PyPackedFuncBase_call(PyObject* self, PyObject* args, PyObject* kwargs);

static PyObject* PyObjectBase_new(PyTypeObject* type, PyObject* args, PyObject* kwargs);
static void PyObjectBase_finalize(PyObject* self);
static PyObject* PyObjectBase_init_handle_by_constructor(PyObject* self, PyObject* args);


static PyObject* PyAny_new(PyTypeObject* type, PyObject* args, PyObject* kwargs);
static int PyAny_init(PyObject* self, PyObject* args, PyObject* kwargs);

static int PyObjectToValue(PyObject* arg_0, Value* value);
static PyObject* ValueSwitchToObject(Value* value);
static PyObject* ValueToPyObject(Value* value);


// 对象方法列表
static PyMethodDef PyObjectBase_methods[] = {
    {
        "__init_handle_by_constructor__",
        PyObjectBase_init_handle_by_constructor,
        METH_VARARGS,
        "Initialize the handle by calling constructor function"
    },
    {NULL}
};

// PackedFuncBase 类型对象定义
static PyTypeObject PyType_PackedFuncBase = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "case_ext.PackedFuncBase",          /* tp_name */
    sizeof(PyPackedFuncBase),           /* tp_basicsize */
    0,                                  /* tp_itemsize */
    0,                                  /* tp_dealloc */
    0,                                  /* tp_vectorcall_offset */
    0,                                  /* tp_getattr */
    0,                                  /* tp_setattr */
    0,                                  /* tp_as_async */
    0,                                  /* tp_repr */
    0,                                  /* tp_as_number */
    0,                                  /* tp_as_sequence */
    0,                                  /* tp_as_mapping */
    0,                                  /* tp_hash  */
    PyPackedFuncBase_call,             /* tp_call */
    0,                                  /* tp_str */
    0,                                  /* tp_getattro */
    0,                                  /* tp_setattro */
    0,                                  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_FINALIZE,  /* tp_flags */
    "Function base object",             /* tp_doc */
    0,                                  /* tp_traverse */
    0,                                  /* tp_clear */
    0,                                  /* tp_richcompare */
    0,                                  /* tp_weaklistoffset */
    0,                                  /* tp_iter */
    0,                                  /* tp_iternext */
    0,                                  /* tp_methods */
    PyPackedFuncBase_members,          /* tp_members */
    0,                                  /* tp_getset */
    0,                                  /* tp_base */
    0,                                  /* tp_dict */
    0,                                  /* tp_descr_get */
    0,                                  /* tp_descr_set */
    0,                                  /* tp_dictoffset */
    PyPackedFuncBase_init,             /* tp_init */
    0,                                  /* tp_alloc */
    PyPackedFuncBase_new,              /* tp_new */
    0,                                  /* tp_free */
    0,                                  /* tp_is_gc */
    0,                                  /* tp_bases */
    0,                                  /* tp_mro */
    0,                                  /* tp_cache */
    0,                                  /* tp_subclasses */
    0,                                  /* tp_weaklist */
    0,                                  /* tp_del */
    0,                                  /* tp_version_tag */
    PyPackedFuncBase_finalize,         /* tp_finalize */
};

// ObjectBase 类型对象定义
static PyTypeObject PyType_ObjectBase = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "case_ext.ObjectBase",              /* tp_name */
    sizeof(PyObjectBase),               /* tp_basicsize */
    0,                                  /* tp_itemsize */
    0,                                  /* tp_dealloc */
    0,                                  /* tp_vectorcall_offset */
    0,                                  /* tp_getattr */
    0,                                  /* tp_setattr */
    0,                                  /* tp_as_async */
    0,                                  /* tp_repr */
    0,                                  /* tp_as_number */
    0,                                  /* tp_as_sequence */
    0,                                  /* tp_as_mapping */
    0,                                  /* tp_hash  */
    0,                                  /* tp_call */
    0,                                  /* tp_str */
    0,                                  /* tp_getattro */
    0,                                  /* tp_setattro */
    0,                                  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_FINALIZE,  /* tp_flags */
    "Object base type",                 /* tp_doc */
    0,                                  /* tp_traverse */
    0,                                  /* tp_clear */
    0,                                  /* tp_richcompare */
    0,                                  /* tp_weaklistoffset */
    0,                                  /* tp_iter */
    0,                                  /* tp_iternext */
    PyObjectBase_methods,               /* tp_methods */
    PyObjectBase_members,               /* tp_members */
    0,                                  /* tp_getset */
    0,                                  /* tp_base */
    0,                                  /* tp_dict */
    0,                                  /* tp_descr_get */
    0,                                  /* tp_descr_set */
    0,                                  /* tp_dictoffset */
    0,                                  /* tp_init */
    0,                                  /* tp_alloc */
    PyObjectBase_new,                   /* tp_new */
    0,                                  /* tp_free */
    0,                                  /* tp_is_gc */
    0,                                  /* tp_bases */
    0,                                  /* tp_mro */
    0,                                  /* tp_cache */
    0,                                  /* tp_subclasses */
    0,                                  /* tp_weaklist */
    0,                                  /* tp_del */
    0,                                  /* tp_version_tag */
    PyObjectBase_finalize,              /* tp_finalize */
};

// Any 类型对象定义
static PyTypeObject PyType_Any = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "case_ext.Any",                     /* tp_name */
    sizeof(PyAny),                      /* tp_basicsize */
    0,                                  /* tp_itemsize */
    0,                                  /* tp_dealloc */
    0,                                  /* tp_vectorcall_offset */
    0,                                  /* tp_getattr */
    0,                                  /* tp_setattr */
    0,                                  /* tp_as_async */
    0,                                  /* tp_repr */
    0,                                  /* tp_as_number */
    0,                                  /* tp_as_sequence */
    0,                                  /* tp_as_mapping */
    0,                                  /* tp_hash  */
    0,                                  /* tp_call */
    0,                                  /* tp_str */
    0,                                  /* tp_getattro */
    0,                                  /* tp_setattro */
    0,                                  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,  /* tp_flags */
    "Any value type",                   /* tp_doc */
    0,                                  /* tp_traverse */
    0,                                  /* tp_clear */
    0,                                  /* tp_richcompare */
    0,                                  /* tp_weaklistoffset */
    0,                                  /* tp_iter */
    0,                                  /* tp_iternext */
    0,                                  /* tp_methods */
    0,                                  /* tp_members */
    0,                                  /* tp_getset */
    0,                                  /* tp_base */
    0,                                  /* tp_dict */
    0,                                  /* tp_descr_get */
    0,                                  /* tp_descr_set */
    0,                                  /* tp_dictoffset */
    PyAny_init,                        /* tp_init */
    0,                                  /* tp_alloc */
    PyAny_new,                         /* tp_new */
};

// 清理单个 Value
static void RuntimeValueDestroy(Value* value) {
    if (!value) return;
    
    switch (value->t) {
        case mc::runtime::TypeIndex::Str:  // string type
            if (value->u.v_str) {
                free(value->u.v_str);  // 释放字符串内存
                value->u.v_str = NULL;
            }
            break;
            
        case mc::runtime::TypeIndex::Object:  // object type
            if (value->u.v_pointer) {
                ObjectFree(value->u.v_pointer);  // 释放对象
                value->u.v_pointer = NULL;
            }
            break;
        // TODO   
        case mc::runtime::TypeIndex::Func:  // function type
            if (value->u.v_pointer) {
                FuncFree(value->u.v_pointer);  // 释放函数句柄
                value->u.v_pointer = NULL;
            }
            break;
            
        // 其他类型（int、float 等）不需要特殊清理
        default:
            break;
    }
}

// 清理 Value 数组
static void RuntimeDestroyN(Value* values, int num) {
    if (!values || num <= 0) return;
    
    for (int i = 0; i < num; ++i) {
        RuntimeValueDestroy(&values[i]);
    }
}

static int PyObjectToValue(PyObject* arg_0, Value* value) {
    if (PyFloat_Check(arg_0)) {
        value->t = mc::runtime::TypeIndex::Float;  // float type
        value->u.v_float = PyFloat_AsDouble(arg_0);
    }
    else if (PyLong_Check(arg_0)) {
        value->t = mc::runtime::TypeIndex::Int;  // int type
        value->u.v_int = PyLong_AsLongLong(arg_0);
    }
    else if (PyBool_Check(arg_0)) {
        value->t = mc::runtime::TypeIndex::Int;  // bool as int
        value->u.v_int = (arg_0 == Py_True);
    }
    else if (Py_None == arg_0) {
        value->t = mc::runtime::TypeIndex::Null;  // null 
        value->u.v_pointer = NULL;
    }
    else if (PyUnicode_Check(arg_0)) {
        Py_ssize_t len;
        const char* str = PyUnicode_AsUTF8AndSize(arg_0, &len);
        if (str == NULL) {
            return -1;
        }
        value->t = mc::runtime::TypeIndex::Str;  // string type
        value->u.v_str = strdup(str); 
        value->p = len;  
    }
    else if (PyObject_IsInstance(arg_0, (PyObject*)&PyType_ObjectBase)) {
        PyObjectBase* obj = (PyObjectBase*)arg_0;
        // 只对Object类型进行引用计数，Pointer类型不需要
        if (obj->type_code >= mc::runtime::TypeIndex::Object) {
            if (0 != ObjectRetain(obj->handle)) {
                PyErr_SetString(PyExc_TypeError, "failed to add ref count");
                return -1;
            }
        }
        value->t = obj->type_code;
        value->u.v_pointer = obj->handle;
        value->p = 0;
    }
    // TODO
    else if (PyObject_IsInstance(arg_0, (PyObject*)&PyType_PackedFuncBase)) {
        PyPackedFuncBase* func = (PyPackedFuncBase*)arg_0;
        if (func->is_global == 2) {
            value->t = mc::runtime::TypeIndex::Pointer;  // class pointer type
        } else {
            value->t = mc::runtime::TypeIndex::Func;  // function type (is_global=0/1)
        }
        value->u.v_pointer = func->handle;
        value->p = 0;
    }
    else if (PyObject_IsInstance(arg_0, (PyObject*)&PyType_Any)) {
        PyAny* any = (PyAny*)arg_0;
        *value = any->value;
    }
    else {
        for (int i = 0; i < INPUT_INSTANCE_CALLBACK_CUR; ++i) {
            if (PyObject_IsInstance(arg_0, INPUT_INSTANCE_CALLBACK[i][0])) {
                PyObject* callback_args = PyTuple_Pack(1, arg_0);
                PyObject* result = PyObject_Call(INPUT_INSTANCE_CALLBACK[i][1], callback_args, NULL);
                Py_DECREF(callback_args);
                
                if (result) {
                    if (PyObject_IsInstance(result, (PyObject*)&PyType_Any)) {
                        PyAny* any_value = (PyAny*)(result);
                        *value = any_value->value;
                        Py_DECREF(result);
                        return 0;
                    }
                    Py_DECREF(result);
                }
                break;
            }
        }

        PyObject* type_name = PyObject_Str((PyObject*)Py_TYPE(arg_0));
        PyErr_Format(PyExc_TypeError,
                    "unsupported type '%U'",
                    type_name);
        Py_DECREF(type_name);
        return -1;
    }
    
    return 0;
}
static PyObject* ValueSwitchToObject(Value* value) {
    if (value->t < 0) {
        PyErr_SetString(PyExc_TypeError, "the first argument is not Object pointer");
        return NULL;
    }

    PyObject* index = PyLong_FromLongLong(value->t);
    PyObject* creator = PyDict_GetItem(RETURN_SWITCH, index);
    Py_DECREF(index);

    if (!creator) {
        if (DEFAULT_CLASS_OBJECT) {
            creator = DEFAULT_CLASS_OBJECT;
        } else {
            PyErr_SetString(PyExc_TypeError, "type_code is not registered");
            return NULL;
        }
    }

    // TODO
    if (value->t == mc::runtime::TypeIndex::Module) {
        PyObject* handle = PyLong_FromVoidPtr(value->u.v_pointer);
        PyObject* func_args = PyTuple_Pack(1, handle);
        Py_DECREF(handle);
        PyObject* result = PyObject_Call(creator, func_args, NULL);
        Py_DECREF(func_args);
        return result;

    } else {
        PyObject* func_args = PyTuple_Pack(0);
        PyObject* result = PyObject_Call(creator, func_args, NULL);
        Py_DECREF(func_args);
        
        PyObjectBase* super = (PyObjectBase*)(result);
        super->handle = value->u.v_pointer;
        super->type_code = value->t;

        for (int i = 0; i < OBJECT_CALLBACK_CUR_IDX; ++i) {
            if (OBJECT_CALLBACK_TABLE[i].index == value->t) {
                PyObject* callback_args = PyTuple_Pack(1, result);
                PyObject* ret = PyObject_Call(OBJECT_CALLBACK_TABLE[i].callback, callback_args, NULL);
                Py_DECREF(callback_args);
                Py_DECREF(ret);
                break;
            }
        }
        return result;    
    }
}

static PyObject* ValueToPyObject(Value* value) {
    switch (value->t) {
        case mc::runtime::TypeIndex::Null: {  // null type
            Py_RETURN_NONE;
        }
        
        case mc::runtime::TypeIndex::Int: {  // int type
            return PyLong_FromLongLong(value->u.v_int);
        }
        
        case mc::runtime::TypeIndex::Float: {  // float type
            return PyFloat_FromDouble(value->u.v_float);
        }
        
        case mc::runtime::TypeIndex::Str: {  // string type
            if (value->p >= 0) {
                return PyUnicode_FromStringAndSize(value->u.v_str, value->p);
            }
            return PyUnicode_FromString(value->u.v_str);
        }
        /*
        case mc::runtime::TypeIndex::Func: {  // function type
            PyObject* obj = PyPackedFuncBase_new(&PyType_PackedFuncBase, NULL, NULL);
            if (obj == NULL) {
                return NULL;
            }
            PyPackedFuncBase* func = (PyPackedFuncBase*)obj;
            func->handle = value->u.v_pointer;
            func->is_global = 0;
            return obj;
        }
        case mc::runtime::TypeIndex::DataType : { // DataType
            int size = 64;
            char str[64] = {0};
            if (0 != DataTypeToStr(value->u.v_datatype, str, &size)) {
                PyErr_SetString(PyExc_TypeError, "DataType is not supported");
                return NULL;
            }
            return PyUnicode_FromKindAndData(PyUnicode_1BYTE_KIND, str, size);
        }
        */ 
        case mc::runtime::TypeIndex::Object: {  // object type
            PyObject* obj = PyObjectBase_new(&PyType_ObjectBase, NULL, NULL);
            if (obj == NULL) {
                return NULL;
            }
            PyObjectBase* base = (PyObjectBase*)obj;
            base->handle = value->u.v_pointer;
            base->type_code = value->t;
            return obj;
        }
        
        case mc::runtime::TypeIndex::Pointer: {  // pointer type
            PyObject* obj = PyPackedFuncBase_new(&PyType_PackedFuncBase, NULL, NULL);
            if (obj == NULL) {
                return NULL;
            }
            PyPackedFuncBase* func = (PyPackedFuncBase*)obj;
            func->handle = value->u.v_pointer;
            func->is_global = 2;  // 标记为class pointer
            return obj;
        }
        
        default: {
            if (value->t < 0) {
                PyErr_SetString(PyExc_TypeError, "Unknown value type");
                return NULL;
            }
            return ValueSwitchToObject(value);
        }
    }
}

// PackedFuncBase 方法实现
static PyObject* PyPackedFuncBase_new(PyTypeObject* type, 
                                     PyObject* args, 
                                     PyObject* kwargs) {
    PyPackedFuncBase* self;
    self = (PyPackedFuncBase*)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->handle = NULL;
        self->is_global = 1;
    }
    return (PyObject*)self;
}

static int PyPackedFuncBase_init(PyObject* self, 
                                PyObject* args, 
                                PyObject* kwargs) {
    PyPackedFuncBase* func = (PyPackedFuncBase*)self;
    unsigned long long handle;  // 使用 unsigned long long 来接收指针值
    int is_global = 1;
    
    if (!PyArg_ParseTuple(args, "Ki", &handle, &is_global)) {
        return -1;
    }
    
    func->is_global = is_global;
    func->handle = (void*)handle;
    return 0;
}

static void PyPackedFuncBase_finalize(PyObject* self) {
    PyPackedFuncBase* func = (PyPackedFuncBase*)self;
    if (!func->is_global && func->handle) {
        FuncFree(func->handle);
        func->handle = NULL;
    }
}

static PyObject* PyPackedFuncBase_call(PyObject* self, 
                                      PyObject* args, 
                                      PyObject* kwargs) {
    PyPackedFuncBase* func = (PyPackedFuncBase*)self;
    if (!func->handle) {
        PyErr_SetString(PyExc_RuntimeError, "Function handle is NULL");
        return NULL;
    }
    
    Py_ssize_t size = PyTuple_GET_SIZE(args);
    Value* values = new Value[size];
    PyObject* result = NULL;
    int success_args = 0;
    Value ret_val;

    // 转换参数
    for (Py_ssize_t i = 0; i < size; ++i) {
        PyObject* item = PyTuple_GET_ITEM(args, i);

        if (0 != PyObjectToValue(item, &values[i])) {
            goto FREE_ARGS;
        }
        ++success_args;
    }

    // 调用函数
    if (0 != FuncCall_PYTHON_C_API(func->handle, values, success_args, &ret_val)) {
        PyErr_SetString(PyExc_TypeError, GetError());
        goto FREE_ARGS;
    }
    result = ValueToPyObject(&ret_val);

FREE_ARGS:
    delete[] values;
    return result;
}

// ObjectBase 方法实现
static PyObject* PyObjectBase_new(PyTypeObject* type,
                                 PyObject* args,
                                 PyObject* kwargs) {
    PyObjectBase* self;
    self = (PyObjectBase*)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->handle = NULL;
        self->type_code = 0;
    }
    return (PyObject*)self;
}

static void PyObjectBase_finalize(PyObject* self) {
    PyObjectBase* obj = (PyObjectBase*)self;
    if (obj->handle) {
        ObjectFree(obj->handle);
        obj->handle = NULL;
    }
}

static PyObject* PyObjectBase_init_handle_by_constructor(PyObject* self, PyObject* args) {
    PyObjectBase* super = (PyObjectBase*)self;
    Py_ssize_t size = PyTuple_GET_SIZE(args);
    Value* item_buffer = new Value[size];
    PyObject* item_0 = NULL;
    void* func_addr = NULL;
    int success_args = 0;
    Value ret_val;

    // 在获取任何参数之前检查参数数量
    if (size < 1) {
        PyErr_SetString(PyExc_TypeError, "need one or more args(0 given)");
        goto RETURN_FLAG;
    }

    // 获取并检查第一个参数
    item_0 = PyTuple_GET_ITEM(args, 0);
    if (!PyObject_IsInstance(item_0, (PyObject*)&PyType_PackedFuncBase)) {
        PyErr_SetString(PyExc_TypeError, "the first argument is not PackedFunc type");
        goto RETURN_FLAG;
    }
    func_addr = ((PyPackedFuncBase*)(item_0))->handle;

    // 转换剩余参数
    for (Py_ssize_t i = 1; i < size; ++i) {
        PyObject* item = PyTuple_GET_ITEM(args, i);
        if (0 != PyObjectToValue(item, item_buffer + i - 1)) {
            goto FREE_ARGS;
        }
        ++success_args;
    }

    // 调用构造函数
    if (0 != FuncCall_PYTHON_C_API(func_addr, item_buffer, size - 1, &ret_val)) {
        PyErr_SetString(PyExc_TypeError, GetError());
        goto FREE_ARGS;
    }

    // 检查返回值类型
    if (ret_val.t < 0) {
        PyErr_SetString(PyExc_TypeError, "the return value is not ObjectBase Type");
        goto FREE_ARGS;
    }

    // 设置对象属性
    super->handle = ret_val.u.v_pointer;
    super->type_code = ret_val.t;

FREE_ARGS:
    // 清理已转换的参数
    RuntimeDestroyN(item_buffer, success_args);

RETURN_FLAG:
    delete[] item_buffer;
    Py_RETURN_NONE;
}

// Any 方法实现
static PyObject* PyAny_new(PyTypeObject* type,
                          PyObject* args,
                          PyObject* kwargs) {
    PyAny* self;
    self = (PyAny*)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->value.u.v_pointer = NULL;
        self->value.p = 0;
        self->value.t = 0;
    }
    return (PyObject*)self;
}

static int PyAny_init(PyObject* self,
                     PyObject* args,
                     PyObject* kwargs) {
    PyAny* any = (PyAny*)self;
    PyObject* value;
    
    if (!PyArg_ParseTuple(args, "O", &value)) {
        return -1;
    }
    return PyObjectToValue(value, &any->value);
}

// 设置创建器的函数
static PyObject* set_packedfunc_creator(PyObject* self, PyObject* args) {
    PyObject* func;
    if (!PyArg_ParseTuple(args, "O", &func)) {
        return NULL;
    }
    if (!PyCallable_Check(func)) {
        PyErr_SetString(PyExc_TypeError, "argument must be callable");
        return NULL;
    }
    
    if (PACKEDFUNC_CREATOR) {
        Py_DECREF(PACKEDFUNC_CREATOR);
    }
    Py_INCREF(func);
    PACKEDFUNC_CREATOR = func;
    
    Py_RETURN_NONE;
}

// 转换函数
static PyObject* ValueSwitchToPackedFunc(Value* value) {
    if (!PACKEDFUNC_CREATOR) {
        PyErr_SetString(PyExc_TypeError, "PackedFunc type_code is not registered");
        return NULL;
    }
    PyObject* handle = PyLong_FromVoidPtr(value->u.v_pointer);
    PyObject* func_args = PyTuple_Pack(1, handle);
    Py_DECREF(handle);
    PyObject* result = PyObject_Call(PACKEDFUNC_CREATOR, func_args, NULL);
    Py_DECREF(func_args);

    return result;
}

// 获取全局函数的实现
static PyObject* get_global_func(PyObject* self, PyObject* args) {
    const char* name = NULL;
    PyObject* allow_missing = NULL;

    if (!PyArg_ParseTuple(args, "sO", &name, &allow_missing)) {
        return NULL;
    }
    if (!PyBool_Check(allow_missing)) {
        PyErr_SetString(PyExc_TypeError, "allow_missing is not bool type");
        return NULL;
    }

    FunctionHandle handle;
    if (GetGlobal(name, &handle)) {
        PyErr_SetString(PyExc_RuntimeError, "failed to call GetGlobal");
        return NULL;
    }

    if (handle) {
        Value value;
        value.t = mc::runtime::TypeIndex::Func;  // function type
        value.u.v_pointer = handle;
        value.p = 0;
        return ValueSwitchToPackedFunc(&value);
    }

    Py_RETURN_NONE;
}

// 注册对象的函数
static PyObject* register_object(PyObject* self, PyObject* args) {
    long long index = 0;
    PyObject* creator;

    if (!PyArg_ParseTuple(args, "LO", &index, &creator)) {
        return NULL;
    }
    if (!PyCallable_Check(creator)) {
        PyErr_SetString(PyExc_TypeError,
                    "the second arg is not a PyType object or a callable function");
        return NULL;
    }

    // 延迟初始化字典
    if (RETURN_SWITCH == NULL) {
        RETURN_SWITCH = PyDict_New();
        if (RETURN_SWITCH == NULL) {
            return NULL;
        }
    }

    Py_INCREF(creator);
    PyObject* index_obj = PyLong_FromLongLong(index);
    if (0 != PyDict_SetItem(RETURN_SWITCH, index_obj, creator)) {
        Py_DECREF(index_obj);
        Py_DECREF(creator);
        return NULL;
    }
    Py_DECREF(index_obj);
    
    Py_RETURN_NONE;
}

// 注册对象回调的函数
static PyObject* register_object_callback(PyObject* self, PyObject* args) {
    long long index = 0;
    PyObject* callback;

    if (!PyArg_ParseTuple(args, "LO", &index, &callback)) {
        return NULL;
    }
    if (!PyCallable_Check(callback)) {
        PyErr_SetString(PyExc_TypeError, "the second arg is not a callable object");
        return NULL;
    }
    if (OBJECT_CALLBACK_CUR_IDX >= MAX_OBJECT_CALLBACK_NUM) {
        PyErr_SetString(PyExc_TypeError, "callback register overflow");
        return NULL;
    }
    Py_INCREF(callback);

    // 如果当前位置已有回调，先清理掉
    if (OBJECT_CALLBACK_TABLE[OBJECT_CALLBACK_CUR_IDX].callback) {
        Py_DECREF(OBJECT_CALLBACK_TABLE[OBJECT_CALLBACK_CUR_IDX].callback);
    }
    
    OBJECT_CALLBACK_TABLE[OBJECT_CALLBACK_CUR_IDX].index = index;
    OBJECT_CALLBACK_TABLE[OBJECT_CALLBACK_CUR_IDX].callback = callback;
    ++OBJECT_CALLBACK_CUR_IDX;

    Py_RETURN_NONE;
}

// 设置默认类对象创建器
static PyObject* set_class_object(PyObject* self, PyObject* args) {
    PyObject* callable;

    if (!PyArg_ParseTuple(args, "O", &callable)) {
        return NULL;
    }
    if (!PyCallable_Check(callable)) {
        PyErr_SetString(PyExc_TypeError, "the arg is not a callable object");
        return NULL;
    }
    
    // 如果已存在，释放旧的
    if (DEFAULT_CLASS_OBJECT) {
        Py_DECREF(DEFAULT_CLASS_OBJECT);
    }
    
    // 设置新的并增加引用计数
    Py_INCREF(callable);
    DEFAULT_CLASS_OBJECT = callable;
    
    Py_RETURN_NONE;
}

static PyObject* make_any(PyObject* self, PyObject* args) {
    int32_t type_code;
    int32_t pad;
    uintptr_t handle;
    int32_t move_mode;
    
    if (!PyArg_ParseTuple(args, "iiKi", &type_code, &pad, &handle, &move_mode)) {
        return NULL;
    }
    
    PyObject* obj = PyAny_new(&PyType_Any, NULL, NULL);
    if (obj == NULL) {
        return NULL;
    }
    PyAny* any = (PyAny*)obj;
    any->value.t = type_code;
    any->value.p = pad;
    any->value.u.v_pointer = (void*)(handle);
    
    if (!move_mode) {
        if (0 != ObjectRetain(any->value.u.v_pointer)) {
            Py_DECREF(obj);
            PyErr_SetString(PyExc_TypeError, "failed to add ref count");
            return NULL;
        }
    }
    
    return obj;
}

static PyObject* register_input_instance_callback(PyObject* self, PyObject* args) {
    PyObject* user_type_object;
    PyObject* user_callback;
    
    if (!PyArg_ParseTuple(args, "OO", &user_type_object, &user_callback)) {
        return NULL;
    }
    
    if (!PyCallable_Check(user_callback)) {
        PyErr_SetString(PyExc_TypeError, "the second argument is not callable type");
        return NULL;
    }
    
    // 检查是否已存在该类型的回调
    for (int i = 0; i < INPUT_INSTANCE_CALLBACK_CUR; ++i) {
        if (user_type_object == INPUT_INSTANCE_CALLBACK[i][0]) {
            Py_DECREF(INPUT_INSTANCE_CALLBACK[i][1]);
            Py_INCREF(user_callback);
            INPUT_INSTANCE_CALLBACK[i][1] = user_callback;
            Py_RETURN_NONE;
        }
    }
    
    if (INPUT_INSTANCE_CALLBACK_CUR >= MAX_INPUT_INSTANCE_CALLBACK_NUM) {
        PyErr_SetString(PyExc_TypeError, "too many instance callbacks");
        return NULL;
    }
    
    Py_INCREF(user_type_object);
    Py_INCREF(user_callback);
    INPUT_INSTANCE_CALLBACK[INPUT_INSTANCE_CALLBACK_CUR][0] = user_type_object;
    INPUT_INSTANCE_CALLBACK[INPUT_INSTANCE_CALLBACK_CUR][1] = user_callback;
    ++INPUT_INSTANCE_CALLBACK_CUR;
    
    Py_RETURN_NONE;
}

// 模块方法定义
static PyMethodDef CaseExtMethods[] = {
    {"GetGlobal", get_global_func, METH_VARARGS, "Get global function by name"},
    {"set_packedfunc_creator", set_packedfunc_creator, METH_VARARGS, "Set PackedFunc creator"},
    {"register_object", register_object, METH_VARARGS, "Register object creator"},
    {"register_object_callback", register_object_callback, METH_VARARGS, "Register object callback"},
    {"set_class_object", set_class_object, METH_VARARGS, "Set default class object creator"},
    {"make_any", make_any, METH_VARARGS, "make any by type_code and pointer"},
    {"register_input_callback", register_input_instance_callback, METH_VARARGS, "register callback"},
    {NULL, NULL, 0, NULL}  // Sentinel
};

// 模块定义
static struct PyModuleDef CaseExtModule = {
    PyModuleDef_HEAD_INIT,
    "case_ext",           /* 模块名 */
    "Case extension module",  /* 模块文档 */
    -1,      /* size of per-interpreter state of the module,
                or -1 if the module keeps state in global variables. */
    CaseExtMethods
};

// 模块初始化函数
PyMODINIT_FUNC PyInit_case_ext(void) {
    // 初始化全局变量
    if (RETURN_SWITCH == NULL) {
        RETURN_SWITCH = PyDict_New();
        if (RETURN_SWITCH == NULL) {
            return NULL;
        }
    }
    
    // 初始化回调表
    memset(OBJECT_CALLBACK_TABLE, 0, sizeof(OBJECT_CALLBACK_TABLE));
    OBJECT_CALLBACK_CUR_IDX = 0;

    // 初始化输入回调表
    for (int i = 0; i < MAX_INPUT_INSTANCE_CALLBACK_NUM; ++i) {
        INPUT_INSTANCE_CALLBACK[i][0] = NULL;
        INPUT_INSTANCE_CALLBACK[i][1] = NULL;
    }
    INPUT_INSTANCE_CALLBACK_CUR = 0;

    PyObject* m;

    // 初始化类型
    if (PyType_Ready(&PyType_PackedFuncBase) < 0)
        return NULL;
    if (PyType_Ready(&PyType_ObjectBase) < 0)
        return NULL;
    if (PyType_Ready(&PyType_Any) < 0)
        return NULL;

    // 创建模块
    m = PyModule_Create(&CaseExtModule);
    if (m == NULL)
        return NULL;

    // 添加类型到模块
    Py_INCREF(&PyType_PackedFuncBase);
    Py_INCREF(&PyType_ObjectBase);
    Py_INCREF(&PyType_Any);
    
    if (PyModule_AddObject(m, "PackedFuncBase", (PyObject*)&PyType_PackedFuncBase) < 0) {
        Py_DECREF(&PyType_PackedFuncBase);
        Py_DECREF(m);
        return NULL;
    }
    
    if (PyModule_AddObject(m, "ObjectBase", (PyObject*)&PyType_ObjectBase) < 0) {
        Py_DECREF(&PyType_ObjectBase);
        Py_DECREF(m);
        return NULL;
    }
    
    if (PyModule_AddObject(m, "Any", (PyObject*)&PyType_Any) < 0) {
        Py_DECREF(&PyType_Any);
        Py_DECREF(m);
        return NULL;
    }

    return m;
}
