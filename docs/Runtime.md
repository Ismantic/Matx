# MC编译器系统设计与实现

## 第0章 系统概览：Python到C++的翻译机制

### 0.1 核心设计理念

MC编译器的本质是一个翻译器，它将Python代码转换为等价的C++代码。但这不是简单的语法翻译，而是一个复杂的语义转换过程，涉及类型系统、内存模型和执行语义的根本性变化。

Python作为动态类型语言，具有以下特征：
- 运行时类型检查
- 自动内存管理
- 灵活的对象模型
- 丰富的内置数据结构

C++作为静态类型语言，具有不同的特征：
- 编译时类型检查
- 手动内存管理
- 严格的类型系统
- 高性能但复杂的语法

MC编译器的挑战在于：如何在保持Python语义的同时，获得C++的性能优势？

### 0.2 编译流程实例：从Python到C++的完整转换

让我们通过一个具体例子来理解MC编译器的工作原理。以下是一个简单的Python函数及其完整的转换过程：

**原始Python代码：**
```python
def test_func(a: int, b: int) -> int:
    c = a + b
    return c
```

**转换步骤1：解析为内部IR (PrimFunc)**

首先，MC编译器将Python函数转换为内部的中间表示（IR）。这个IR是类型化的，包含了完整的类型信息：

```cpp
fn(a: int32, b: int32) -> int32 {
    Alloca c = (a + b)
    Return c
}
```

在这个IR中：
- 参数类型被明确指定为`int32`
- 变量`c`通过`Alloca`语句声明和初始化
- 返回类型被显式标记
- 所有运算都是类型安全的

**转换步骤2：生成最终的C++代码**

然后，MC编译器将这个IR转换为完整的C++代码，包括FFI接口和运行时集成：

```cpp
#include <stdint.h>
#include <string.h>
#include <stdexcept>
#include <stdio.h>
#include "runtime_value.h"
#include "parameters.h"
#include "registry.h"
#include "c_api.h"

using namespace mc::runtime;

const char* __mc_module_version = "1.0.0";

void* __mc_module_ctx = nullptr;

static thread_local char error_buffer[1024];

namespace {

int64_t test_func(int64_t a, int64_t b) {
  int64_t c = (a + b);
  return c;
}

int test_func__c_api(Value* args, int num_args, Value* ret_val, void* resource_handle) {
    if (num_args != 2) {
        snprintf(error_buffer, sizeof(error_buffer),
                "test_func() takes 2 positional arguments but %d were given",
                num_args);
        SetError(error_buffer);
        return -1;
    }

    if (args[0].t != TypeIndex::Int) {
        snprintf(error_buffer, sizeof(error_buffer),
                "test_func argument 0 type mismatch, expect 'int' type");
        SetError(error_buffer);
        return -1;
    }
    if (args[1].t != TypeIndex::Int) {
        snprintf(error_buffer, sizeof(error_buffer),
                "test_func argument 1 type mismatch, expect 'int' type");
        SetError(error_buffer);
        return -1;
    }

    auto result = test_func(args[0].u.v_int, args[1].u.v_int);

    ret_val->t = TypeIndex::Int;
    ret_val->u.v_int = result;
    return 0;
}

} // namespace

extern "C" {

BackendFunc __mc_func_array__[] = {
    (BackendFunc)test_func__c_api,
};

FuncRegistry __mc_func_registry__ = {
    "\1test_func",    // 导出函数: test_func
    __mc_func_array__,
};

const char* __mc_closures_names__ = "0\000";  // 无闭包

__attribute__((constructor))
void init_module() {
    // 初始化模块资源(如果需要)
    printf("Module loaded, registry at: %p\n", &__mc_func_registry__);
}

__attribute__((destructor))
void cleanup_module() {
    if (__mc_module_ctx) {
        // 清理模块资源
        __mc_module_ctx = nullptr;
    }
}
} // extern "C"
```

**生成代码的关键组成部分：**

1. **核心计算函数**：`test_func()`实现了实际的计算逻辑，使用静态类型的C++代码
2. **FFI接口函数**：`test_func__c_api()`提供了Python和C++之间的桥接
3. **类型检查**：在运行时验证参数类型，确保类型安全
4. **错误处理**：完整的错误处理机制，包括参数数量和类型检查
5. **模块注册**：通过`__mc_func_registry__`将函数注册到运行时系统
6. **生命周期管理**：通过构造/析构函数管理模块资源

**MC编译器的四层架构：**

```
Python源码 (def test_func(a: int, b: int) -> int: ...)
    ↓ [Python AST解析]
Python AST (FunctionDef节点)
    ↓ [语义分析+类型推导]
MC高级IR (AstFunc表示)
    ↓ [类型细化+优化]
MC底层IR (PrimFunc: fn(a: int32, b: int32) -> int32 {...})
    ↓ [代码生成]
C++代码 + 运行时库 (完整的可编译C++模块)
```

这个转换过程展示了MC编译器的核心价值：
- **保持Python语义**：生成的C++代码与原Python代码在语义上完全等价
- **提供类型安全**：通过运行时类型检查确保类型安全
- **实现高性能**：核心计算使用静态类型的C++代码，避免动态类型的开销
- **支持FFI互操作**：生成的代码可以被Python直接调用

### 0.3 类型系统的桥接

#### 0.3.1 Python类型到MC类型的映射

MC编译器设计了一套类型系统来桥接Python和C++：

| Python类型  | MC类型表示              | C++类型        |
|-----------|---------------------|--------------|
| int       | DataType::Int(64)   | int64_t      |
| float     | DataType::Float(64) | double       |
| bool      | DataType::Bool()    | bool         |
| str       | TypeIndex::Str      | std::string  |
| list[T]   | List<T>             | List (自定义容器) |
| dict[K,V] | Dict<K,V>           | Dict (自定义容器) |

这个映射体现在代码中：

