# MC 运行时值系统文档

## 概述

MC 运行时值系统提供了一个统一的值表示和操作框架，用于在运行时处理不同类型的数据。该系统由以下核心组件组成：

### 核心值类型
- **`Any`**: 基础的值容器，提供类型检查和转换功能
- **`McView`**: 轻量级的值视图，用于只读访问，无生命周期管理
- **`McValue`**: 完整的值类型，具有 RAII 语义和完整的生命周期管理

### 支持组件
- **`Parameters`**: 参数容器，用于函数调用时的参数传递
- **`DataType/Dt`**: 数据类型描述系统，支持多种基础数据类型的表示

## 系统架构

### 核心设计理念

1. **类型统一**: 使用联合体 `Value` 统一表示所有运行时值
2. **层次设计**: Any → McView → McValue，逐层增加功能
3. **零开销抽象**: 基于联合体的高效内存布局
4. **类型安全**: 运行时类型检查和安全转换

### 数据表示层

系统基于 C API 层的 `Value` 结构：

```cpp
// 来自 c_api.h 的底层数据结构
struct Value {
    union {
        int64_t v_int;
        double v_float;
        char* v_str;
        void* v_pointer;
        Dt v_datatype;
    } u;
    int32_t p;    // 扩展信息（如字符串长度）
    int32_t t;    // 类型索引
};
```

## 核心类详解

### Any - 基础值容器

`Any` 类是整个值系统的基础，提供类型检查和转换的核心功能。

```cpp
class Any {
protected:
    Value value_;
    
public:
    constexpr Any() noexcept : value_({0,0,TypeIndex::Null}) {}
    
    // 类型检查
    bool IsNull() const noexcept;
    bool IsInt() const noexcept;
    bool IsFloat() const noexcept;
    bool IsStr() const noexcept;
    bool IsObject() const noexcept;
    
    // 模板化类型检查
    template<typename T>
    bool Is() const noexcept;
    
    // 类型转换
    template<typename T>
    T As() const;  // 带检查的安全转换
    
    template<typename T>
    T As_() const noexcept;  // 无检查的快速转换
};
```

#### 类型检查机制

**基础类型检查**：
```cpp
bool IsInt() const noexcept {
    return value_.t == TypeIndex::Int;
}
bool IsObject() const noexcept {
    return value_.t >= TypeIndex::Object;  // 对象类型索引都 >= 0
}
```

**模板化类型检查**：
```cpp
template<typename T>
bool Is() const noexcept {
    using RawType = std::remove_cv_t<std::remove_reference_t<T>>;
    return Is<RawType>(std::is_base_of<object_r, RawType>{});
}
```

这个设计使用 SFINAE 技术根据类型特征分发到不同的实现：

```cpp
// 对于基础类型（非 object_r 派生）
template<typename T>
bool Is(std::false_type) const noexcept {
    if constexpr(std::is_same_v<T, int>) {
        return IsInt();
    } else if constexpr(std::is_same_v<T, float>) {
        return IsFloat();
    }
    // ... 其他基础类型
}

// 对于对象类型（object_r 派生）
template<typename T>
bool Is(std::true_type) const noexcept {
    if (!IsObject()) return false;
    auto* object = static_cast<object_t*>(value_.u.v_pointer);
    if (!object) return false;
    
    // 使用对象系统的运行时类型检查
    using NodeType = typename std::remove_pointer_t<
        decltype(std::declval<T>().operator->())>;
    return object->template IsType<NodeType>();
}
```

#### 类型转换机制

**安全转换**（带类型检查）：
```cpp
template<typename T>
T As() const {
    if (!Is<T>()) {
        throw std::runtime_error("Type Mismatch in Conversion");
    }
    return As_<T>();
}
```

**快速转换**（无类型检查）：
```cpp
template<typename T>
T As_() const noexcept {
    using RawType = std::remove_cv_t<std::remove_reference_t<T>>;
    return As_<RawType>(std::is_base_of<object_r, RawType>{});
}
```

#### 特化的转换实现

```cpp
// 基础类型转换
template<> inline int64_t Any::As_<int64_t>() const noexcept {
    return value_.u.v_int;
}

template<> inline double Any::As_<double>() const noexcept {
    if (value_.t == TypeIndex::Int) {
        return static_cast<double>(value_.u.v_int);  // 自动类型提升
    }
    return value_.u.v_float;
}

// 对象类型转换
template<typename T>
T As_(std::true_type) const noexcept {
    auto* ptr = static_cast<object_t*>(value_.u.v_pointer);
    if (!ptr) return T();
    return T(object_p<object_t>(ptr));  // 构造智能指针
}
```

