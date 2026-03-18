# MC 运行时值系统完整文档

## 1. 系统概述

MC 运行时值系统是一个统一的类型安全数据表示框架，提供了从基础类型到复杂对象的完整抽象。系统基于 C 联合体设计，实现零开销的类型统一表示，同时通过 C++ 模板技术提供类型安全的访问接口。

### 1.1 核心设计原则

1. **类型统一**: 所有运行时值都基于统一的 `Value` 结构表示
2. **零开销抽象**: 基于联合体的内存布局，避免不必要的内存开销
3. **类型安全**: 运行时类型检查 + 编译时类型推导
4. **RAII 语义**: 完整的生命周期管理和资源自动释放
5. **高性能**: 针对函数调用和参数传递优化

### 1.2 系统架构

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   底层C结构     │    │   类型系统      │    │   高级接口      │
├─────────────────┤    ├─────────────────┤    ├─────────────────┤
│ Value (union)   │ -> │ DataType/Dt     │ -> │ Any/McView      │
│ 16字节联合体    │    │ 类型描述符      │    │ McValue         │
│ 类型索引       │    │ 字符串转换      │    │ Parameters      │
└─────────────────┘    └─────────────────┘    └─────────────────┘
```

## 2. 底层数据表示

### 2.1 Value 结构

所有运行时值基于 C API 的 `Value` 结构：

```cpp
struct Value {
    union {
        int64_t v_int;        // 64位整数
        double v_float;       // 64位浮点数
        char* v_str;          // 字符串指针
        void* v_pointer;      // 对象指针
        Dt v_datatype;        // 数据类型描述符
    } u;
    int32_t p;               // 扩展信息（如字符串长度）
    int32_t t;               // 类型索引（TypeIndex枚举）
};
```

**内存布局**:
- 总大小: 16 字节
- 联合体: 8 字节（存储实际数据）
- 扩展信息: 4 字节（上下文相关）
- 类型索引: 4 字节（TypeIndex 枚举值）

**类型索引分类**:
- 负数 (-6 到 -1): 基础类型 (Int, Float, Str, Func, DataType, Null)
- 0-255: 预定义对象类型 (Object, Module, RuntimeStr, etc.)
- 256+: 动态分配的对象类型

## 3. 数据类型系统

### 3.1 Dt 结构体 - 紧凑类型描述

```cpp
struct Dt {
    uint8_t c_;   // 类型代码 (Code): 0=Int, 1=UInt, 2=Float, 3=Handle
    uint8_t b_;   // 位宽 (Bits): 8, 16, 32, 64
    uint16_t a_;  // 通道数 (Array/Lanes): 1, 2, 4, 8, 16...
};
```

**设计特点**:
- **紧凑性**: 4字节描述完整类型信息
- **向量化**: 支持 SIMD 向量类型 (如 int32x4)
- **可扩展**: 支持未来新的类型代码

### 3.2 DataType 类 - 高级类型接口

```cpp
class DataType {
public:
    enum TypeCode {
        kInt = 0,       // 有符号整数
        kUInt = 1,      // 无符号整数  
        kFloat = 2,     // 浮点数
        kHandle = 3     // 句柄/指针
    };
    
    // 构造函数
    DataType() {}
    explicit DataType(Dt t) : data_(t) {}
    DataType(int code, int bits, int lanes) {
        data_.c_ = static_cast<uint8_t>(code);
        data_.b_ = static_cast<uint8_t>(bits);
        data_.a_ = static_cast<uint16_t>(lanes);
    }
    
    // 静态工厂方法
    static DataType Int(int bits, int lanes = 1) {
        return DataType(kInt, bits, lanes);
    }
    static DataType Bool(int lanes = 1) {
        return DataType(kUInt, 1, lanes);  // bool = uint1
    }
    static DataType Handle(int bits = 64, int lanes = 1) {
        return DataType(kHandle, bits, lanes);
    }
    static DataType Void() {
        return DataType(kHandle, 0, 0);    // void = handle0x0
    }
    
    // 访问器
    int c() const { return static_cast<int>(data_.c_); }
    int b() const { return static_cast<int>(data_.b_); }
    int a() const { return static_cast<int>(data_.a_); }
    