```cpp
// src/datatype.h - MC的类型表示
enum TypeCode {
    kInt = 0,    // 对应Python int
    kUInt = 1,
    kFloat = 2,  // 对应Python float
    kHandle = 3  // 对应Python对象
};

// 布尔类型的特殊处理
static DataType Bool(int a = 1) {
    return DataType(kUInt, 1, a);  // 1位无符号整数
}
```

#### 0.3.2 渐进式类型细化

MC采用渐进式类型细化的策略：

1. AstExpr阶段：保留Python的灵活性，类型信息可能不完整
2. PrimExpr阶段：所有表达式都有明确的DataType信息

```cpp
// src/expression.h
class AstExprNode : public BaseExprNode {
    // 高级表达式，类型信息可能不完整
};

class PrimExprNode : public BaseExprNode {
    DataType datatype;  // 明确的类型信息
};
```

### 0.4 AST转换的核心机制

#### 0.4.1 表达式转换

Python表达式转换为MC IR的过程体现在表达式层次结构中：

```cpp
// 二元运算的转换
class PrimAddNode : public PrimBinaryOpNode<PrimAddNode> {
    PrimExpr a;  // 左操作数
    PrimExpr b;  // 右操作数
    // datatype继承自PrimExprNode，表示结果类型
};
```

Python的a + b表达式转换过程：
1. 识别为二元加法运算
2. 推导操作数类型
3. 确定结果类型
4. 创建PrimAdd节点

#### 0.4.2 语句转换

Python语句系统映射到MC的语句IR：

```cpp
// src/statement.h
class AllocaVarStmtNode : public StmtNode {
    PrimVar var;         // 变量声明
    BaseExpr init_value; // 初始值
};

class AssignStmtNode : public StmtNode {
    BaseExpr u;  // 左值
    BaseExpr v;  // 右值  
};
```

Python代码c = a + b的转换：
1. 创建AllocaVarStmt声明变量c
2. 创建PrimAdd表达式表示a + b
3. 创建AssignStmt连接变量和表达式

#### 0.4.3 函数转换

函数转换是最复杂的部分：

```cpp
// src/function.h
class PrimFuncNode : public BaseFuncNode {
    Array<PrimVar> gs;   // 类型化的参数列表
    Array<PrimExpr> fs;  // 默认参数
    Stmt body;           // 函数体语句
    Type rt;             // 返回类型
};
```

### 0.5 运行时系统的支撑作用

#### 0.5.1 为什么需要自定义运行时？

直接翻译Python到C++面临几个根本问题：

1. 类型擦除：Python的list可以存储任意类型，C++的std::vector<T>需要确定类型
2. 内存管理：Python自动垃圾回收，C++需要手动管理
3. 动态特性：Python支持运行时类型检查，C++编译时确定

MC的解决方案是构建自定义运行时系统：

```cpp
// 统一的值容器
class McValue {
    // 可以存储int, float, string, 或任意对象
    Value value_;  // 联合体 + 类型标签
};

// 智能指针管理对象生命周期
template<typename T>
class object_p {
    // 自动引用计数，解决内存管理问题
};

// 运行时类型检查
template<typename T>
bool IsType() const noexcept {
    return IsFrom(T::RuntimeTypeIndex());
}
```

#### 0.5.2 容器系统的实现

Python的内置容器在MC中有对应的实现：

```cpp
// src/container.h - Python list的对应实现
class List : public object_r {
    // 基于McValue的动态数组
    // 支持异构存储，如Python的list
};

class Dict : public object_r {
    std::unordered_map<McValue, McValue> data_;
    // McValue作为键和值，支持任意类型
};
```

### 0.6 FFI层：Python与C++的桥梁

#### 0.6.1 双向互操作

MC编译器不是纯粹的ahead-of-time编译器，它支持Python和C++代码的混合执行：

```python
# ffi.py - Python端
_LIB = ctypes.CDLL("lib/libmatx.so", ctypes.RTLD_GLOBAL)

# register.py - 对象注册机制  
def register_object(type_key=None, callback=None):
    # 将Python类注册到C++运行时
```

```cpp
// src/c_api.h - C++端
extern "C" {
    typedef void* FunctionHandle;
    typedef void* ObjectHandle;

    DLL int FuncCall_PYTHON_C_API(FunctionHandle fn, Value* vs, int n, Value* r);
    DLL int RegisterGlobal(const char* name, FunctionHandle fn);
}
```

#### 0.6.2 值传递机制

Python和C++之间的数据传递通过Value结构：

```cpp
struct Value {
    Union u{};      // 联合体存储实际值
    int32_t p{0};   // 字符串长度等辅助信息
    int32_t t{0};   // 类型标签
};
```

这个设计允许：
- Python的任意值传递给C++函数
- C++的计算结果返回给Python
- 自动的类型转换和检查

### 0.7 编译流程实例

让我们完整地跟踪一个函数的编译过程：

源码：
```python
def fibonacci(n: int) -> int:
    if n <= 1:
        return n
    return fibonacci(n-1) + fibonacci(n-2)
```

步骤1：Python AST解析
```python
# mc_simple.py等文件处理
ast_node = ast.parse(source_code)
func_def = ast_node.body[0]  # FunctionDef节点
```

步骤2：转换为高级IR
```python
# 创建AstFunc表示
ast_func = AstFunc(
    gs=[...],      # 参数
    fs=[],         # 默认参数  
    body=...,      # 函数体
    rt=IntType(),  # 返回类型
    ts=[]          # 类型参数
)
```

步骤3：类型细化为底层IR
```cpp
// C++端处理，转换为PrimFunc
PrimFunc prim_func = MakeObject<PrimFuncNode>();
prim_func->gs = {PrimVar("n", DataType::Int(64))};
prim_func->body = SeqStmt({
    // if语句转换为条件跳转
    // 递归调用转换为函数调用
    // 返回语句转换为ReturnStmt
});
```

步骤4：生成C++代码
```cpp
// 最终生成的C++函数（简化版）
int64_t fibonacci(int64_t n) {
    if (n <= 1) {
        return n;
    }
    return fibonacci(n-1) + fibonacci(n-2);
}
```

### 0.8 系统的优势与权衡

#### 0.8.1 优势