### McView - 轻量级值视图

`McView` 是 `Any` 的轻量级封装，用于只读访问，不管理值的生命周期。

```cpp
class McView : public Any {
public:
    constexpr McView() noexcept : Any() {}
    constexpr McView(Value value) noexcept: Any(value) {}
    constexpr McView(const Value* value) noexcept : Any(*value) {}
    
    // 从对象引用构造
    template <typename R,
              typename = typename std::enable_if<std::is_base_of<object_r, R>::value>::type>
    McView(const R& value) noexcept;
    
    // 从 McValue 构造
    McView(const McValue& value) noexcept;
    
    // 默认拷贝/移动语义
    McView(const McView&) noexcept = default;
    McView& operator=(const McView&) noexcept = default;
};
```

**设计特点**：
- **无生命周期管理**: 不增减引用计数，不释放内存
- **零拷贝**: 只复制 `Value` 结构本身
- **高性能**: 适用于临时访问和参数传递

**使用场景**：
```cpp
void ProcessValue(McView view) {
    if (view.IsInt()) {
        std::cout << "Integer: " << view.As<int64_t>() << std::endl;
    }
    // view 析构时不会影响原始值
}

McValue original(42);
ProcessValue(original);  // 高效传递，无拷贝开销
```

### McValue - 完整值类型

`McValue` 是完整的值类型，提供 RAII 语义和完整的生命周期管理。

```cpp
class McValue : public Any {
public:
    McValue() noexcept = default;
    ~McValue() { Clean(); }
    
    // 多种构造函数
    McValue(int64_t value) noexcept;
    McValue(double value) noexcept;
    McValue(const char* str);
    McValue(const std::string& str);
    
    // 对象构造
    template<typename T>
    McValue(object_p<T> obj) noexcept;
    
    template <typename R>
    McValue(R value) noexcept;  // object_r 及其派生类
    
    // 拷贝/移动语义
    McValue(const McValue& other);
    McValue& operator=(const McValue& other);
    McValue(McValue&& other) noexcept;
    McValue& operator=(McValue&& other) noexcept;
    
    // 特殊操作
    template<typename T>
    T MoveTo();  // 移动语义转换
    
    void AsValue(Value* value) noexcept;  // 转移到 C API
};
```

#### 生命周期管理

**清理机制**：
```cpp
void Clean() noexcept {
    if (value_.t == TypeIndex::Str) {
        delete[] value_.u.v_str;  // 释放字符串内存
    } else if (value_.t >= TypeIndex::Object) {
        if (value_.u.v_pointer) {
            static_cast<object_t*>(value_.u.v_pointer)->DecCounter();  // 减少引用计数
        }
    }
    value_.t = TypeIndex::Null;
    value_.u.v_pointer = nullptr;
}
```

**拷贝机制**：
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
            if (obj) obj->IncCounter();  // 增加引用计数
        }
        break;
    }
}
```

#### 对象构造的特殊处理

**智能指针构造**：
```cpp
template<typename T>
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
```

**对象引用构造**（移动语义）：
```cpp
template <typename R>
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
```

## 高级功能

### 类型转换和互操作

#### C API 互操作

```cpp
// 转移到 C API
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
```

#### 对象系统桥接

```cpp
Object AsObject(Any value) {
    if (value.IsObject()) {
        Object node(object_p<object_t>(
                  static_cast<object_t*>(value.As_<void*>())));
        return node;
    }
    throw std::runtime_error("NotObject");
}
```

### 比较和哈希

#### 相等性比较

```cpp
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
```

#### 哈希支持

```cpp
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

## 使用示例

### 基本使用

```cpp
// 创建不同类型的值
McValue int_val(42);
McValue float_val(3.14);
McValue str_val("hello");
McValue obj_val(MakeObject<MyObject>());

// 类型检查
if (int_val.Is<int64_t>()) {
    std::cout << "Integer: " << int_val.As<int64_t>() << std::endl;
}

// 类型转换（自动提升）
if (int_val.Is<double>()) {  // true，int 可以转换为 double
    std::cout << "As double: " << int_val.As<double>() << std::endl;
}
```

### 容器存储