    // 类型判断谓词
    bool IsScalar() const { return a() == 1; }
    bool IsBool() const { return c() == kUInt && b() == 1; }
    bool IsFloat() const { return c() == kFloat; }
    bool IsInt() const { return c() == kInt; }
    bool IsUint() const { return c() == kUInt; }
    bool IsVoid() const { return c() == kHandle && b() == 0 && a() == 0; }
    bool IsHandle() const { return c() == kHandle && !IsVoid(); }

private:
    Dt data_{};
};
```

### 3.3 字符串转换系统

**字符串格式**: `类型名[位宽][x通道数]`

| 字符串 | 类型描述 | Dt 表示 | 说明 |
|--------|----------|---------|------|
| `bool` | 布尔类型 | `{1,1,1}` | 特殊情况：uint1 |
| `int32` | 32位有符号整数 | `{0,32,1}` | 标准整数 |
| `uint64` | 64位无符号整数 | `{1,64,1}` | 长整数 |
| `float32` | 32位浮点数 | `{2,32,1}` | 单精度浮点 |
| `int32x4` | 4通道32位整数向量 | `{0,32,4}` | SIMD向量 |
| `handle64` | 64位句柄 | `{3,64,1}` | 指针类型 |

**解析实现**:
```cpp
Dt StrToDt(std::string_view s) {
    Dt t;
    t.b_ = 32;  // 默认32位
    t.a_ = 1;   // 默认1通道
    
    // 特殊情况：bool
    if (s == "bool") {
        t.c_ = DataType::kUInt;
        t.b_ = 1;
        t.a_ = 1;
        return t;
    }
    
    // 解析类型前缀
    const char* scan;
    if (s.substr(0, 3) == "int") {
        t.c_ = DataType::kInt;
        scan = s.data() + 3;
    } else if (s.substr(0, 4) == "uint") {
        t.c_ = DataType::kUInt;
        scan = s.data() + 4;
    } else if (s.substr(0, 5) == "float") {
        t.c_ = DataType::kFloat;
        scan = s.data() + 5;
    } else {
        throw std::runtime_error("unknown type " + std::string(s));
    }
    
    // 解析位宽
    char* x;
    uint8_t bits = static_cast<uint8_t>(strtoul(scan, &x, 10));
    if (bits != 0) t.b_ = bits;
    
    // 解析通道数 (x后面的数字)
    char* end = x;
    if (*x == 'x') {
        t.a_ = static_cast<uint16_t>(strtoul(x + 1, &end, 10));
    }
    
    // 验证完整性
    if (end != s.data() + s.length()) {
        throw std::runtime_error("invalid type format: " + std::string(s));
    }
    
    return t;
}
```

## 4. 核心值类型

### 4.1 Any - 基础值容器

`Any` 是整个值系统的基础，提供类型检查和转换的核心功能。

```cpp
class Any {
protected:
    Value value_;
    
public:
    constexpr Any() noexcept : value_({0,0,TypeIndex::Null}) {}
    constexpr Any(Value value) noexcept : value_(value) {}
    
    // 基础类型检查
    bool IsNull() const noexcept { 
        return value_.t == TypeIndex::Null; 
    }
    bool IsInt() const noexcept { 
        return value_.t == TypeIndex::Int; 
    }
    bool IsFloat() const noexcept { 
        return value_.t == TypeIndex::Float; 
    }
    bool IsStr() const noexcept { 
        return value_.t == TypeIndex::Str; 
    }
    bool IsDataType() const noexcept { 
        return value_.t == TypeIndex::DataType || value_.t == TypeIndex::Str; 
    }
    bool IsObject() const noexcept { return value_.t >= TypeIndex::Object; }
    
    // 模板化类型检查
    template<typename T>
    bool Is() const noexcept;
    
    // 类型转换
    template<typename T>
    T As() const;          // 安全转换（带检查）
    
    template<typename T>
    T As_() const noexcept; // 快速转换（无检查）
    