1. 语义保持：MC生成的C++代码与原Python代码语义等价
2. 性能提升：静态类型和编译优化带来显著性能提升
3. 渐进迁移：支持Python和C++代码混合执行
4. 类型安全：运行时类型检查防止类型错误

#### 0.8.2 权衡

1. 复杂性：需要维护复杂的运行时系统
2. 内存开销：类型标签和引用计数带来额外开销
3. 编译时间：多层转换增加编译时间
4. 调试复杂度：生成的C++代码可能难以调试

### 0.9 小结

MC编译器通过精心设计的四层架构，成功地将Python的动态特性映射到C++的静态世界：

1. 类型系统提供了Python到C++的类型桥梁
2. 运行时系统解决了内存管理和类型安全问题
3. IR系统实现了渐进式的类型细化
4. FFI层支持混合编程模式

这个设计使得MC编译器既保持了Python的易用性，又获得了C++的性能优势。在接下来的章节中，我们将深入探讨每个组件的具体实现。

---

## 第1章 运行时系统基础：类型、对象与值

本章介绍MC编译器运行时系统的三个核心组件：类型系统(DataType)、对象模型(Object)和值系统(Runtime Value)。这三个系统构成了整个编译器的底层基础设施。

### 1.1 设计背景与目标

MC编译器需要将Python的动态类型代码编译成C++的静态类型代码。这个过程中面临几个核心挑战：

1. 类型表示问题：如何用紧凑的方式表示各种类型信息？
2. 内存管理问题：如何在C++中安全地管理对象生命周期？
3. 值传递问题：如何统一处理基本类型和复杂对象？

MC的解决方案是构建一个分层的运行时系统：
- DataType层：紧凑的类型表示
- Object层：引用计数的对象管理
- Value层：类型擦除的值容器

### 1.2 类型系统：DataType与Dt

#### 1.2.1 Dt结构：4字节的类型表示

```cpp
struct Dt {
    uint8_t c_; // Code, e.g, Int, Float
    uint8_t b_; // e.g. 8, 16, 32
    uint16_t a_; // e.g. 1
};
```

Dt结构用仅仅4个字节表示完整的类型信息。这个设计非常精妙：

- c_字段存储类型码(TypeCode)，区分整数、浮点数、句柄等基本类别
- b_字段存储位宽信息，如8位、32位、64位等
- a_字段存储数组维度，支持向量化类型如int32x4

这种紧凑设计的优势是可以高效地进行类型比较和传递，避免了字符串比较的开销。

#### 1.2.2 DataType类：类型操作接口

```cpp
class DataType {
public:
    enum TypeCode {
        kInt = 0,
        kUInt = 1,
        kFloat = 2,
        kHandle = 3
    };

    static DataType Int(int b, int a = 1) {
        return DataType(kInt, b, a);
    }

    bool IsScalar() const {
        return a() == 1;
    }

    bool IsBool() const {
        return c() == DataType::kUInt && b() == 1;
    }
};
```

DataType类封装了Dt结构，提供了类型构造和查询的便利接口。特别注意：

- 静态工厂方法：Int(32)创建32位整数类型，Bool()创建布尔类型
- 类型查询方法：IsScalar()、IsBool()等提供语义化的类型检查
- 特殊类型处理：布尔类型被表示为1位无符号整数，体现了底层优化思想

#### 1.2.3 字符串与类型转换

```cpp
Dt StrToDt(std::string_view s) {
    if (s == "bool") {
        t.c_ = DataType::kUInt;
        t.b_ = 1;
        return t;
    }

    if (s.substr(0, 3) == "int") {
        t.c_ = DataType::kInt;
        scan = s.data() + 3;
    }
    // 解析位宽和向量长度...
}
```

类型系统支持从字符串解析类型，这对Python前端至关重要。解析器支持：
- 基本类型："int32", "float64", "bool"
- 向量类型："int32x4", "float64x2"

### 1.3 对象模型：引用计数与智能指针

对象模型是整个系统的核心，它解决了C++中的内存安全问题。

#### 1.3.1 object_t：统一的对象基类

```cpp
class object_t {
public:
    virtual ~object_t();

    int32_t Index() const {
        return t_;
    }

    template <typename T>
    bool IsType() const noexcept {
        return IsFrom(T::RuntimeTypeIndex());
    }

    void IncCounter() noexcept { ++count_; }
    void DecCounter() noexcept {
        if (--count_ == 0) {
            delete this;
        }
    }

protected:
    int32_t t_{0};                    // 类型索引
    std::atomic<int32_t> count_{0};   // 引用计数
};
```

object_t是所有对象的基类，它的设计体现了几个关键思想：

**类型索引系统**：每个对象都有一个类型索引t_，避免了虚函数调用的开销。IsType<T>()通过类型索引快速判断对象类型，比dynamic_cast更高效。

**原子引用计数**：使用std::atomic<int32_t>确保多线程安全。DecCounter()中的自删除机制实现了自动内存管理。

**RTTI替代方案**：通过静态类型索引替代C++的RTTI，既提高了性能又保持了类型安全。

#### 1.3.2 object_p：类型安全的智能指针

```cpp
template <typename T>
class object_p {
public:
    explicit object_p(T* p) noexcept : data_{p} {
        if (data_) data_->IncCounter();
    }

    ~object_p() {
        if (data_) data_->DecCounter();
    }

    object_p(const object_p& other) noexcept : data_(other.data_) {
        if (data_) data_->IncCounter();
    }

    object_p& operator=(const object_p& other) noexcept {
        if (data_ != other.data_) {
            T* o = data_;
            data_ = other.data_;
            if (data_) data_->IncCounter();
            if (o) o->DecCounter();
        }
        return *this;
    }

private:
    T* data_{nullptr};
};
```

object_p是一个智能指针模板，提供了RAII式的内存管理：

**构造时增加引用**：构造函数自动调用IncCounter()
**析构时减少引用**：析构函数自动调用DecCounter()
**拷贝语义**：拷贝构造和赋值都正确维护引用计数
**异常安全**：通过RAII保证即使发生异常也能正确释放内存