```cpp
// 异构容器
std::vector<McValue> values;
values.emplace_back(42);
values.emplace_back(3.14);
values.emplace_back("hello");
values.emplace_back(MakeObject<MyObject>());

// 统一处理
for (const auto& val : values) {
    std::cout << "Type: " << val.T() << ", Value: " << val << std::endl;
}
```

### 函数参数

```cpp
// 使用 McView 作为函数参数（高效）
void ProcessValue(McView view) {
    if (view.IsInt()) {
        HandleInteger(view.As<int64_t>());
    } else if (view.IsObject()) {
        HandleObject(AsObject(view));
    }
}

// 使用 McValue 作为返回值
McValue CreateValue(const std::string& type, const std::string& data) {
    if (type == "int") {
        return McValue(std::stoll(data));
    } else if (type == "string") {
        return McValue(data);
    }
    return McValue();  // null
}
```

## 性能考虑

### 内存布局

- **Value 结构**: 16 字节（8字节联合体 + 4字节扩展 + 4字节类型）
- **零开销抽象**: Any/McView 只是薄包装
- **高效拷贝**: 基础类型拷贝开销极小

### 性能优化

1. **避免不必要的类型检查**: 使用 `As_()` 而不是 `As()` 在确定类型时
2. **使用 McView**: 对于只读访问避免生命周期管理开销
3. **移动语义**: 大对象使用 `std::move` 或 `MoveTo()`
4. **对象池**: 为频繁创建的对象类型使用对象池

### 最佳实践

1. **参数传递**: 使用 `McView` 作为函数参数
2. **返回值**: 使用 `McValue` 作为返回值
3. **容器存储**: 使用 `McValue` 在容器中存储异构数据
4. **类型检查**: 在性能敏感路径使用快速转换 `As_()`
5. **资源管理**: 依赖 RAII，避免手动内存管理

## 与对象系统的集成

运行时值系统与对象系统深度集成：

- **类型索引统一**: 使用相同的 `TypeIndex` 枚举
- **引用计数集成**: 自动管理对象生命周期
- **类型检查委托**: 对象类型检查委托给对象系统
- **转换支持**: 提供与对象系统的双向转换

这种设计使得值系统可以无缝地处理基础类型和复杂对象，为整个 MC 运行时提供了统一的数据表示层。

## 支持组件详解

### Parameters - 参数容器

`Parameters` 类是专门用于函数调用时参数传递的轻量级容器。

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
    
    // 访问操作
    inline const Any& operator[](int64_t i) const { return *(item_ + i); }
    inline Any& operator[](int64_t i) { return *(item_ + i); }
    
    bool empty() { return size_ == 0; }

private:
    Any* item_;      // 参数数组指针
    size_t size_;    // 参数个数
};
```

#### 设计特点

1. **轻量级设计**: 只包含指针和大小，不拥有数据
2. **零拷贝**: 直接引用外部数据，避免参数复制
3. **类型兼容**: 可以接受 `Any` 数组或 `McView` 初始化列表
4. **常量友好**: 支持编译时构造

#### 使用场景

```cpp
// 函数签名
using Function = std::function<McValue(Parameters)>;

// 直接传递参数
void CallFunction(Function func) {
    func({McView(42), McView(3.14), McView("hello")});
}

// 从数组构造
std::vector<McValue> args = {McValue(1), McValue(2), McValue(3)};
Parameters params(args.data(), args.size());

// 访问参数
for (int i = 0; i < params.size(); ++i) {
    if (params[i].IsInt()) {
        std::cout << "Arg " << i << ": " << params[i].As<int64_t>() << std::endl;
    }
}
```

### DataType/Dt - 数据类型描述系统

数据类型系统提供了对基础数据类型的完整描述和操作。

#### Dt 结构体

```cpp
struct Dt {
    uint8_t c_;   // 类型代码 (Code): Int, UInt, Float, Handle
    uint8_t b_;   // 位宽 (Bits): 8, 16, 32, 64
    uint16_t a_;  // 通道数 (Array/Lanes): 1, 2, 4, 8, ...
};
```

#### DataType 类

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
    DataType(int code, int bits, int lanes);
    
    // 静态工厂方法
    static DataType Int(int bits, int lanes = 1);
    static DataType Bool(int lanes = 1);     // uint1
    static DataType Handle(int bits = 64, int lanes = 1);
    static DataType Void();                  // handle0x0
    
    // 访问器
    int c() const { return static_cast<int>(data_.c_); }
    int b() const { return static_cast<int>(data_.b_); }
    int a() const { return static_cast<int>(data_.a_); }
    
    // 类型判断
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

#### 字符串转换系统

**字符串到类型转换**：
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
    if (s.substr(0, 3) == "int") {
        t.c_ = DataType::kInt;
        // 解析位宽和通道数...
    }
    // 类似处理 uint, float
    
    return t;
}
```