    // 访问底层数据
    constexpr const Value& value() const noexcept { return value_; }
    int32_t T() const noexcept { return value_.t; }
};
```

### 4.2 Any 的类型检查机制详解

#### 4.2.1 Is<T>() 的实现原理

`Is<T>()` 方法使用了高级的 C++ 模板元编程技术，通过 SFINAE (Substitution Failure Is Not An Error) 实现编译时类型分发：

```cpp
template<typename T>
bool Is() const noexcept {
    using RawType = std::remove_cv_t<std::remove_reference_t<T>>;
    return Is<RawType>(std::is_base_of<object_r, RawType>{});
}
```

**关键技术点**:

1. **类型清理**: `std::remove_cv_t<std::remove_reference_t<T>>` 移除 const、volatile 和引用修饰符
2. **类型特征检测**: `std::is_base_of<object_r, RawType>{}` 检测 T 是否继承自 object_r
3. **标签分发**: 基于类型特征选择不同的重载函数

**分发机制**:
```cpp
// 对于基础类型（不继承自 object_r）
template<typename T>
bool Is(std::false_type) const noexcept {
    if constexpr(std::is_same_v<T, int>) {
        return IsInt();
    } else if constexpr(std::is_same_v<T, float>) {
        return IsFloat();
    } else if constexpr(std::is_same_v<T, std::string>) {
        return IsStr();
    } else if constexpr(std::is_same_v<T, Dt>) {
        return value_.t == TypeIndex::DataType;
    }
    return false;
}

// 对于对象类型（继承自 object_r）
template<typename T>
bool Is(std::true_type) const noexcept {
    if (!IsObject()) return false;
    auto* object = static_cast<object_t*>(value_.u.v_pointer);
    if (!object) return false;
    
    if constexpr (std::is_base_of_v<object_r, T>) {
        if constexpr (std::is_same_v<T, object_r>) {
            return true;  // 任何对象都是 object_r
        } else {
            // 提取具体的对象类型
            using NodeType = typename std::remove_pointer_t<
                decltype(std::declval<T>().operator->())>;
            return object->template IsType<NodeType>();
        }
    }
    
    return false;
}
```

**C++ 语法解析**:

1. **`if constexpr`**: C++17 编译时条件分支，未选中的分支不会被编译
2. **`std::is_same_v`**: 类型相等性检查的简化语法
3. **`std::is_base_of_v`**: 继承关系检查的简化语法  
4. **`decltype(std::declval<T>())`**: 获取 T() 表达式的类型，无需实际构造对象
5. **`typename std::remove_pointer_t`**: 移除指针类型获取指向的类型

#### 4.2.2 特化的 Is<T>() 实现

对于常用类型，系统提供了特化版本以优化性能：

```cpp
// 64位整数特化
template<> inline bool Any::Is<int64_t>() const noexcept { 
    return value_.t == TypeIndex::Int; 
}

// double 类型特化（支持 int -> double 隐式转换）
template<> inline bool Any::Is<double>() const noexcept { 
    return value_.t == TypeIndex::Float || value_.t == TypeIndex::Int; 
}

// C 字符串特化
template<> inline bool Any::Is<const char*>() const noexcept { 
    return value_.t == TypeIndex::Str; 
}

// 数据类型特化
template<> inline bool Any::Is<Dt>() const noexcept {
    return value_.t == TypeIndex::DataType;
}

// void 指针特化（任何对象都可以转换为 void*）
template<> inline bool Any::Is<void*>() const noexcept { 
    return value_.t >= TypeIndex::Object; 
}
```

### 4.3 Any 的类型转换机制详解

#### 4.3.1 As<T>() 安全转换

```cpp
template<typename T>
T As() const {
    if (!Is<T>()) {
        throw std::runtime_error("Type Mismatch in Conversion");
    }
    return As_<T>();
}
```

**安全特性**:
- 先进行类型检查，确保转换安全
- 类型不匹配时抛出异常，避免未定义行为
- 适用于用户接口，提供完整的错误处理

#### 4.3.2 As_<T>() 快速转换

```cpp
template<typename T>
T As_() const noexcept {
    using RawType = std::remove_cv_t<std::remove_reference_t<T>>;
    return As_<RawType>(std::is_base_of<object_r, RawType>{});
}
```

**性能特性**:
- 无类型检查，直接进行数据转换
- `noexcept` 保证不抛出异常
- 适用于性能敏感的内部代码

**分发实现**:
```cpp
// 基础类型转换
template<typename T>
T As_(std::false_type) const noexcept {
    if constexpr(std::is_same_v<T, int>) {
        return value_.u.v_int;
    } else if constexpr(std::is_same_v<T, float>) {
        return value_.u.v_float;
    } else if constexpr(std::is_same_v<T, std::string>) {
        return std::string(value_.u.v_str);
    } else if constexpr(std::is_same_v<T, Dt>) {
        return value_.u.v_datatype;
    }
    return T();  // 默认构造
}