#### 1.3.3 object_r：类型擦除的对象容器

```cpp
class object_r {
public:
    explicit object_r(object_p<object_t> p) noexcept
        : data_(std::move(p)) {}

    template<typename T>
    const T* As() const noexcept {
        if (data_ && data_->IsType<T>()) {
            return static_cast<const T*>(data_.get());
        }
        return nullptr;
    }

    bool none() const noexcept {
        return data_ == nullptr;
    }

private:
    object_p<object_t> data_;
};
```

object_r实现了类型擦除，允许以统一的方式处理不同类型的对象：

**类型擦除**：所有对象都存储为object_p<object_t>，丢失编译时类型信息
**运行时类型检查**：As<T>()方法通过RTTI安全地进行类型转换
**空值处理**：none()方法检查对象是否为空

#### 1.3.4 MakeObject：对象创建工厂

```cpp
template <typename T, typename... Gs>
object_p<T> MakeObject(Gs&&... gs) {
    static_assert(std::is_base_of<object_t, T>::value,
                  "MakeObject can only be used to create object_t");
    T* p = new T(std::forward<Gs>(gs)...);
    p->t_ = T::RuntimeTypeIndex();  // 设置类型索引
    return object_p<T>(p);
}
```

MakeObject是创建对象的统一入口，它确保：
- 类型约束：只能创建object_t的派生类
- 类型索引设置：自动设置正确的类型索引
- 完美转发：高效地传递构造参数

### 1.4 值系统：Any、McView与McValue

值系统解决了如何统一处理基本类型和对象类型的问题。

#### 1.4.1 Value联合体：统一存储

```cpp
union Union {
    int64_t v_int;
    double  v_float;
    char*   v_str;
    void*   v_pointer;
    Dt      v_datatype;
};

struct Value {
    Union u{};
    int32_t p{0}; // str length
    int32_t t{0}; // type index
};
```

Value结构是一个带类型标签的联合体，可以存储任意类型的值：
- 联合体节省内存，所有类型共享同一块存储空间
- 类型标签t记录当前存储的实际类型
- 长度字段p用于字符串等变长数据

#### 1.4.2 Any：类型擦除的值基类

```cpp
class Any {
protected:
    Value value_;

public:
    bool IsNull() const noexcept {
        return value_.t == TypeIndex::Null;
    }

    template<typename T>
    bool Is() const noexcept {
        // 基于SFINAE的类型检查
        return Is<T>(std::is_base_of<object_r, T>{});
    }

    template<typename T>
    T As() const {
        if (!Is<T>()) {
            throw std::runtime_error("Type Mismatch in Conversion");
        }
        return As_<T>();
    }
};
```

Any类提供了类型安全的值访问接口：

**类型查询**：Is<T>()使用模板特化和SFINAE技术进行类型检查
**安全转换**：As<T>()在转换前检查类型兼容性
**异常处理**：类型不匹配时抛出异常而非未定义行为

#### 1.4.3 McView：轻量级值引用

```cpp
class McView : public Any {
public:
    constexpr McView(Value value) noexcept: Any(value) {}

    McView(const McValue& value) noexcept;

    // 默认拷贝语义，无需内存管理
    McView(const McView&) noexcept = default;
};
```

McView是轻量级的值引用，它：
- 不拥有值的内存，只是一个引用
- 拷贝开销很小，适合在函数间传递
- 主要用于只读访问

#### 1.4.4 McValue：拥有性值容器

```cpp
class McValue : public Any {
public:
    McValue(int64_t value) noexcept {
        value_.u.v_int = value;
        value_.t = TypeIndex::Int;
    }

    McValue(const char* str) {
        if (str) {
            size_t len = strlen(str);
            value_.u.v_str = new char[len+1];
            strcpy(value_.u.v_str, str);
            value_.t = TypeIndex::Str;
        }
    }

    ~McValue() { Clean(); }

private:
    void Clean() noexcept {
        if (value_.t == TypeIndex::Str) {
            delete[] value_.u.v_str;
        } else if (value_.t >= TypeIndex::Object) {
            if (value_.u.v_pointer) {
                static_cast<object_t*>(value_.u.v_pointer)->DecCounter();
            }
        }
    }

    void CopyFrom(const Any& other) {
        // 深拷贝实现...
    }
};
```

McValue是拥有性的值容器，负责值的生命周期管理：

**自动内存管理**：析构函数中的Clean()方法处理所有类型的内存释放
**深拷贝语义**：CopyFrom()实现了正确的深拷贝
**类型多样性**：支持基本类型、字符串、对象等所有类型

### 1.5 系统集成与使用模式

#### 1.5.1 类型-对象-值的协作

```cpp
// 创建一个整数类型的变量对象
auto var_node = MakeObject<PrimVarNode>();
var_node->datatype = DataType::Int(32);
var_node->var_name = "x";

// 包装成类型安全的对象引用
PrimVar var(object_p<object_t>(var_node));

// 创建包含该对象的值
McValue value(var);

// 类型安全的访问
if (value.Is<PrimVar>()) {
    PrimVar retrieved = value.As<PrimVar>();
    std::cout << retrieved->var_name << std::endl;
}
```

这个例子展示了三个系统的协作：
1. DataType提供类型信息
2. Object系统管理对象生命周期
3. Value系统提供统一的值接口

#### 1.5.2 内存安全保证

整个系统的内存安全来自多层保护：

1. 智能指针：object_p自动管理引用计数
2. RAII：McValue的析构函数确保资源释放
3. 类型检查：运行时类型检查防止非法转换
4. 异常安全：所有操作都是异常安全的

#### 1.5.3 性能考量

虽然提供了高层次的抽象，但系统在性能上做了很多优化：

1. 类型索引：避免虚函数调用和字符串比较
2. 原地构造：MakeObject使用完美转发避免不必要的拷贝
3. 联合体存储：Value结构最小化内存占用
4. 引用语义：McView提供零拷贝的值传递

### 1.6 小结

