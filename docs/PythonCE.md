# Python C Extension 开发教程

本教程将系统性地介绍如何开发Python C Extension模块，从基础概念到高级特性，帮助你构建高性能、类型安全的Python扩展。教程中的示例基于实际项目经验，具有很强的实用性。

## 目录

1. [Python C Extension基础](#1-python-c-extension基础)
2. [简单扩展模块入门](#2-简单扩展模块入门)
3. [自定义Python对象类型](#3-自定义python对象类型)
4. [参数解析和类型转换](#4-参数解析和类型转换)
5. [内存管理和异常处理](#5-内存管理和异常处理)
6. [回调机制和高级特性](#6-回调机制和高级特性)
7. [编译、调试和测试](#7-编译调试和测试)
8. [性能优化和最佳实践](#8-性能优化和最佳实践)
9. [实际案例：构建FFI系统](#9-实际案例构建ffi系统)

## 1. Python C Extension基础

### 1.1 什么是Python C Extension

Python C Extension是用C/C++编写的Python模块，它具有以下优势：

- **性能优势**：C/C++代码执行速度快，适合计算密集型任务
- **系统集成**：可以调用系统API和第三方C/C++库
- **内存效率**：直接管理内存，避免Python GC开销
- **现有代码复用**：将现有C/C++代码封装为Python接口

### 1.2 开发环境准备

#### 必要的工具和头文件

```bash
# Ubuntu/Debian
sudo apt-get install python3-dev build-essential

# CentOS/RHEL
sudo yum install python3-devel gcc gcc-c++

# macOS
xcode-select --install
brew install python
```

#### 基本头文件

```cpp
#include <Python.h>           // Python C API，必须第一个包含
#include <structmember.h>     // 用于定义对象成员访问
```

**重要**：`Python.h`必须在其他头文件之前包含，因为它可能重新定义某些宏。

### 1.3 基本概念

#### PyObject - Python对象的C表示

```cpp
typedef struct _object {
    _PyObject_HEAD_EXTRA    // 调试信息（发布版本为空）
    Py_ssize_t ob_refcnt;   // 引用计数
    struct _typeobject *ob_type; // 类型对象指针
} PyObject;
```

#### 引用计数管理

```cpp
Py_INCREF(obj);    // 增加引用计数
Py_DECREF(obj);    // 减少引用计数，可能释放对象
Py_XINCREF(obj);   // 安全版本，检查obj是否为NULL
Py_XDECREF(obj);   // 安全版本，检查obj是否为NULL
```

#### 错误处理

Python C API使用异常标志来处理错误：

```cpp
// 设置异常
PyErr_SetString(PyExc_ValueError, "Invalid argument");
return NULL;  // 表示函数失败

// 检查异常
if (PyErr_Occurred()) {
    // 处理异常
    return NULL;
}

// 清除异常
PyErr_Clear();
```

## 2. 简单扩展模块入门

### 2.1 Hello World扩展

让我们从最简单的例子开始：

```cpp
#include <Python.h>

// hello.c - 最简单的扩展模块
static PyObject* hello_world(PyObject* self, PyObject* args) {
    return PyUnicode_FromString("Hello, World from C!");
}

// 方法定义表
static PyMethodDef HelloMethods[] = {
    {"hello", hello_world, METH_NOARGS, "Return a greeting"},
    {NULL, NULL, 0, NULL}  // 结束标记
};

// 模块定义
static struct PyModuleDef hellomodule = {
    PyModuleDef_HEAD_INIT,
    "hello",               // 模块名
    "Hello world module",  // 模块文档
    -1,                    // 全局状态大小
    HelloMethods
};

// 模块初始化函数
PyMODINIT_FUNC PyInit_hello(void) {
    return PyModule_Create(&hellomodule);
}
```

### 2.2 编译和测试

使用setuptools编译扩展：

```python
# setup.py
from setuptools import setup, Extension

module = Extension('hello',
                   sources=['hello.c'])

setup(name='HelloWorld',
      version='1.0',
      description='Hello world C extension',
      ext_modules=[module])
```

编译并测试：

```bash
python setup.py build_ext --inplace
python -c "import hello; print(hello.hello())"
```

### 2.3 METH_VARARGS：接受参数的函数

```cpp
static PyObject* add_numbers(PyObject* self, PyObject* args) {
    int a, b;
    
    // 解析参数
    if (!PyArg_ParseTuple(args, "ii", &a, &b)) {
        return NULL;  // 参数解析失败
    }
    
    return PyLong_FromLong(a + b);
}

static PyMethodDef MathMethods[] = {
    {"add", add_numbers, METH_VARARGS, "Add two integers"},
    {NULL, NULL, 0, NULL}
};
```

## 3. 自定义Python对象类型

### 3.1 定义自定义对象结构

创建自定义Python对象类型是扩展的强大功能。让我们创建一个简单的Point类：

```cpp
#include <Python.h>
#include <structmember.h>

// 定义Point对象结构
typedef struct {
    PyObject_HEAD    // 必须是第一个成员
    double x;        // x坐标
    double y;        // y坐标
} PointObject;

// 定义可访问的成员
static PyMemberDef Point_members[] = {
    {"x", T_DOUBLE, offsetof(PointObject, x), 0, "x coordinate"},
    {"y", T_DOUBLE, offsetof(PointObject, y), 0, "y coordinate"},
    {NULL}  // 哨兵
};
```

**成员类型说明：**
- `T_INT` - int
- `T_LONG` - long  
- `T_DOUBLE` - double
- `T_STRING` - char*
- `T_OBJECT` - PyObject*
- `T_BOOL` - char (作为布尔值)

### 3.2 对象方法实现

#### 对象创建和初始化

```cpp
// 对象创建函数
static PyObject* Point_new(PyTypeObject* type, PyObject* args, PyObject* kwds) {
    PointObject* self;
    
    self = (PointObject*)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->x = 0.0;  // 初始化默认值
        self->y = 0.0;
    }
    
    return (PyObject*)self;
}

// 对象初始化函数
static int Point_init(PointObject* self, PyObject* args, PyObject* kwds) {
    double x = 0.0, y = 0.0;
    static char* kwlist[] = {"x", "y", NULL};
    
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|dd", kwlist, &x, &y)) {
        return -1;
    }
    
    self->x = x;
    self->y = y;
    return 0;
}

// 对象销毁函数
static void Point_dealloc(PointObject* self) {
    // 如果有需要释放的资源，在这里释放
    Py_TYPE(self)->tp_free((PyObject*)self);
}
```

#### 字符串表示

```cpp
// __str__ 方法
static PyObject* Point_str(PointObject* self) {
    return PyUnicode_FromFormat("Point(%.2f, %.2f)", self->x, self->y);
}

// __repr__ 方法  
static PyObject* Point_repr(PointObject* self) {
    return PyUnicode_FromFormat("Point(x=%.2f, y=%.2f)", self->x, self->y);
}
```

#### 自定义方法

```cpp
// 计算距离原点的距离
static PyObject* Point_distance_from_origin(PointObject* self, PyObject* Py_UNUSED(ignored)) {
    double distance = sqrt(self->x * self->x + self->y * self->y);
    return PyFloat_FromDouble(distance);
}

// 计算两点间距离
static PyObject* Point_distance_to(PointObject* self, PyObject* args) {
    PointObject* other;
    
    if (!PyArg_ParseTuple(args, "O!", &PointType, &other)) {
        return NULL;
    }
    
    double dx = self->x - other->x;
    double dy = self->y - other->y;
    double distance = sqrt(dx * dx + dy * dy);
    
    return PyFloat_FromDouble(distance);
}

// 移动点
static PyObject* Point_move(PointObject* self, PyObject* args) {
    double dx, dy;
    
    if (!PyArg_ParseTuple(args, "dd", &dx, &dy)) {
        return NULL;
    }
    
    self->x += dx;
    self->y += dy;
    
    Py_RETURN_NONE;
}

// 方法定义表
static PyMethodDef Point_methods[] = {
    {"distance_from_origin", (PyCFunction)Point_distance_from_origin, METH_NOARGS,
     "Calculate distance from origin"},
    {"distance_to", (PyCFunction)Point_distance_to, METH_VARARGS,
     "Calculate distance to another point"},
    {"move", (PyCFunction)Point_move, METH_VARARGS,
     "Move point by dx, dy"},
    {NULL}  // 哨兵
};
```

### 3.3 数值操作支持

让Point支持数学运算：

```cpp
// 加法运算：point1 + point2
static PyObject* Point_add(PyObject* left, PyObject* right) {
    PointObject* p1, * p2;
    
    if (!PyObject_IsInstance(left, (PyObject*)&PointType) ||
        !PyObject_IsInstance(right, (PyObject*)&PointType)) {
        Py_RETURN_NOTIMPLEMENTED;
    }
    
    p1 = (PointObject*)left;
    p2 = (PointObject*)right;
    
    return Point_create(p1->x + p2->x, p1->y + p2->y);
}

// 乘法运算：point * scalar
static PyObject* Point_multiply(PyObject* left, PyObject* right) {
    PointObject* point;
    double scalar;
    
    // 检查操作数类型
    if (PyObject_IsInstance(left, (PyObject*)&PointType) && PyFloat_Check(right)) {
        point = (PointObject*)left;
        scalar = PyFloat_AsDouble(right);
    } else if (PyFloat_Check(left) && PyObject_IsInstance(right, (PyObject*)&PointType)) {
        scalar = PyFloat_AsDouble(left);
        point = (PointObject*)right;
    } else {
        Py_RETURN_NOTIMPLEMENTED;
    }
    
    return Point_create(point->x * scalar, point->y * scalar);
}

// 数值方法表
static PyNumberMethods Point_as_number = {
    .nb_add = Point_add,
    .nb_multiply = Point_multiply,
    // 可以添加更多运算符重载
};
```

### 3.4 完整的类型对象定义

```cpp
static PyTypeObject PointType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "point.Point",
    .tp_doc = "Point objects",
    .tp_basicsize = sizeof(PointObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = Point_new,
    .tp_init = (initproc)Point_init,
    .tp_dealloc = (destructor)Point_dealloc,
    .tp_repr = (reprfunc)Point_repr,
    .tp_str = (reprfunc)Point_str,
    .tp_members = Point_members,
    .tp_methods = Point_methods,
    .tp_as_number = &Point_as_number,
};

// 辅助函数：创建Point对象
static PyObject* Point_create(double x, double y) {
    PointObject* point = (PointObject*)PointType.tp_alloc(&PointType, 0);
    if (point) {
        point->x = x;
        point->y = y;
    }
    return (PyObject*)point;
}
```

### 3.5 继承和子类化

让Point支持被继承：

```cpp
// 在tp_flags中添加Py_TPFLAGS_BASETYPE标志
.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,

// 如果需要更复杂的继承控制
static PyObject* Point_subclass_new(PyTypeObject* type, PyObject* args, PyObject* kwds) {
    PyObject* obj;
    
    // 调用基类的tp_new
    obj = PointType.tp_new(type, args, kwds);
    if (obj == NULL) {
        return NULL;
    }
    
    // 子类特定的初始化
    // ...
    
    return obj;
}
```

### 3.6 属性访问控制

实现`__getattr__`和`__setattr__`：

```cpp
// 自定义属性获取
static PyObject* Point_getattro(PointObject* self, PyObject* name) {
    // 检查是否是特殊属性
    if (PyUnicode_CompareWithASCIIString(name, "magnitude") == 0) {
        double mag = sqrt(self->x * self->x + self->y * self->y);
        return PyFloat_FromDouble(mag);
    }
    
    // 默认属性访问
    return PyObject_GenericGetAttr((PyObject*)self, name);
}

// 自定义属性设置
static int Point_setattro(PointObject* self, PyObject* name, PyObject* value) {
    // 禁止设置某些属性
    if (PyUnicode_CompareWithASCIIString(name, "magnitude") == 0) {
        PyErr_SetString(PyExc_AttributeError, "can't set magnitude");
        return -1;
    }
    
    // 默认属性设置
    return PyObject_GenericSetAttr((PyObject*)self, name, value);
}

// 在类型对象中设置
.tp_getattro = (getattrofunc)Point_getattro,
.tp_setattro = (setattrofunc)Point_setattro,
```

### 3.8 注册类型到模块

```cpp
static struct PyModuleDef geometrymodule = {
    PyModuleDef_HEAD_INIT,
    "geometry",
    "Geometry module with Point class",
    -1,
    NULL  // 没有模块级别函数
};

PyMODINIT_FUNC PyInit_geometry(void) {
    PyObject* m;
    
    // 准备类型对象
    if (PyType_Ready(&PointType) < 0) {
        return NULL;
    }
    
    // 创建模块
    m = PyModule_Create(&geometrymodule);
    if (m == NULL) {
        return NULL;
    }
    
    // 增加类型引用计数并添加到模块
    Py_INCREF(&PointType);
    if (PyModule_AddObject(m, "Point", (PyObject*)&PointType) < 0) {
        Py_DECREF(&PointType);
        Py_DECREF(m);
        return NULL;
    }
    
    return m;
}
```

## 4. 参数解析和类型转换

### 4.1 PyArg_ParseTuple详解

PyArg_ParseTuple是参数解析的核心函数，支持多种格式字符串：

```cpp
// 基本类型格式符
static PyObject* demo_types(PyObject* self, PyObject* args) {
    int i;           // "i" - 整数
    double f;        // "f" 或 "d" - 浮点数
    char* s;         // "s" - 字符串
    PyObject* obj;   // "O" - 任意Python对象
    
    if (!PyArg_ParseTuple(args, "ifsO", &i, &f, &s, &obj)) {
        return NULL;
    }
    
    printf("int: %d, float: %f, string: %s\n", i, f, s);
    
    Py_RETURN_NONE;
```

### 4.2 高级参数解析

```cpp
// 可选参数和关键字参数
static PyObject* advanced_parse(PyObject* self, PyObject* args, PyObject* kwargs) {
    int required_int;
    double optional_float = 1.0;  // 默认值
    char* optional_string = "default";
    
    static char* kwlist[] = {"num", "factor", "name", NULL};
    
    // "|" 表示后面的参数是可选的
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "i|ds", kwlist,
                                     &required_int, &optional_float, 
                                     &optional_string)) {
        return NULL;
    }
    
    return PyUnicode_FromFormat("num=%d, factor=%.2f, name=%s", 
                               required_int, optional_float, optional_string);
}
```

### 4.3 类型检查和转换

```cpp
static PyObject* type_checking(PyObject* self, PyObject* args) {
    PyObject* obj;
    
    if (!PyArg_ParseTuple(args, "O", &obj)) {
        return NULL;
    }
    
    // 类型检查
    if (PyLong_Check(obj)) {
        long value = PyLong_AsLong(obj);
        return PyUnicode_FromFormat("Integer: %ld", value);
    }
    else if (PyFloat_Check(obj)) {
        double value = PyFloat_AsDouble(obj);
        return PyUnicode_FromFormat("Float: %.2f", value);
    }
    else if (PyUnicode_Check(obj)) {
        const char* value = PyUnicode_AsUTF8(obj);
        return PyUnicode_FromFormat("String: %s", value);
    }
    else if (PyList_Check(obj)) {
        Py_ssize_t size = PyList_Size(obj);
        return PyUnicode_FromFormat("List with %zd items", size);
    }
    
    return PyUnicode_FromString("Unknown type");
}
```

### 4.4 列表和字典处理

```cpp
static PyObject* process_list(PyObject* self, PyObject* args) {
    PyObject* list;
    
    if (!PyArg_ParseTuple(args, "O!", &PyList_Type, &list)) {
        return NULL;
    }
    
    Py_ssize_t size = PyList_Size(list);
    double sum = 0.0;
    
    for (Py_ssize_t i = 0; i < size; i++) {
        PyObject* item = PyList_GetItem(list, i);  // 借用引用
        
        if (PyFloat_Check(item)) {
            sum += PyFloat_AsDouble(item);
        } else if (PyLong_Check(item)) {
            sum += PyLong_AsDouble(item);
        } else {
            PyErr_SetString(PyExc_TypeError, "List must contain numbers");
            return NULL;
        }
    }
    
    return PyFloat_FromDouble(sum);
```

## 5. 内存管理和异常处理

### 5.1 引用计数规则

#### 引用计数的基本概念

```cpp
// 错误的内存管理示例
static PyObject* bad_memory_management(PyObject* self, PyObject* args) {
    PyObject* str = PyUnicode_FromString("Hello");
    
    // 错误：没有处理引用计数
    return str;  // 可能导致内存泄漏或崩溃
}

// 正确的内存管理
static PyObject* good_memory_management(PyObject* self, PyObject* args) {
    PyObject* str = PyUnicode_FromString("Hello");  // 新引用
    
    if (str == NULL) {
        return NULL;  // 内存分配失败
    }
    
    // 直接返回新引用给调用者
    return str;  // 调用者负责减少引用计数
}
```

#### 借用引用 vs 新引用

```cpp
static PyObject* reference_examples(PyObject* self, PyObject* args) {
    PyObject* list;
    
    if (!PyArg_ParseTuple(args, "O!", &PyList_Type, &list)) {
        return NULL;
    }
    
    // PyList_GetItem返回借用引用
    PyObject* item = PyList_GetItem(list, 0);  // 借用引用
    if (item == NULL) {
        return NULL;
    }
    
    // 如果要保存item的引用，必须增加引用计数
    Py_INCREF(item);
    
    // 创建新的列表
    PyObject* new_list = PyList_New(1);  // 新引用
    if (new_list == NULL) {
        Py_DECREF(item);  // 释放之前增加的引用
        return NULL;
    }
    
    // PyList_SetItem会窃取引用（steal reference）
    PyList_SetItem(new_list, 0, item);  // item的引用被转移
    
    return new_list;  // 返回新引用
}
```

### 5.2 异常处理最佳实践

#### 设置和检查异常

```cpp
static PyObject* division_example(PyObject* self, PyObject* args) {
    double a, b;
    
    if (!PyArg_ParseTuple(args, "dd", &a, &b)) {
        return NULL;  // PyArg_ParseTuple已经设置了异常
    }
    
    if (b == 0.0) {
        PyErr_SetString(PyExc_ZeroDivisionError, "Cannot divide by zero");
        return NULL;
    }
    
    return PyFloat_FromDouble(a / b);
}
```

#### 异常链和上下文

```cpp
static PyObject* complex_operation(PyObject* self, PyObject* args) {
    PyObject* file_path;
    
    if (!PyArg_ParseTuple(args, "U", &file_path)) {
        return NULL;
    }
    
    // 尝试打开文件
    PyObject* file = PyObject_CallMethod(
        PyImport_ImportModule("builtins"), "open", "Os", file_path, "r");
    
    if (file == NULL) {
        // 异常已经被设置，添加上下文信息
        if (PyErr_Occurred()) {
            PyErr_SetString(PyExc_RuntimeError, "Failed to process file");
        }
        return NULL;
    }
    
    // 使用文件...
    Py_DECREF(file);
    
    Py_RETURN_NONE;
}
```

### 5.3 资源管理模式

#### RAII风格的资源管理

```cpp
typedef struct {
    PyObject_HEAD
    FILE* file;
    char* filename;
} FileObject;

static int File_init(FileObject* self, PyObject* args, PyObject* kwargs) {
    char* filename;
    char* mode = "r";
    
    static char* kwlist[] = {"filename", "mode", NULL};
    
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|s", kwlist,
                                     &filename, &mode)) {
        return -1;
    }
    
    self->file = fopen(filename, mode);
    if (self->file == NULL) {
        PyErr_SetFromErrnoWithFilename(PyExc_OSError, filename);
        return -1;
    }
    
    // 保存文件名副本
    self->filename = strdup(filename);
    if (self->filename == NULL) {
        fclose(self->file);
        self->file = NULL;
        PyErr_NoMemory();
        return -1;
    }
    
    return 0;
}

static void File_dealloc(FileObject* self) {
    if (self->file) {
        fclose(self->file);
    }
    if (self->filename) {
        free(self->filename);
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
```

## 6. 回调机制和高级特性

### 6.1 Python回调函数

```cpp
// 全局回调函数存储
static PyObject* g_callback = NULL;

static PyObject* set_callback(PyObject* self, PyObject* args) {
    PyObject* callback;
    
    if (!PyArg_ParseTuple(args, "O", &callback)) {
        return NULL;
    }
    
    // 检查是否可调用
    if (!PyCallable_Check(callback)) {
        PyErr_SetString(PyExc_TypeError, "Object must be callable");
        return NULL;
    }
    
    // 更新回调函数
    Py_XDECREF(g_callback);  // 释放旧的回调
    Py_INCREF(callback);     // 增加新回调的引用
    g_callback = callback;
    
    Py_RETURN_NONE;
}

static PyObject* trigger_callback(PyObject* self, PyObject* args) {
    int value;
    
    if (!PyArg_ParseTuple(args, "i", &value)) {
        return NULL;
    }
    
    if (g_callback == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "No callback set");
        return NULL;
    }
    
    // 调用Python回调函数
    PyObject* result = PyObject_CallFunction(g_callback, "i", value);
    
    if (result == NULL) {
        return NULL;  // 回调函数抛出了异常
    }
    
    // 返回回调的结果
    return result;
}
```

### 6.2 线程和GIL

#### 释放GIL进行耗时操作

```cpp
static PyObject* cpu_intensive_task(PyObject* self, PyObject* args) {
    int iterations;
    
    if (!PyArg_ParseTuple(args, "i", &iterations)) {
        return NULL;
    }
    
    double result = 0.0;
    
    // 释放GIL，允许其他Python线程运行
    Py_BEGIN_ALLOW_THREADS
    
    for (int i = 0; i < iterations; i++) {
        result += sin(i) * cos(i);  // 模拟CPU密集型计算
    }
    
    // 重新获取GIL
    Py_END_ALLOW_THREADS
    
    return PyFloat_FromDouble(result);
}
```

#### 在C线程中调用Python代码

```cpp
#include <pthread.h>

typedef struct {
    PyObject* callback;
    int value;
    PyObject* result;
} ThreadData;

void* worker_thread(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    
    // 获取GIL
    PyGILState_STATE gstate = PyGILState_Ensure();
    
    // 在子线程中调用Python函数
    data->result = PyObject_CallFunction(data->callback, "i", data->value);
    
    // 释放GIL
    PyGILState_Release(gstate);
    
    return NULL;
}

static PyObject* async_callback(PyObject* self, PyObject* args) {
    PyObject* callback;
    int value;
    
    if (!PyArg_ParseTuple(args, "Oi", &callback, &value)) {
        return NULL;
    }
    
    if (!PyCallable_Check(callback)) {
        PyErr_SetString(PyExc_TypeError, "First argument must be callable");
        return NULL;
    }
    
    ThreadData* data = malloc(sizeof(ThreadData));
    data->callback = callback;
    data->value = value;
    data->result = NULL;
    
    Py_INCREF(callback);  // 保护回调函数
    
    pthread_t thread;
    pthread_create(&thread, NULL, worker_thread, data);
    
    // 释放GIL等待线程完成
    Py_BEGIN_ALLOW_THREADS
    pthread_join(thread, NULL);
    Py_END_ALLOW_THREADS
    
    PyObject* result = data->result;
    Py_DECREF(callback);
    free(data);
    
    return result;
```

## 7. 编译、调试和测试

### 7.1 setup.py进阶用法

```python
# setup.py - 更复杂的配置
from setuptools import setup, Extension
import pybind11

# 定义扩展模块
extension = Extension(
    'mymodule',
    sources=[
        'src/module.c',
        'src/geometry.c',
        'src/utils.c',
    ],
    include_dirs=[
        'include',
        '/usr/local/include',
        pybind11.get_cmake_dir() + '/../../../include',
    ],
    libraries=['math', 'm'],
    library_dirs=['/usr/local/lib'],
    define_macros=[
        ('VERSION_INFO', '"dev"'),
        ('DEBUG', '1'),
    ],
    extra_compile_args=['-std=c99', '-Wall', '-O3'],
    extra_link_args=['-lm'],
)

setup(
    name='MyGeometryModule',
    version='1.0.0',
    author='Your Name',
    author_email='your.email@example.com',
    description='A geometry calculation module',
    long_description=open('README.md').read(),
    long_description_content_type='text/markdown',
    ext_modules=[extension],
    python_requires='>=3.6',
    classifiers=[
        'Development Status :: 5 - Production/Stable',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: MIT License',
        'Programming Language :: Python :: 3',
        'Programming Language :: C',
    ],
)
```

### 7.2 调试技巧

#### 使用gdb调试

```bash
# 编译调试版本
python setup.py build_ext --inplace --debug

# 使用gdb调试
gdb python
(gdb) run -c "import mymodule; mymodule.test_function()"
(gdb) bt  # 查看调用栈
```

#### 内存检查

```bash
# 使用valgrind检查内存泄漏
valgrind --tool=memcheck --leak-check=full python -c "import mymodule; mymodule.test()"
```

#### 添加调试宏

```cpp
#ifdef DEBUG
#define DEBUG_PRINT(fmt, args...) fprintf(stderr, "DEBUG: %s:%d:%s(): " fmt, \
    __FILE__, __LINE__, __func__, ##args)
#else
#define DEBUG_PRINT(fmt, args...)
#endif

static PyObject* my_function(PyObject* self, PyObject* args) {
    DEBUG_PRINT("Function called with args: %p\n", args);
    
    // 函数实现...
    
    DEBUG_PRINT("Function returning\n");
    return result;
}
```

### 7.3 单元测试

```python
# test_mymodule.py
import unittest
import mymodule

class TestMyModule(unittest.TestCase):
    
    def test_point_creation(self):
        p = mymodule.Point(3.0, 4.0)
        self.assertEqual(p.x, 3.0)
        self.assertEqual(p.y, 4.0)
    
    def test_point_distance(self):
        p1 = mymodule.Point(0, 0)
        p2 = mymodule.Point(3, 4)
        self.assertAlmostEqual(p1.distance(p2), 5.0)
    
    def test_point_arithmetic(self):
        p1 = mymodule.Point(1, 2)
        p2 = mymodule.Point(3, 4)
        p3 = p1 + p2
        self.assertEqual(p3.x, 4.0)
        self.assertEqual(p3.y, 6.0)
    
    def test_error_handling(self):
        with self.assertRaises(TypeError):
            mymodule.Point("invalid", "args")
    
    def test_memory_management(self):
        # 创建大量对象测试内存管理
        points = [mymodule.Point(i, i) for i in range(10000)]
        del points  # 应该正确释放内存

if __name__ == '__main__':
    unittest.main()
```

## 8. 性能优化和最佳实践

### 8.1 代码安全建议

1. **句柄化管理**：使用句柄而非直接指针，避免内存安全问题
2. **引用计数**：正确管理Python和C++对象的引用计数
3. **异常安全**：所有C函数都应该处理异常并设置Python异常
4. **内存清理**：实现proper的finalize函数，确保资源释放
5. **类型安全**：使用`PyObject_IsInstance`进行类型检查

### 8.2 性能优化

1. **避免频繁的类型转换**：缓存转换结果
2. **减少Python对象创建**：使用对象池或缓存
3. **批量操作**：一次性处理多个对象而非逐个处理
4. **延迟计算**：只在需要时进行昂贵的计算

### 8.3 常见陷阱

1. **引用计数错误**：
```cpp
// 错误：没有增加引用计数
PyObject* obj = some_function();
return obj;  // 可能导致对象过早释放

// 正确：增加引用计数
PyObject* obj = some_function();
Py_INCREF(obj);
return obj;
```

2. **异常处理错误**：
```cpp
// 错误：没有检查异常
PyObject* result = PyObject_Call(func, args, NULL);
return result;  // 如果调用失败，返回NULL但没有设置异常

// 正确：检查并处理异常
PyObject* result = PyObject_Call(func, args, NULL);
if (!result) {
    if (!PyErr_Occurred()) {
        PyErr_SetString(PyExc_RuntimeError, "Function call failed");
    }
    return NULL;
}
return result;
```

3. **内存泄漏**：
```cpp
// 错误：没有释放临时对象
PyObject* args = PyTuple_Pack(1, value);
PyObject* result = PyObject_Call(func, args, NULL);
return result;  // args没有被释放

// 正确：释放临时对象
PyObject* args = PyTuple_Pack(1, value);
PyObject* result = PyObject_Call(func, args, NULL);
Py_DECREF(args);
return result;
```

### 8.4 代码组织建议

1. **模块化设计**：将不同功能分到不同的源文件
2. **一致的命名**：使用统一的命名约定
3. **完善的文档**：为每个公开函数编写文档
4. **错误码标准化**：使用统一的错误处理机制

### 8.5 进阶主题

1. **异步支持**：实现异步函数调用
2. **序列化支持**：支持对象的序列化和反序列化
3. **调试支持**：添加调试信息和性能统计
4. **多线程支持**：添加GIL管理和线程安全机制

## 9. 实际案例：构建FFI系统

虽然这个教程是通用的，我们可以看看如何构建一个简单的FFI（Foreign Function Interface）系统作为实际案例。

### 9.1 设计目标

1. **类型安全**：在Python和C之间安全地传递数据
2. **对象管理**：管理复杂对象的生命周期
3. **扩展性**：支持用户自定义类型
4. **性能**：最小化转换开销

### 9.2 核心组件

```cpp
// 通用值类型
typedef union {
    int64_t v_int;
    double v_float;
    char* v_str;
    void* v_pointer;
} ValueUnion;

typedef struct {
    ValueUnion u;
    int32_t type;
    int32_t length;  // 用于字符串长度等
} Value;

// 类型转换函数
static int PyObjectToValue(PyObject* obj, Value* value);
static PyObject* ValueToPyObject(Value* value);
```

这个教程展示了如何构建一个功能完整的Python C Extension，从基础概念到高级特性，涵盖了对象系统、内存管理、异常处理、回调机制等关键主题。通过学习这些通用的模式和最佳实践，你可以构建出强大而高效的Python扩展模块。