// 对象类型转换
template<typename T>
T As_(std::true_type) const noexcept {
    auto* ptr = static_cast<object_t*>(value_.u.v_pointer);
    if (!ptr) return T();
    return T(object_p<object_t>(ptr));  // 构造智能指针包装
}
```

#### 4.3.3 关键的 As_<T>() 特化

只为具有特殊转换逻辑的类型提供特化：

```cpp
// double 转换（支持 int -> double 自动类型提升）
template<> inline double Any::As_<double>() const noexcept {
    if (value_.t == TypeIndex::Int) {
        return static_cast<double>(value_.u.v_int);  // 整数自动提升为浮点数
    }
    return value_.u.v_float;
}

// void 指针转换（对象类型的通用接口）
template<> inline void* Any::As_<void*>() const noexcept {
    return value_.u.v_pointer;
}
```

**特化原则**: 只为需要特殊转换逻辑的类型提供特化。大部分基础类型（如 int64_t、const char* 等）通过通用模板已能正确处理，无需额外特化。

### 4.4 McView - 轻量级值视图

`McView` 继承自 `Any`，提供只读访问，无生命周期管理：

```cpp
class McView : public Any {
public:
    using Any::Any;  // 继承父类构造函数
    
    constexpr McView() noexcept : Any() {}
    constexpr McView(Value value) noexcept: Any(value) {}
    constexpr McView(const Value* value) noexcept : Any(*value) {}
    
    // 从对象引用构造
    template <typename R,
              typename = typename std::enable_if<std::is_base_of<object_r, R>::value>::type>
    McView(const R& value) noexcept {
        *this = McView(McValue(value));
    }
    
    // 默认拷贝/移动语义（浅拷贝）
    McView(const McView&) noexcept = default;
    McView& operator=(const McView&) noexcept = default;
    McView(McView&& other) noexcept = default;
    McView& operator=(McView&& other) noexcept = default;
    
    // 从 McValue 构造
    McView(const McValue& value) noexcept;
};
```

**设计特点**:
- **无生命周期管理**: 析构函数不释放任何资源
- **零拷贝**: 只复制 Value 结构本身（16字节）
- **高性能**: 适用于函数参数传递和临时访问

### 4.5 McValue - 完整值类型

`McValue` 扩展 `Any` 提供完整的 RAII 语义和生命周期管理。这种扩展的核心目的是解决 `Any` 作为轻量级视图无法安全管理资源的问题。

#### 4.5.1 为什么需要 McValue？

**核心问题：所有权和生命周期**

```cpp
// Any 的问题：无法安全返回拥有资源的值
Any CreateStringAny() {
    Any result;
    result.value_.u.v_str = new char[6];  // 分配内存
    strcpy(result.value_.u.v_str, "hello");
    result.value_.t = TypeIndex::Str;
    return result;  // 问题：谁负责释放这块内存？
}

// McValue 的解决方案：自动资源管理
McValue CreateStringValue() {
    return McValue("hello");  // McValue 拥有字符串，析构时自动释放
}
```

**所有权语义对比**：

| 方面 | Any | McValue |
|------|-----|---------|
| **所有权** | 借用（borrow） | 拥有（own） |
| **生命周期** | 不管理 | RAII 管理 |
| **拷贝语义** | 浅拷贝（危险） | 深拷贝（安全） |
| **用途** | 只读视图、参数传递 | 值存储、返回值 |
| **性能** | 零开销 | 管理开销 |

#### 4.5.2 McValue 的完整实现

`McValue` 提供完整的值语义和自动资源管理：

```cpp
class McValue : public Any {
public:
    McValue() noexcept = default;
    ~McValue() { Clean(); }
    
    // 基础类型构造函数
    McValue(int32_t value) noexcept {
        value_.u.v_int = value;
        value_.t = TypeIndex::Int;
    }
    