本章介绍的运行时系统为整个编译器提供了坚实的基础：

- DataType提供了紧凑高效的类型表示
- Object模型解决了C++中的内存安全问题
- Value系统实现了类型擦除和统一的值处理

这三个系统的协同工作，使得MC编译器能够安全高效地处理从Python到C++的类型转换和内存管理。在下一章中，我们将看到如何基于这些基础设施构建更高层的容器系统。

---

## 第2章 容器系统：Python数据结构的C++实现

本章深入探讨MC编译器的容器系统，它是Python数据结构在C++中的精准实现。容器系统包含Array、List、Dict、Set、Tuple、Map和Str等核心组件，它们不仅保持了Python的语义，还提供了高效的内存管理和类型安全保证。

### 2.1 容器系统设计理念

#### 2.1.1 Python容器的挑战

Python的内置容器具有几个关键特性：
- 异构存储：list可以存储任意类型的对象
- 动态扩展：容器大小可以在运行时改变
- 引用语义：容器存储的是对象引用，而非对象本身
- 丰富的操作：支持切片、迭代、比较等高级操作

在C++中直接实现这些特性面临诸多挑战：
- 类型擦除：如何在静态类型系统中支持异构存储？
- 内存安全：如何安全地管理动态分配的内存？
- 迭代器设计：如何提供符合STL标准的迭代器接口？

#### 2.1.2 MC的解决方案

MC编译器采用了分层设计来解决这些问题：

1. **Node层**：实际的数据存储和基本操作
2. **Container层**：类型安全的高级接口
3. **Value层**：统一的值表示和类型转换

这种设计的核心思想是：
- 通过McValue实现类型擦除
- 通过object_r实现内存安全
- 通过模板实现类型安全的接口

### 2.2 Array：同构动态数组

Array是MC中最基础的容器，对应Python的同构列表。

#### 2.2.1 ArrayNode：底层存储实现

```cpp
class ArrayNode : public object_t {
public:
    static constexpr const int32_t INDEX = TypeIndex::RuntimeArray;
    static constexpr const std::string_view NAME = "Array";
    DEFINE_TYPEINDEX(ArrayNode, object_t);

    using value_type = object_r;
    using container_type = std::vector<value_type>;
    using const_iterator = typename container_type::const_iterator;

    void push_back(object_r value) {
        data_.push_back(std::move(value));
    }

    size_t size() const { return data_.size(); }
    const object_r& operator[](size_t i) const { return data_[i]; }

private:
    std::vector<object_r> data_;
};
```

ArrayNode的设计要点：

**类型索引系统**：通过`INDEX`常量实现运行时类型识别，避免了虚函数调用的开销。

**object_r存储**：使用`object_r`作为元素类型，实现了类型擦除的同时保持内存安全。`object_r`是`object_p<object_t>`的类型擦除版本。

**STL容器封装**：底层使用`std::vector`，充分利用了STL的性能优化和内存管理。

**移动语义支持**：`push_back`使用移动语义避免不必要的拷贝。

#### 2.2.2 Array：类型安全的接口层

```cpp
template<typename T>
class Array : public object_r {
public:
    using value_type = T;
    using node_type = ArrayNode;

    explicit Array(object_p<object_t> obj) : object_r(std::move(obj)) {
        if (data_ && !data_->IsType<ArrayNode>()) {
            throw std::runtime_error("Type mismatch: expected Array");
        }
    }

    Array(std::initializer_list<T> init) {
        Assign(init.begin(), init.end());
    }

    T operator[](size_t i) const {
        return Downcast<T>(Get()->operator[](i));
    }

private:
    ArrayNode* Get() const {
        return static_cast<ArrayNode*>(const_cast<object_t*>(get()));
    }
};
```

Array模板类提供了类型安全的高级接口：

**类型检查**：构造函数中进行运行时类型检查，确保类型安全。

**类型转换**：`Downcast<T>`安全地将`object_r`转换为具体类型`T`。

**初始化列表支持**：支持C++11的初始化列表语法，提供便利的构造方式。

#### 2.2.3 内存优化：COW和就地构造

```cpp
template <typename IterType>
void Assign(IterType first, IterType last) {
    int64_t cap = std::distance(first, last);
    
    ArrayNode* p = Get();
    if (p != nullptr && data_.unique() && p->capacity() >= cap) {
        // 重用现有空间
        p->clear();
    } else {
        // 分配新空间
        data_ = ArrayNode::Empty(cap);
        p = Get();
    }
    
    // 就地构造
    object_r* itr = p->MutableBegin();
    for (int64_t i = 0; i < cap; ++i, ++first, ++itr) {
        new (itr) object_r(*first);
    }
}
```

这个实现展示了几个重要的优化技术：

**COW优化**：通过`data_.unique()`检查引用计数，只有在独占时才重用内存。

**容量预分配**：通过`capacity()`检查避免重复分配。

**就地构造**：使用placement new直接在预分配的内存中构造对象。

### 2.3 List：异构动态列表

List是Python list的直接对应，支持异构存储。

#### 2.3.1 ListNode：异构存储的实现

```cpp
class ListNode : public object_t {
public:
    static constexpr uint32_t INDEX = TypeIndex::RuntimeList;
    static constexpr const std::string_view NAME = "List";
    DEFINE_TYPEINDEX(ListNode, object_t);

    using value_type = McValue;
    using container_type = std::vector<value_type>;

    ListNode() = default;
    ListNode(std::initializer_list<value_type> init) : data_(init) {}

    value_type& operator[](int64_t i) {
        return data_[i];
    }

    value_type& at(int64_t i) {
        if (i < 0) {
            i += data_.size();  // 支持负索引
        }
        return data_.at(i);
    }

    template<typename T>
    void append(T&& value) {
        push_back(value_type(std::forward<T>(value)));
    }

private:
    container_type data_;
};
```

ListNode与ArrayNode的关键差异：

**McValue存储**：使用`McValue`而非`object_r`，支持基本类型和对象的统一存储。

**负索引支持**：`at()`方法支持Python式的负索引，`list[-1]`访问最后一个元素。