**类型到字符串转换**：
```cpp
std::string DtToStr(const Dt& t) {
    if (t.b_ == 0) return {};  // void类型
    
    std::ostringstream os;
    
    // bool特殊处理
    if (t.b_ == 1 && t.a_ == 1 && t.c_ == DataType::kUInt) {
        os << "bool";
        return os.str();
    }
    
    // 常规类型: 类型名 + 位宽 + 通道数
    os << _ToStr(t.c_) << static_cast<int>(t.b_);
    if (t.a_ != 1) {
        os << 'x' << static_cast<int>(t.a_);
    }
    
    return os.str();
}
```

#### 支持的数据类型

| 类型字符串 | 含义 | Dt表示 |
|-----------|------|--------|
| `bool` | 布尔类型 | `{kUInt, 1, 1}` |
| `int32` | 32位有符号整数 | `{kInt, 32, 1}` |
| `uint64` | 64位无符号整数 | `{kUInt, 64, 1}` |
| `float32` | 32位浮点数 | `{kFloat, 32, 1}` |
| `int32x4` | 4通道32位整数向量 | `{kInt, 32, 4}` |
| `handle64` | 64位句柄 | `{kHandle, 64, 1}` |

#### 使用示例

```cpp
// 创建数据类型
DataType int32_type = DataType::Int(32);
DataType float_vec = DataType(DataType::kFloat, 32, 4);  // float32x4
DataType bool_type = DataType::Bool();

// 字符串解析
DataType parsed = DataType(StrToDt("int64x2"));
std::cout << "Type: " << DtToStr(parsed.data_) << std::endl;  // "int64x2"

// 类型检查
if (bool_type.IsBool()) {
    std::cout << "This is a boolean type" << std::endl;
}

// 在值系统中使用
McValue dt_value(DataType::Int(64));
if (dt_value.IsDataType()) {
    DataType recovered = dt_value.As<DataType>();
    std::cout << "Bits: " << recovered.b() << std::endl;
}
```

#### 与值系统的集成

DataType 与值系统紧密集成：

```cpp
// 值系统中的类型检查
template<>
bool Any::Is<DataType>() const noexcept {
    return value_.t == TypeIndex::DataType || value_.t == TypeIndex::Str;
}

// 支持从字符串自动转换
template<>
DataType Any::As_<DataType>() const noexcept {
    switch (value_.t) {
        case TypeIndex::Str: {
            return DataType(StrToDt(As_<const char*>()));  // 字符串转换
        }
        default:
            return DataType(value_.u.v_datatype);         // 直接访问
    }
}
```

## 完整的系统集成

### 函数调用示例

```cpp
// 定义一个函数
Function add_func = [](Parameters params) -> McValue {
    if (params.size() != 2) {
        throw std::runtime_error("Need 2 parameters");
    }
    
    if (params[0].Is<int64_t>() && params[1].Is<int64_t>()) {
        return McValue(params[0].As<int64_t>() + params[1].As<int64_t>());
    }
    
    throw std::runtime_error("Type mismatch");
};

// 调用函数
McValue result = add_func({McView(42), McView(58)});
std::cout << "Result: " << result.As<int64_t>() << std::endl;  // 100
```

### 类型描述和验证

```cpp
// 创建带类型信息的值
McValue typed_value(DataType::Float(64));

// 验证和转换
if (typed_value.Is<DataType>()) {
    DataType dt = typed_value.As<DataType>();
    
    if (dt.IsFloat() && dt.b() == 64) {
        std::cout << "This is a 64-bit float type" << std::endl;
    }
    
    // 类型字符串表示
    std::string type_str = DtToStr(dt.data_);
    std::cout << "Type string: " << type_str << std::endl;  // "float64"
}
```

这个完整的运行时值系统为 MC 提供了强大而灵活的数据处理能力，支持从简单的基础类型到复杂的对象类型，以及高效的函数调用和参数传递机制。