    McValue(int64_t value) noexcept {
        value_.u.v_int = value;
        value_.t = TypeIndex::Int;
    }
    
    McValue(double value) noexcept {
        value_.u.v_float = value;
        value_.t = TypeIndex::Float;
    }
    
    McValue(const char* str) {
        if (str) {
            size_t len = strlen(str);
            value_.u.v_str = new char[len+1];
            strcpy(value_.u.v_str, str);
            value_.t = TypeIndex::Str;
        } else {
            value_.t = TypeIndex::Null;
            value_.u.v_str = nullptr;
        }
    }
    
    McValue(const std::string& str) : McValue(str.c_str()) {}
    
    McValue(Dt datatype) noexcept {
        value_.u.v_datatype = datatype;
        value_.t = TypeIndex::DataType;
    }
    
    McValue(DataType datatype) : McValue(datatype.data_) {}
    
    // 对象构造函数
    template<typename T,
             typename = typename std::enable_if<std::is_base_of<object_t, T>::value>::type>
    McValue(object_p<T> obj) noexcept {
        if (obj.get()) {
            value_.t = obj->Index();           // 使用对象的实际类型索引
            value_.u.v_pointer = obj.get();
            obj.get()->IncCounter();          // 增加引用计数
        } else {
            value_.t = TypeIndex::Null;
            value_.u.v_pointer = nullptr;
        }
    }
    
    template <typename R,
              typename = typename std::enable_if<std::is_base_of<object_r, R>::value>::type>
    McValue(R value) noexcept {  // R 是 object_r 派生类
        if (value.data_.data_) {
            value_.t = value.data_.data_->Index();
            value_.u.v_pointer = value.data_.data_;
            value.data_.data_ = nullptr;      // 移动所有权，避免重复释放
        } else {
            value_.t = TypeIndex::Null;
            value_.u.v_pointer = nullptr;
        }
    }
    
    // 拷贝/移动语义
    McValue(const McValue& other) { CopyFrom(other); }
    McValue& operator=(const McValue& other) {
        if (this != &other) {
            Clean();
            CopyFrom(other);
        }
        return *this;
    }
    
    McValue(McValue&& other) noexcept {
        value_ = other.value_;
        other.value_.t = TypeIndex::Null;
        other.value_.u.v_pointer = nullptr;
    }
    
    McValue& operator=(McValue&& other) noexcept {
        if (this != &other) {
            Clean();
            value_ = other.value_;
            other.value_.t = TypeIndex::Null;
            other.value_.u.v_pointer = nullptr;
        }
        return *this;
    }
    
    // 特殊操作
    template<typename T>
    T MoveTo() {
        T result = As<T>();
        value_.t = TypeIndex::Null;
        value_.u.v_pointer = nullptr;
        return result;
    }
    
    void AsValue(Value* value) noexcept;  // 转移到 C API

private:
    void Clean() noexcept;
    void CopyFrom(const Any& other);
};
```

#### 4.5.3 资源管理的核心机制

**1. 自动资源清理**:
```cpp
void Clean() noexcept {
    if (value_.t == TypeIndex::Str) {
        delete[] value_.u.v_str;              // 释放字符串内存
    } else if (value_.t >= TypeIndex::Object) {
        if (value_.u.v_pointer) {
            static_cast<object_t*>(value_.u.v_pointer)->DecCounter();  // 减少引用计数
        }
    }
    value_.t = TypeIndex::Null;
    value_.u.v_pointer = nullptr;
}
```

**2. 深拷贝保证安全性**:
```cpp
void CopyFrom(const Any& other) {
    value_.t = other.T();
    
    switch (value_.t) {
    case TypeIndex::Str: {
        const char* str = other.As_<const char*>();
        if (str) {
            size_t len = strlen(str);
            value_.u.v_str = new char[len+1];  // 深拷贝字符串
            strcpy(value_.u.v_str, str);
        }
        break;
    }
    default:
        if (other.IsObject()) {
            object_t* obj = static_cast<object_t*>(other.value_.u.v_pointer);
            value_.u.v_pointer = obj;
            if (obj) obj->IncCounter();       // 正确管理引用计数
        }
        break;
    }
}
```

**3. 与对象系统的正确集成**:
```cpp
// 对象构造 - 正确增加引用计数
template<typename T>
McValue(object_p<T> obj) noexcept {
    if (obj.get()) {
        value_.t = obj->Index();           // 使用对象的实际类型索引
        value_.u.v_pointer = obj.get();
        obj.get()->IncCounter();          // 关键：增加引用计数
    }
}