**模板化append**：通过完美转发支持任意类型的元素添加。

#### 2.3.2 List：Python语义的接口

```cpp
class List : public object_r {
public:
    using Node = ListNode;
    using value_type = Node::value_type;

    List() = default;
    List(std::initializer_list<value_type> init) {
        data_ = MakeObject<Node>(init);
    }

    value_type& operator[](int64_t i) const {
        return Get()->at(i);  // 使用at()支持负索引
    }

    template<typename T>
    void append(T&& val) const {
        if (auto node = Get()) {
            node->append(std::forward<T>(val));
        }
    }

private:
    Node* Get() const {
        return static_cast<Node*>(const_cast<object_t*>(get()));
    }
};
```

List接口设计的Python兼容性：

**负索引**：`operator[]`使用`at()`方法，完全支持Python的负索引语义。

**append方法**：保持Python的方法名和语义。

**const正确性**：修改操作通过`const`方法实现，因为修改的是对象内容而非引用本身。

### 2.4 Dict：键值对映射

Dict是Python dict的实现，支持任意类型的键和值。

#### 2.4.1 DictNode：哈希表的实现

```cpp
class DictNode : public object_t {
public:
    static constexpr const int32_t INDEX = TypeIndex::RuntimeDict;
    static constexpr const std::string_view NAME = "Dict";
    DEFINE_TYPEINDEX(DictNode, object_t);

    using container_type = std::unordered_map<McValue, McValue>;
    using key_type = typename container_type::key_type;
    using mapped_type = typename container_type::mapped_type;

    mapped_type& operator[](const key_type& key) {
        return data_[key];
    }

    bool contains(const key_type& key) const {
        return find(key) != data_.end();
    }

    const_iterator find(const key_type& key) const {
        return data_.find(key);
    }

private:
    container_type data_;
};
```

DictNode的核心特性：

**McValue键值**：键和值都使用`McValue`，支持任意类型的键值对。

**哈希支持**：`McValue`实现了哈希函数，支持作为`unordered_map`的键。

**Python语义**：`operator[]`和`contains()`方法与Python dict的行为一致。

#### 2.4.2 迭代器设计：keys、values、items

```cpp
template <class I>
struct key_iterator_ : public I {
    using value_type = typename I::value_type::first_type;
    
    key_iterator_(const I& i) : I(i) {}
    
    value_type operator*() const {
        return (*this)->first;
    }
};

template <class I>
struct value_iterator_ : public I {
    using value_type = typename I::value_type::second_type;
    
    value_iterator_(const I& i) : I(i) {}
    
    value_type operator*() const {
        return (*this)->second;
    }
};

template <typename D>
struct DictKeys {
    using iterator = typename D::key_const_iterator;
    D data;
    
    DictKeys(const D& data) : data(data) {}
    
    iterator begin() const { return data.key_begin(); }
    iterator end() const { return data.key_end(); }
};
```

这个设计实现了Python dict的三种视图：

**迭代器适配器**：通过继承和操作符重载，将pair迭代器转换为键或值迭代器。

**视图对象**：`DictKeys`、`DictValues`、`DictItems`提供了类似Python的视图接口。

**延迟计算**：视图不拷贝数据，只提供迭代器适配，保持高效。

### 2.5 Set：唯一值集合

Set实现了Python set的语义和操作。

#### 2.5.1 SetNode：哈希集合的实现

```cpp
class SetNode : public object_t {
public:
    static const int32_t INDEX = TypeIndex::RuntimeSet;
    static constexpr const std::string_view NAME = "Set";
    DEFINE_TYPEINDEX(SetNode, object_t);

    using value_type = McValue;
    using container_type = std::unordered_set<value_type>;

    bool insert(const value_type& value) {
        return data_.insert(value).second;
    }

    bool contains(const value_type& value) const {
        return data_.find(value) != data_.end();
    }

    size_t erase(const value_type& value) {
        return data_.erase(value);
    }

private:
    container_type data_;
};
```

#### 2.5.2 Set：集合运算的实现

```cpp
class Set : public object_r {
public:
    Set set_union(const Set& other) const {
        auto result = Set();
        if (empty()) {
            result = other;
        } else if (!other.empty()) {
            result = *this;
            for (const auto& item : other) {
                result.insert(item);
            }
        }
        return result;
    }

    Set set_minus(const Set& other) const {
        auto result = Set();
        if (!empty()) {
            for (const auto& item : *this) {
                if (!other.contains(item)) {
                    result.insert(item);
                }
            }
        }
        return result;
    }
};
```

Set的集合运算实现了Python set的数学语义：

**并集运算**：`set_union`实现了Python的`|`运算符。

**差集运算**：`set_minus`实现了Python的`-`运算符。

**惰性求值**：在空集合情况下避免不必要的计算。

### 2.6 Tuple：不可变序列

Tuple是Python tuple的实现，强调不可变性。

#### 2.6.1 TupleNode：固定大小的存储

```cpp
class TupleNode : public object_t {
public:
    static constexpr const int32_t INDEX = TypeIndex::RuntimeTuple;
    static constexpr const std::string_view NAME = "Tuple";
    DEFINE_TYPEINDEX(TupleNode, object_t);

    using value_type = McValue;
    using iterator = const value_type*;

    TupleNode(const value_type* s, const value_type* e)
        : size_(e - s) {
        data_ = new value_type[size_];
        for (size_t i = 0; i < size_; ++i) {
            data_[i] = s[i];
        }
    }

    ~TupleNode() {
        delete[] data_;
    }

    size_t size() const noexcept { return size_; }
    const value_type* data() const noexcept { return data_; }

private:
    size_t size_;
    value_type* data_;
};
```

TupleNode的设计特点：

**固定大小**：构造后大小不可变，符合tuple的语义。

**手动内存管理**：使用原生数组而非vector，避免额外的容量开销。

**只读访问**：只提供const访问方法，保证不可变性。

#### 2.6.2 Tuple：不可变语义的接口