// 对象引用移动构造 - 转移所有权
template <typename R>
McValue(R value) noexcept {  // R 是 object_r 派生类
    if (value.data_.data_) {
        value_.t = value.data_.data_->Index();
        value_.u.v_pointer = value.data_.data_;
        value.data_.data_ = nullptr;      // 关键：转移所有权，避免重复释放
    }
}
```

#### 4.5.4 McValue vs Any 的实际应用场景

**错误的 Any 使用**：
```cpp
// 危险：返回拥有资源的 Any
Any BadCreateObject() {
    auto obj = MakeObject<MyObject>();
    Any result;
    result.value_.t = obj->Index();
    result.value_.u.v_pointer = obj.get();
    return result;  // obj 析构，引用计数减1，可能导致悬空指针！
}

// 危险：Any 的浅拷贝
Any a = some_string_any;
Any b = a;  // 两个 Any 指向同一字符串，谁负责释放？
```

**正确的 McValue 使用**：
```cpp
// 安全：McValue 自动管理生命周期
McValue CreateObject() {
    auto obj = MakeObject<MyObject>();
    return McValue(obj);  // 构造时正确增加引用计数
}

// 安全：McValue 的深拷贝
McValue a("hello");
McValue b = a;  // b 拥有独立的字符串副本
```

**正确的 Any 使用**：
```cpp
// 正确：Any 用于只读访问
void ProcessValue(McView view) {  // McView 继承自 Any
    if (view.IsStr()) {
        std::cout << view.As<const char*>() << std::endl;  // 只读访问
    }
}  // view 析构，但不影响原始数据

// 正确：Any 用于临时访问已有数据
McValue storage("persistent data");
Any temp_view = storage;  // 临时视图，storage 管理生命周期
```

### 4.6 Parameters - 参数容器

专门用于函数调用的轻量级参数容器：

```cpp
class Parameters {
public:
    constexpr Parameters() : item_(nullptr), size_(0) {}
    
    // 从数组构造
    constexpr explicit Parameters(const Any* begin, size_t len)
        : item_(const_cast<Any*>(begin)), size_(len) {}
    
    // 从初始化列表构造
    constexpr Parameters(std::initializer_list<McView> gs)
        : item_(const_cast<Any*>(static_cast<const Any*>(gs.begin())))
        , size_(gs.size()) {}
    
    // 容器接口
    constexpr int size() const { return size_; }
    constexpr const Any* begin() const { return item_; }
    constexpr const Any* end() const { return item_ + size_; }
    
    // 元素访问
    inline const Any& operator[](int64_t i) const { return *(item_ + i); }
    inline Any& operator[](int64_t i) { return *(item_ + i); }
    
    bool empty() { return size_ == 0; }

private:
    Any* item_;      // 参数数组指针（不拥有）
    size_t size_;    // 参数个数
};
```

**设计特点**:
- **零拷贝**: 只存储指针和大小，不拥有数据
- **灵活构造**: 支持数组指针和初始化列表
- **STL 兼容**: 提供标准容器接口

## 5. 系统集成与使用

### 5.1 DataType 与值系统集成

```cpp
// 在 runtime_value.cc 中的特化实现
template<>
bool Any::Is<DataType>() const noexcept {
    if (value_.t != TypeIndex::DataType && value_.t != TypeIndex::Str) {
        return false;
    }
    return true;
}

template<>
DataType Any::As_<DataType>() const noexcept {
    switch (value_.t) {
        case TypeIndex::Str: {
            return DataType(StrToDt(As_<const char*>()));  // 字符串自动转换
        }
        default:
            return DataType(value_.u.v_datatype);         // 直接访问
    }
}
```

### 5.2 比较和哈希支持

```cpp
// 值相等性比较
bool operator==(const McValue& u, const McValue& v) {
    if (u.T() != v.T()) return false;
    
    switch (u.T()) {
        case TypeIndex::Null: return true;
        case TypeIndex::Int: return u.As<int64_t>() == v.As<int64_t>();
        case TypeIndex::Float: return u.As<double>() == v.As<double>();
        case TypeIndex::Str: return strcmp(u.As<const char*>(), v.As<const char*>()) == 0;
        default:
            if (u.T() >= TypeIndex::Object) {
                // 对象比较指针地址
                return static_cast<object_t*>(u.value_.u.v_pointer)
                    == static_cast<object_t*>(v.value_.u.v_pointer);
            }
            return false;
    }
}

// 标准库哈希支持
namespace std {
template<>
struct hash<mc::runtime::McValue> {
    size_t operator()(const mc::runtime::McValue& value) const {
        switch(value.T()) {
            case mc::runtime::TypeIndex::Null: return 0;
            case mc::runtime::TypeIndex::Int: 
                return std::hash<int64_t>{}(value.As_<int64_t>());
            case mc::runtime::TypeIndex::Float: 
                return std::hash<double>{}(value.As_<double>());
            case mc::runtime::TypeIndex::Str: 
                return std::hash<std::string>{}(
                    value.As_<const char*>() ? value.As_<const char*>() : ""
                );
            default:
                if (value.IsObject()) {
                    void* obj = value.As_<void*>();
                    return obj ? std::hash<void*>{}(obj) : 0;
                }
                return 0;
        }
    }
};
}
```

### 5.3 C API 互操作

```cpp
// 转移到 C API（所有权转移）
void McValue::AsValue(Value* value) noexcept {
    if (value_.t == TypeIndex::Str && value_.u.v_str != nullptr) {
        // 字符串需要特殊处理：创建新副本
        const char* old_str = value_.u.v_str;
        int32_t len = strlen(old_str);
        
        char* new_str = new char[len + 1];
        memcpy(new_str, old_str, len);
        new_str[len] = '\0';
        
        value->t = TypeIndex::Str;
        value->p = len;               // 设置长度信息
        value->u.v_str = new_str;
    } else {
        *value = value_;             // 直接复制
    }
    
    // 转移所有权
    value_.t = TypeIndex::Null;
    value_.u.v_pointer = nullptr;
}

// 从 C API 创建
Object AsObject(Any value) {
    if (value.IsObject()) {
        Object node(object_p<object_t>(
                  static_cast<object_t*>(value.As_<void*>())));
        return node;
    }
    throw std::runtime_error("NotObject");
}
```

## 6. 完整使用示例

### 6.1 基础类型操作

```cpp
// 创建各种类型的值
McValue int_val(42);
McValue float_val(3.14);
McValue str_val("hello world");
McValue bool_val(DataType::Bool());
McValue obj_val(MakeObject<MyObject>());

// 类型检查
std::cout << "int_val.Is<int64_t>(): " << int_val.Is<int64_t>() << std::endl;  // true
std::cout << "int_val.Is<double>(): " << int_val.Is<double>() << std::endl;    // true (自动提升)
std::cout << "str_val.Is<const char*>(): " << str_val.Is<const char*>() << std::endl;  // true

// 类型转换
int64_t i = int_val.As<int64_t>();
double d = int_val.As<double>();        // 自动提升
std::string s = str_val.As<std::string>();
```

### 6.2 函数调用系统

```cpp
// 函数类型定义
using Function = std::function<McValue(Parameters)>;

// 定义一个加法函数
Function add_func = [](Parameters params) -> McValue {
    if (params.size() != 2) {
        throw std::runtime_error("Add function needs exactly 2 parameters");
    }
    
    // 支持 int + int
    if (params[0].Is<int64_t>() && params[1].Is<int64_t>()) {
        return McValue(params[0].As<int64_t>() + params[1].As<int64_t>());
    }
    
    // 支持 double + double（包括 int 自动提升）
    if (params[0].Is<double>() && params[1].Is<double>()) {
        return McValue(params[0].As<double>() + params[1].As<double>());
    }
    
    throw std::runtime_error("Unsupported parameter types for add");
};

// 调用函数
McValue result1 = add_func({McView(10), McView(20)});           // int + int = 30
McValue result2 = add_func({McView(3.14), McView(2.86)});       // double + double = 6.0
McValue result3 = add_func({McView(10), McView(2.5)});          // int + double = 12.5