```cpp
class Tuple : public object_r {
public:
    using Node = TupleNode;
    using value_type = Node::value_type;

    template<typename Iterator>
    Tuple(Iterator s, Iterator e) {
        std::vector<value_type> t;
        std::copy(s, e, std::back_inserter(t));
        data_ = MakeObject<Node>(t.data(), t.data()+t.size());
    }

    const value_type& operator[](size_t i) const {
        auto node = static_cast<const Node*>(get());
        return node->data()[i];
    }

    bool operator==(const Tuple& other) const noexcept {
        if (size() != other.size()) return false;
        return std::equal(begin(), end(), other.begin());
    }
};
```

Tuple的不可变性保证：

**只读访问**：`operator[]`返回const引用，防止修改。

**值比较**：`operator==`实现了结构化比较，符合Python tuple的语义。

**构造时拷贝**：在构造时就完成所有数据拷贝，之后不可修改。

### 2.7 Map：类型化键值映射

Map是对Dict的类型化封装，提供编译时类型安全。

#### 2.7.1 MapNode：通用键值存储

```cpp
class MapNode : public object_t {
public:
    static constexpr const int32_t INDEX = TypeIndex::RuntimeMap;
    static constexpr const std::string_view NAME = "Map";
    DEFINE_TYPEINDEX(MapNode, object_t);

    using key_type = object_r;
    using mapped_type = object_r;
    using container_type = std::unordered_map<key_type, mapped_type, object_s, object_e>;

    const mapped_type& at(const key_type& key) const {
        auto it = data_.find(key);
        if (it == data_.end()) {
            throw std::out_of_range("Key not found in Map");
        }
        return it->second;
    }

    void set(const key_type& key, const mapped_type& value) {
        data_[key] = value;
    }

private:
    container_type data_;
};
```

MapNode使用`object_r`作为键值类型，需要自定义哈希和比较函数：

**object_s**：`object_r`的哈希函数实现。

**object_e**：`object_r`的相等比较函数实现。

**异常安全**：`at()`方法在键不存在时抛出异常，而非返回默认值。

#### 2.7.2 Map：类型化接口

```cpp
template<typename K, typename V>
class Map : public object_r {
public:
    using key_type = K;
    using mapped_type = V;
    using node_type = MapNode;

    V at(const K& key) const {
        return Downcast<V>(Get()->at(key));
    }

    void set(const K& key, const V& value) {
        Get()->set(key, value);
    }

    V operator[](const K& key) const {
        auto* node = Get();
        auto it = node->find(key);
        if (it == node->end()) {
            return V();  // 返回默认构造的值
        }
        return Downcast<V>(it->second);
    }

private:
    MapNode* Get() const {
        return static_cast<MapNode*>(const_cast<object_t*>(get()));
    }
};
```

Map的类型化设计：

**静态类型检查**：模板参数`K`和`V`提供编译时类型检查。

**安全类型转换**：`Downcast<V>`在运行时验证类型转换的正确性。

**默认值语义**：`operator[]`在键不存在时返回默认构造的值，而非抛出异常。

### 2.8 Str：字符串的高效实现

Str是Python str在MC中的对应实现。

#### 2.8.1 StrNode：字符串数据存储

```cpp
class StrNode : public object_t {
public:
    static constexpr const int32_t INDEX = TypeIndex::RuntimeStr;
    static constexpr const std::string_view NAME = "RuntimeStr";
    DEFINE_TYPEINDEX(StrNode, object_t);

    using container_type = std::string;

    StrNode() = default;
    explicit StrNode(container_type data) : data_(std::move(data)) {}

    size_type size() const noexcept { return data_.size(); }
    const char* data() const noexcept { return data_.data(); }
    const char* c_str() const noexcept { return data_.c_str(); }

    container_type data_;

private:
    friend class Str;
};
```

StrNode的设计特点：

**std::string封装**：直接使用标准库的字符串实现，获得所有优化。

**移动语义**：构造函数使用移动语义，避免字符串拷贝。

**友元访问**：通过friend声明允许Str直接访问内部数据。

#### 2.8.2 Str：COW和字符串操作

```cpp
class Str : public object_r {
public:
    using node_type = StrNode;
    using size_type = size_t;

    Str() : object_r(MakeObject<StrNode>()) {}
    Str(const char* str) 
        : object_r(MakeObject<StrNode>(StrNode::container_type(str))) {}
    Str(const std::string& str) 
        : object_r(MakeObject<StrNode>(str)) {}

    size_type size() const noexcept { return Get()->size(); }
    const char* data() const noexcept { return Get()->data(); }

    bool operator==(const Str& other) const noexcept {
        return std::string_view(data(), size()) == 
               std::string_view(other.data(), other.size());
    }

    // COW support
    DEFINE_COW_METHOD(StrNode)

    operator std::string_view() const noexcept {
        return std::string_view(data(), size());
    }

private:
    const StrNode* Get() const {
        return static_cast<const StrNode*>(get());
    }

    StrNode* GetMutable() {
        return static_cast<StrNode*>(get_mutable());
    }
};
```

Str的关键特性：

**COW支持**：`DEFINE_COW_METHOD`宏实现了写时复制优化。

**视图转换**：`operator std::string_view()`提供零拷贝的字符串视图。

**高效比较**：使用`string_view`进行比较，避免字符串拷贝。

#### 2.8.3 字符串运算符重载

```cpp
inline Str operator+(const Str& lhs, const Str& rhs) {
    std::string result;
    result.reserve(lhs.size() + rhs.size());
    result.append(lhs.data(), lhs.size());
    result.append(rhs.data(), rhs.size());
    return Str(result);
}

inline std::ostream& operator<<(std::ostream& os, const Str& str) {
    return os.write(str.data(), str.size());
}

// 专门的哈希函数
template<>
struct std::hash<mc::runtime::Str> {
    size_t operator()(const mc::runtime::Str& str) const noexcept {
        return std::hash<std::string_view>{}(std::string_view(str));
    }
};
```

运算符重载的设计：

**预分配优化**：字符串连接时预分配空间，避免多次重新分配。