std::cout << "Results: " << result1.As<int64_t>() << ", " 
          << result2.As<double>() << ", " << result3.As<double>() << std::endl;
```

### 6.3 类型描述和验证

```cpp
// 创建类型描述
DataType int32_type = DataType::Int(32);
DataType float64_vec = DataType(DataType::kFloat, 64, 4);  // float64x4
DataType bool_type = DataType::Bool();

// 字符串转换
std::string type_str = DtToStr(float64_vec.data_);
std::cout << "Vector type: " << type_str << std::endl;  // "float64x4"

// 从字符串解析
DataType parsed = DataType(StrToDt("int32x8"));
std::cout << "Parsed type - Code: " << parsed.c() 
          << ", Bits: " << parsed.b() 
          << ", Lanes: " << parsed.a() << std::endl;  // 0, 32, 8

// 类型判断
if (bool_type.IsBool()) {
    std::cout << "Boolean type detected" << std::endl;
}

if (float64_vec.IsFloat() && !float64_vec.IsScalar()) {
    std::cout << "Float vector type detected" << std::endl;
}
```

### 6.4 异构容器和类型安全

```cpp
// 异构容器
std::vector<McValue> heterogeneous_data;
heterogeneous_data.emplace_back(42);
heterogeneous_data.emplace_back(3.14);
heterogeneous_data.emplace_back("hello");
heterogeneous_data.emplace_back(DataType::Int(64));
heterogeneous_data.emplace_back(MakeObject<MyObject>("test"));

// 类型安全处理
for (const auto& value : heterogeneous_data) {
    std::cout << "Type: " << value.T() << ", Value: ";
    
    if (value.Is<int64_t>()) {
        std::cout << "Integer(" << value.As<int64_t>() << ")";
    } else if (value.Is<double>()) {
        std::cout << "Float(" << value.As<double>() << ")";
    } else if (value.Is<const char*>()) {
        std::cout << "String(" << value.As<const char*>() << ")";
    } else if (value.Is<DataType>()) {
        DataType dt = value.As<DataType>();
        std::cout << "DataType(" << DtToStr(dt.data_) << ")";
    } else if (value.IsObject()) {
        std::cout << "Object(" << value.As<void*>() << ")";
    }
    
    std::cout << std::endl;
}
```

### 6.5 性能优化示例

```cpp
// 高性能函数调用（避免类型检查）
Function fast_multiply = [](Parameters params) -> McValue {
    // 假设已知参数类型，使用快速转换
    int64_t a = params[0].As_<int64_t>();  // 无检查转换
    int64_t b = params[1].As_<int64_t>();
    return McValue(a * b);
};

// 使用 McView 避免拷贝
void ProcessValues(const std::vector<McValue>& values) {
    for (const auto& val : values) {
        ProcessSingleValue(McView(val));  // 零拷贝传递
    }
}

void ProcessSingleValue(McView view) {
    // 只读访问，无生命周期管理开销
    if (view.IsInt()) {
        std::cout << "Processing integer: " << view.As_<int64_t>() << std::endl;
    }
}
```

## 7. 性能考虑和最佳实践

### 7.1 性能特点

- **内存布局**: Value 结构 16 字节，缓存友好
- **类型检查**: 基础类型 O(1)，对象类型 O(继承深度)
- **转换开销**: 基础类型零开销，对象类型涉及智能指针构造
- **字符串处理**: 深拷贝开销，但支持移动语义优化

### 7.2 最佳实践

1. **参数传递**: 使用 `McView` 进行只读访问
2. **性能敏感路径**: 使用 `As_()` 避免类型检查
3. **容器存储**: 使用 `McValue` 确保正确的生命周期管理
4. **大对象**: 优先使用移动语义（`std::move`, `MoveTo()`）
5. **类型验证**: 在系统边界进行完整类型检查，内部使用快速路径

### 7.3 内存管理

- **自动管理**: 依赖 RAII，避免手动内存操作
- **引用计数**: 对象类型自动管理生命周期
- **字符串拷贝**: 注意深拷贝开销，考虑字符串池优化
- **资源泄漏**: 正确使用移动语义避免资源泄漏

这个完整的运行时值系统为 MC 提供了类型安全、高性能的统一数据表示层，支持从简单基础类型到复杂对象类型的完整生态系统。