**流输出支持**：重载`operator<<`支持标准库的流输出。

**哈希函数特化**：为`std::hash`提供特化，支持在哈希容器中使用。

### 2.9 容器系统的统一设计模式

#### 2.9.1 分层架构模式

所有容器都遵循相同的分层架构：

```
Container<T> (类型化接口)
     ↓
ContainerNode (数据存储)
     ↓
object_t (对象基类)
```

这种分层设计的优势：

**类型安全**：上层接口提供编译时类型检查。

**内存管理**：通过object_t自动管理内存。

**性能优化**：底层使用STL容器获得最佳性能。

#### 2.9.2 类型索引模式

```cpp
#define DEFINE_TYPEINDEX(Class, Base) \
    static constexpr int32_t RuntimeTypeIndex() { \
        return Class::INDEX; \
    }
```

每个容器节点都定义了唯一的类型索引：

**运行时类型识别**：通过类型索引快速判断对象类型。

**避免RTTI开销**：不使用dynamic_cast，性能更高。

**类型安全转换**：结合模板实现安全的类型转换。

#### 2.9.3 COW模式

```cpp
#define DEFINE_COW_METHOD(NodeType) \
    object_t* get_mutable() { \
        if (data_ && data_->count_ > 1) { \
            data_ = MakeObject<NodeType>(*As<NodeType>()); \
        } \
        return data_.get(); \
    }
```

写时复制优化：

**读取优化**：多个引用共享同一个对象，节省内存。

**写入优化**：只有在修改时才进行拷贝，避免不必要的开销。

**线程安全**：原子引用计数保证多线程安全。

### 2.10 容器注册与FFI集成

#### 2.10.1 容器工厂函数

```cpp
static McValue NewArray(Parameters gs) {
    std::vector<Object> data;
    data.reserve(gs.size());
    for (int i = 0; i < gs.size(); ++i) {
        data.push_back(AsObject(gs[i]));
    }
    return Array<Object>(data);
}

static McValue NewTuple(Parameters gs) {
    std::vector<McValue> fs;
    for(auto i = 0; i < gs.size(); ++i) {
        fs.push_back(McValue(gs[i]));
    }
    return Tuple(fs.begin(), fs.end());
}
```

工厂函数的设计：

**统一接口**：所有容器都通过`Parameters`参数创建。

**类型转换**：自动将参数转换为合适的内部类型。

**异常安全**：所有操作都是异常安全的。

#### 2.10.2 注册宏的使用

```cpp
REGISTER_TYPEINDEX(ArrayNode);
REGISTER_TYPEINDEX(TupleNode);
REGISTER_TYPEINDEX(DictNode);
REGISTER_TYPEINDEX(SetNode);
REGISTER_TYPEINDEX(ListNode);
REGISTER_TYPEINDEX(MapNode);
REGISTER_TYPEINDEX(StrNode);

REGISTER_FUNCTION("runtime.Array", NewArray);
REGISTER_FUNCTION("runtime.Tuple", NewTuple);
REGISTER_FUNCTION("runtime.Map", NewMap);
REGISTER_GLOBAL("runtime.Str")
    .SetBody([](std::string str){
        return Str(str);
    });
```

注册系统的作用：

**类型注册**：将C++类型注册到运行时系统。

**函数注册**：将构造函数注册为全局函数。

**Python集成**：支持从Python代码调用C++容器。

### 2.11 性能优化技术

#### 2.11.1 内存布局优化

容器系统采用了多种内存优化技术：

**对象池**：预分配对象池减少内存分配开销。

**内存对齐**：确保对象在内存中正确对齐。

**缓存友好**：连续内存布局提高缓存命中率。

#### 2.11.2 算法优化

**惰性求值**：延迟计算直到真正需要时才执行。

**短路求值**：在可能的情况下提前结束计算。

**批量操作**：批量处理多个元素减少函数调用开销。

#### 2.11.3 编译时优化

**模板特化**：为常用类型提供专门的优化版本。

**内联函数**：关键路径上的函数都声明为内联。

**常量折叠**：编译器在编译时计算常量表达式。

### 2.12 使用示例和最佳实践

#### 2.12.1 基本使用模式

```cpp
// 创建Array
Array<int> arr = {1, 2, 3, 4, 5};
for (int i : arr) {
    std::cout << i << " ";
}

// 创建Dict
Dict dict = {{"name", "John"}, {"age", 30}};
if (dict.contains("name")) {
    std::cout << dict["name"].As<std::string>() << std::endl;
}

// 创建List
List list;
list.append(42);
list.append("hello");
list.append(3.14);
```

#### 2.12.2 类型安全的使用

```cpp
// 类型化Map
Map<std::string, int> scores;
scores.set("Alice", 95);
scores.set("Bob", 87);

// 安全的类型转换
if (auto alice_score = scores.at("Alice"); alice_score > 90) {
    std::cout << "Alice got an A!" << std::endl;
}
```

#### 2.12.3 性能优化的使用

```cpp
// 预分配容量
Array<int> large_array;
large_array.reserve(10000);

// 使用移动语义
std::vector<std::string> source = {"a", "b", "c"};
Array<std::string> dest(std::move(source));

// COW优化
Str original = "Hello";
Str copy = original;  // 不进行实际拷贝
copy = copy + " World";  // 现在才进行拷贝
```

### 2.13 小结

本章详细介绍了MC编译器的容器系统，这个系统的设计体现了以下关键思想：

1. **分层架构**：通过Node和Container层的分离，实现了数据存储和类型安全的分离。

2. **类型擦除**：通过McValue和object_r实现了Python式的异构存储。

3. **内存安全**：通过智能指针和引用计数实现了自动内存管理。

4. **性能优化**：通过COW、预分配、就地构造等技术实现了高性能。

5. **Python兼容**：所有容器都保持了Python的语义和API风格。

这个容器系统为MC编译器提供了坚实的基础，使得Python代码能够在保持语义的同时获得C++的性能优势。在下一章中，我们将探讨如何在这些容器的基础上构建更高级的运行时功能。
