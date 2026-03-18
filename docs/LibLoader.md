# 动态库加载系统详解 (LibLoader)

动态库加载系统是 MC 项目运行时架构的核心组件，它解决了一个关键问题：**如何在运行时动态加载共享库，并让其中的函数能够通过 FFI 系统无缝调用**。这个系统为 MC 编译器提供了强大的可扩展性和模块化能力。

## 系统架构概览

动态库加载系统采用分层抽象设计：

```
┌─────────────────────────────────────────────────────────────────┐
│                        Python 用户层                             │
│  test_func = module_loader_("./test_func.so")                   │
│  result = test_func.get_function("test_func")(3, 4)             │
└─────────────────────────┬───────────────────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────────────────┐
│                    Python FFI 层                               │
│  Module 对象包装、register_object、register_input_callback      │
└─────────────────────────┬───────────────────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────────────────┐
│                    C++ 抽象层                                   │
│  Module、ModuleNode、Library 抽象接口                          │
└─────────────────────────┬───────────────────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────────────────┐
│                 动态库实现层                                     │
│  LibraryModuleNode、DefaultLibray、函数注册表解析               │
└─────────────────────────┬───────────────────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────────────────┐
│                   系统调用层                                     │
│  dlopen、dlsym、函数符号解析                                   │
└─────────────────────────────────────────────────────────────────┘
```

## 1. 核心抽象接口设计

### 1.1 Library 抽象基类

**设计目标**：提供平台无关的动态库操作接口

```cpp
class Library : public object_t {
public:
    virtual ~Library() {}
    virtual void* GetSymbol(std::string_view name) = 0;  // 符号查找
};
```

**关键特点**：
- 纯虚函数接口，隐藏平台差异
- 继承自 `object_t`，支持引用计数管理
- 简洁的符号查找接口

### 1.2 ModuleNode 抽象基类

**设计目标**：定义模块的基本功能接口

```cpp
class ModuleNode : public object_t {
public:
    static constexpr const std::string_view NAME = "runtime.Module";
    static constexpr const uint32_t INDEX = TypeIndex::Module;

    virtual Function GetFunction(const std::string_view& name,
                                 const object_p<object_t>& ss) = 0;
    
    Function GetFunction(const std::string_view& name, bool use_imports = false);
};
```

**关键功能**：
- 抽象函数查找接口
- 支持模块导入机制（预留接口）
- 与 FFI 系统的 `Function` 类型集成

### 1.3 Module 用户接口

**设计目标**：为用户提供简洁的模块操作接口

```cpp
class Module : public object_r {
public:
    explicit Module(object_p<object_t> n) : object_r(std::move(n)) {}

    Function GetFunction(const std::string_view& name,
                         const object_p<object_t>& ss) {
        return operator->()->GetFunction(name, ss);
    }

    static Module Load(const std::string& filename);  // 静态加载方法
};
```

**设计优势**：
- 使用智能指针管理内存
- 提供便捷的函数访问接口
- 支持静态工厂方法

## 2. 动态库加载的完整流程

### 2.1 Python 端的模块加载

**用户代码**：
```python
# 1. 获取模块加载器
module_loader_ = matx_script_api.GetGlobal("runtime.ModuleLoader", True)

# 2. 加载动态库
test_module = module_loader_("./test_func.so")

# 3. 获取函数
test_func = test_module.get_function("test_func")

# 4. 调用函数
result = test_func(3, 4)
```

### 2.2 C++ 端的加载机制

**ModuleLoader 函数实现**：
```cpp
McValue ModuleLoader(Parameters gs) {
    // 1. 创建动态库加载器
    auto n = MakeObject<DefaultLibray>();
    auto name = gs[0].As<const char*>();
    
    // 2. 初始化动态库（调用 dlopen）
    n->Init(name);
    
    // 3. 创建模块节点
    auto m = CreateModuleFromLibrary(n);
    
    return m;  // 返回 Module 对象
}
REGISTER_FUNCTION("runtime.ModuleLoader", ModuleLoader);
```

**CreateModuleFromLibrary 详细实现**：
```cpp
Module CreateModuleFromLibrary(object_p<Library> p) {
    // 1. 创建库模块节点
    auto n = MakeObject<LibraryModuleNode>(p);
    Module root = Module(n);
    
    // 2. 关键：设置模块上下文
    if (auto* ctx_addr = reinterpret_cast<void**>(
          p->GetSymbol(Symbol::ModuleCtx))) {
        *ctx_addr = root.operator->();  // 让模块能找到自己
    }
    
    return root;
}
```

**为什么需要设置模块上下文？**

这是一个关键的设计，让我们通过具体例子来理解：

**问题场景**：假设动态库中有一个函数需要调用同一个模块中的另一个函数，或者需要访问模块级别的资源。

**具体例子**：
```cpp
// 假设在 advanced_module.so 中有这样的函数
extern "C" void* __mc_module_ctx = nullptr;  // 模块上下文指针

namespace {
    // 函数1：创建一个数组
    int create_array__c_api(Value* args, int num_args, Value* ret_val, void* resource) {
        // 这个函数需要调用同模块中的 "get_default_size" 函数
        
        // 1. 获取模块上下文
        auto* module = static_cast<ModuleNode*>(__mc_module_ctx);
        
        // 2. 通过模块上下文获取同模块中的其他函数
        auto get_size_func = module->GetFunction("get_default_size", /*self*/);
        
        // 3. 调用同模块中的函数
        Parameters empty_params(nullptr, 0);
        McValue size_result = get_size_func(empty_params);
        int default_size = size_result.As<int>();
        
        // 4. 使用获取到的大小创建数组
        // ... 创建数组逻辑
        return 0;
    }
    
    int get_default_size__c_api(Value* args, int num_args, Value* ret_val, void* resource) {
        ret_val->t = TypeIndex::Int;
        ret_val->u.v_int = 100;  // 默认大小是100
        return 0;
    }
}
```

**没有模块上下文会发生什么？**
```cpp
// 错误的做法 - 没有模块上下文
int create_array__c_api(Value* args, int num_args, Value* ret_val, void* resource) {
    // ❌ 问题：如何获取同模块中的 "get_default_size" 函数？
    // ❌ 没有模块引用，无法调用同模块中的其他函数
    // ❌ 只能硬编码大小，失去了灵活性
    
    int hardcoded_size = 100;  // 只能硬编码
    // ... 创建数组逻辑
    return 0;
}
```

**设置模块上下文的过程**：
```cpp
Module CreateModuleFromLibrary(object_p<Library> p) {
    // 1. 创建模块节点
    auto n = MakeObject<LibraryModuleNode>(p);
    Module root = Module(n);
    
    // 2. 关键：设置模块上下文
    if (auto* ctx_addr = reinterpret_cast<void**>(
          p->GetSymbol(Symbol::ModuleCtx))) {  // 查找 "__mc_module_ctx"
        *ctx_addr = root.operator->();  // 将模块对象地址赋值给动态库中的全局变量
    }
    
    return root;
}
```

**模块上下文的实际用途**：

1. **模块内函数互相调用**：
   ```cpp
   // 动态库中的函数可以调用同模块中的其他函数
   auto* module = static_cast<ModuleNode*>(__mc_module_ctx);
   auto other_func = module->GetFunction("other_function_name", self_ref);
   ```

2. **访问模块级资源**：
   ```cpp
   // 访问模块级别的配置或状态
   auto* module = static_cast<ModuleNode*>(__mc_module_ctx);
   auto config_func = module->GetFunction("get_module_config", self_ref);
   ```

3. **支持闭包函数**：
   ```cpp
   // 闭包函数需要模块上下文作为资源句柄
   int closure_func__c_api(Value* args, int num_args, Value* ret_val, void* resource) {
       // resource 参数实际上就是模块上下文
       auto* module = static_cast<ModuleNode*>(resource);
       // 使用模块上下文访问模块资源
   }
   ```

**类比理解**：
- 就像一个 **类的成员函数** 需要 `this` 指针来访问其他成员函数和成员变量
- **动态库中的函数** 需要 **模块上下文指针** 来访问同模块中的其他函数和资源
- 模块上下文 = 动态库的 "this" 指针

**一个更简单的例子**：
```cpp
// 在 math_module.so 中
extern "C" void* __mc_module_ctx = nullptr;

namespace {
    int add__c_api(Value* args, int num_args, Value* ret_val, void* resource) {
        // 普通的加法函数
        ret_val->t = TypeIndex::Int;
        ret_val->u.v_int = args[0].u.v_int + args[1].u.v_int;
        return 0;
    }
    
    int add_with_log__c_api(Value* args, int num_args, Value* ret_val, void* resource) {
        // 这个函数想要调用同模块中的 "add" 函数，然后记录结果
        
        // 1. 通过模块上下文获取 "add" 函数
        auto* module = static_cast<ModuleNode*>(__mc_module_ctx);
        auto add_func = module->GetFunction("add", self_ref);
        
        // 2. 调用 "add" 函数
        McValue result = add_func(Parameters(args, 2));
        
        // 3. 记录日志（假设有日志函数）
        printf("add(%d, %d) = %d\n", 
               args[0].u.v_int, args[1].u.v_int, result.As<int>());
        
        // 4. 返回结果
        ret_val->t = TypeIndex::Int;
        ret_val->u.v_int = result.As<int>();
        return 0;
    }
}
```

**关键点**：
- 没有模块上下文，`add_with_log` 函数就无法调用同模块中的 `add` 函数
- 模块上下文让动态库内部形成了一个 **函数生态系统**，函数之间可以互相协作
- 这样设计使得动态库不仅仅是独立函数的集合，而是一个有机的 **功能模块**

## 3. 函数注册表解析机制

### 3.1 函数注册表的结构

每个动态库都包含特殊的符号来描述其函数接口：

```cpp
namespace Symbol {
    constexpr const char* ModuleCtx = "__mc_module_ctx";          // 模块上下文
    constexpr const char* FuncRegistry = "__mc_func_registry__";  // 函数注册表
    constexpr const char* ClosuresNames = "__mc_closures_names__"; // 闭包函数名
}
```

**函数注册表格式**：
```cpp
typedef struct {
    const char* names;     // 编码的函数名称列表
    BackendFunc* funcs;    // 函数指针数组
} FuncRegistry;
```

### 3.2 函数名称编码解析

**编码格式**：
```
[函数数量:1字节][函数名1\0][函数名2\0]...[函数名N\0]
```

**解析实现**：
```cpp
static std::vector<std::string_view> ReadFuncRegistryNames(const char* names) {
    std::vector<std::string_view> result;
    if (!names) return result;
    
    // 1. 读取函数数量
    uint8_t num_funcs = static_cast<uint8_t>(names[0]);
    
    // 2. 逐个解析以 null 结尾的字符串
    const char* p = names + 1;
    while (*p && result.size() < num_funcs) {
        size_t len = strlen(p);
        result.push_back(std::string_view(p, len));
        p += len + 1;  // 跳到下一个字符串
    }
    
    return result;
}
```

### 3.3 LibraryModuleNode 的函数加载

**构造时预加载**：
```cpp
explicit LibraryModuleNode(object_p<Library> p) : p_(std::move(p)) {
    LoadFunctions();  // 构造时立即加载所有函数
}

void LoadFunctions() {
    // 1. 获取函数注册表
    auto* func_reg = reinterpret_cast<FuncRegistry*>(
        p_->GetSymbol(Symbol::FuncRegistry));
    
    if (!func_reg) {
        throw std::runtime_error("Missing function registry");
    }
    
    // 2. 解析函数名称
    auto func_names = ReadFuncRegistryNames(func_reg->names);
    
    // 3. 建立函数名到函数指针的映射
    for (size_t i = 0; i < func_names.size(); ++i) {
        func_table_.emplace(func_names[i], func_reg->funcs[i]);
    }
    
    // 4. 加载闭包函数名称
    auto* closure_names = reinterpret_cast<const char**>(
        p_->GetSymbol(Symbol::ClosuresNames));
    
    if (closure_names) {
        auto names = ReadFuncRegistryNames(*closure_names);
        closure_names_.insert(names.begin(), names.end());
    }
}
```

**设计优势**：
- **高性能**：构造时一次性加载，避免重复解析
- **错误检查**：加载时验证注册表完整性
- **内存优化**：使用 `string_view` 避免字符串复制

## 4. 函数包装和调用机制

### 4.1 BackendFunc 到 Function 的转换

**BackendFunc 签名**：
```cpp
typedef int(*BackendFunc)(Value* args, int num_args, Value* rv, void* resource);
```

**Function 签名**：
```cpp
Function: McValue operator()(Parameters args)
```

**WrapFunction 实现**：
```cpp
inline Function WrapFunction(BackendFunc func,
                           const object_p<object_t>& sptr_to_self,
                           bool capture_resource = false) {
    if (capture_resource) {
        // 闭包函数：需要模块资源句柄
        return Function([func, sptr_to_self](Parameters args) -> McValue {
            if (args.size() <= 0) {
                throw std::runtime_error("Resource handle required");
            }
            
            // 最后一个参数是资源句柄
            void* handle = args[args.size() - 1].As<void*>();
            
            // 转换参数
            std::vector<Value> c_args;
            c_args.reserve(args.size() - 1);
            for (int i = 0; i < args.size() - 1; ++i) {
                c_args.push_back(args[i].value());
            }
            
            // 调用 C 函数
            Value ret_val;
            if (int ret = (*func)(c_args.data(), c_args.size(), &ret_val, handle); ret != 0) {
                throw std::runtime_error(GetError());
            }
            
            return McValue(McView(&ret_val));
        });
    } else {
        // 普通函数：不需要资源句柄
        return Function([func, sptr_to_self](Parameters args) -> McValue {
            std::vector<Value> c_args;
            c_args.reserve(args.size());
            for (int i = 0; i < args.size(); ++i) {
                c_args.push_back(args[i].value());
            }
            
            Value ret_val;
            if (int ret = (*func)(c_args.data(), c_args.size(), &ret_val, nullptr); ret != 0) {
                throw std::runtime_error(GetError());
            }
            
            return McValue(McView(&ret_val));
        });
    }
}
```

### 4.2 函数查找的优化策略

**两级查找策略**：
```cpp
Function GetFunction(const std::string_view& name,
                     const object_p<object_t>& sptr_to_self) override {
    // 1. 首先查找预加载的函数表（O(1)）
    auto it = func_table_.find(name);
    if (it != func_table_.end()) {
        bool is_closure = closure_names_.find(name) != closure_names_.end();
        return WrapFunction(it->second, sptr_to_self, is_closure);
    }
    
    // 2. 如果不在预加载表中，尝试动态查找
    auto faddr = GetBackendFunction(name);
    if (!faddr) {
        return Function();  // 返回空函数表示未找到
    }
    
    bool is_closure = closure_names_.find(name) != closure_names_.end();
    return WrapFunction(faddr, sptr_to_self, is_closure);
}
```

**设计优势**：
- **高效查找**：常用函数通过 hash 表 O(1) 查找
- **动态扩展**：支持查找未预注册的函数
- **内存优化**：只缓存实际使用的函数

## 5. Python 端的模块对象管理

### 5.1 Module 类的 Python 包装

**Python 端的 Module 类**：
```python
class Module(object):
    def __init__(self, handle):
        self.handle = handle

    def get_function(self, name):
        ret_handle = PackedFuncHandle()
        status = _LIB.GetBackendFunction(
            ModuleHandle(self.handle),
            c_str(name),
            ctypes.c_int(0),
            ctypes.byref(ret_handle)
        )
        return matx_script_api.PackedFuncBase(ret_handle.value, False)
```

**GetBackendFunction 的 C++ 实现 (src/c_api.cc:225-244)**：
```cpp
int GetBackendFunction(ModuleHandle m, 
                       const char* func_name, 
                       int use_imports,
                       ModuleHandle* out) {
    API_BEGIN();
    using mc::runtime::ModuleNode;
    using mc::runtime::object_t;
    using mc::runtime::Function;
    
    // 1. 将 ModuleHandle 转换为 ModuleNode 指针
    auto me = static_cast<ModuleNode*>(static_cast<object_t*>(m));
    
    // 2. 调用 ModuleNode 的 GetFunction 方法
    auto pn = me->GetFunction(func_name, use_imports != 0);
    
    // 3. 如果找到函数，创建新的 Function 对象并返回
    if (pn != nullptr) {
        *out = new Function(pn);  // 在堆上分配新的 Function 对象
    } else {
        *out = nullptr;  // 函数未找到
    }
    API_END();
}
```

**关键作用**：
- **类型转换桥梁**：将 C API 的 `ModuleHandle` 转换为 C++ 的 `ModuleNode*`
- **内存管理**：在堆上创建新的 `Function` 对象，返回给 Python 端
- **错误处理**：通过 `API_BEGIN/API_END` 宏进行异常安全处理

### 5.2 模块对象的 FFI 集成

**注册为 FFI 对象**：
```python
def _conv(mod: Module):
    handle = mod.handle
    return matx_script_api.make_any(1, 0, handle, 0)

# 注册 Module 类型
matx_script_api.register_object(1, Module)
matx_script_api.register_input_callback(Module, _conv)
```

**工作流程**：
1. `register_object(1, Module)` 注册类型创建器
2. `register_input_callback(Module, _conv)` 注册输入转换器
3. 当 C++ 函数返回 Module 对象时，调用 Python 创建器
4. 当 Python Module 对象传递给 C++ 时，调用转换器

### 5.3 完整的调用链路

**从 Python 到 C++ 再到 Python**：
```python
# 1. 加载模块（C++ 返回 Module 对象）
test_module = module_loader_("./test_func.so")
#   ↓ C++ ModuleLoader 返回 Module 对象
#   ↓ ValueToPyObject 发现是 Module 类型
#   ↓ 调用 register_object 注册的 Python 创建器
#   ↓ 创建 Python Module 对象并设置 handle

# 2. 获取函数（Module 对象调用 get_function）
test_func = test_module.get_function("test_func")
#   ↓ Python Module.get_function 调用 C API
#   ↓ 返回 PackedFuncBase 对象

# 3. 调用函数（PackedFuncBase 对象调用）
result = test_func(3, 4)
#   ↓ PyPackedFuncBase_call 转换参数
#   ↓ 调用动态库中的函数
#   ↓ 返回结果并转换为 Python 对象
```

## 6. 平台抽象和实现

### 6.1 DefaultLibray 实现

**Unix/Linux 平台实现**：
```cpp
class DefaultLibray final : public Library {
public:
    void Init(const std::string& name) {
        Load(name);
    }

    void* GetSymbol(std::string_view name) final {
        return GetSymbol_(name.data());
    }

private:
    void* pointer_{nullptr};

    void Load(const std::string& name) {
        pointer_ = dlopen(name.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!pointer_) {
            printf("Failed to load library: %s\n", dlerror());
        }
    }

    void UnLoad() {
        dlclose(pointer_);
        pointer_ = nullptr;
    }

    void* GetSymbol_(const char* name) {
        void* sym = dlsym(pointer_, name);
        return sym;
    }
};
```

**设计特点**：
- 使用 RAII 管理动态库生命周期
- 支持符号查找和错误处理
- 平台特定的实现细节被封装

### 6.2 SystemLibrary 实现

**系统库管理**：
```cpp
class SystemLibrary : public Library {
public:
    static const object_p<SystemLibrary>& Global() {
        static auto inst = MakeObject<SystemLibrary>();
        return inst;
    }

    void* GetSymbol(std::string_view name) final {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = map_.find(name);
        return it != map_.end() ? it->second : nullptr;
    }

    void RegisterSymbol(std::string_view name, void* p) {
        std::lock_guard<std::mutex> lock(mutex_);
        map_[name] = p;
    }

private:
    std::mutex mutex_;
    std::unordered_map<std::string_view, void*> map_;
};
```

**用途**：
- 管理系统级别的符号
- 提供线程安全的符号注册
- 支持运行时符号解析

## 7. 设计模式和核心价值

### 7.1 设计模式

1. **抽象工厂模式**：
   - `Library` 抽象基类定义接口
   - `DefaultLibray` 和 `SystemLibrary` 提供具体实现
   - 隐藏平台差异

2. **适配器模式**：
   - `WrapFunction` 将 `BackendFunc` 适配为 `Function`
   - 桥接不同的函数调用约定

3. **单例模式**：
   - `SystemLibrary::Global()` 提供全局单例
   - 确保系统符号的唯一性

4. **代理模式**：
   - `Module` 类代理 `ModuleNode` 的功能
   - 提供简化的用户接口

### 7.2 核心价值

**可扩展性**：
- 支持运行时加载新的功能模块
- 不需要重新编译核心系统
- 支持插件式架构

**性能优化**：
- 预加载函数表，避免重复解析
- O(1) 时间复杂度的函数查找
- 延迟加载未使用的函数

**类型安全**：
- 通过 FFI 系统的类型转换保证安全
- 运行时类型检查
- 异常安全的资源管理

**平台无关**：
- 抽象接口隐藏平台差异
- 支持不同操作系统的动态库机制
- 统一的符号查找接口

## 8. 动态库的结构和实现 (test_func.cc)

为了完整理解动态库加载系统，我们需要看看被加载的动态库是如何实现的。`test_func.cc` 是一个完整的动态库示例，展示了 MC 项目动态库的标准结构。

### 8.1 模块上下文机制 (__mc_module_ctx)

在深入动态库结构之前，我们先理解一个关键概念：**模块上下文**。

**为什么需要模块上下文？**

这是一个关键的设计，让我们通过具体例子来理解：

**问题场景**：假设动态库中有一个函数需要调用同一个模块中的另一个函数，或者需要访问模块级别的资源。

**一个简单的例子**：
```cpp
// 在 math_module.so 中
extern "C" void* __mc_module_ctx = nullptr;

namespace {
    int add__c_api(Value* args, int num_args, Value* ret_val, void* resource) {
        // 普通的加法函数
        ret_val->t = TypeIndex::Int;
        ret_val->u.v_int = args[0].u.v_int + args[1].u.v_int;
        return 0;
    }
    
    int add_with_log__c_api(Value* args, int num_args, Value* ret_val, void* resource) {
        // 这个函数想要调用同模块中的 "add" 函数，然后记录结果
        
        // 1. 通过模块上下文获取 "add" 函数
        auto* module = static_cast<ModuleNode*>(__mc_module_ctx);
        auto add_func = module->GetFunction("add", self_ref);
        
        // 2. 调用 "add" 函数
        McValue result = add_func(Parameters(args, 2));
        
        // 3. 记录日志
        printf("add(%d, %d) = %d\n", 
               args[0].u.v_int, args[1].u.v_int, result.As<int>());
        
        // 4. 返回结果
        ret_val->t = TypeIndex::Int;
        ret_val->u.v_int = result.As<int>();
        return 0;
    }
}
```

**没有模块上下文会发生什么？**
```cpp
// 错误的做法 - 没有模块上下文
int add_with_log__c_api(Value* args, int num_args, Value* ret_val, void* resource) {
    // ❌ 问题：如何获取同模块中的 "add" 函数？
    // ❌ 没有模块引用，无法调用同模块中的其他函数
    // ❌ 只能硬编码逻辑，失去了模块化的优势
    
    int result = args[0].u.v_int + args[1].u.v_int;  // 只能重复实现逻辑
    printf("add(%d, %d) = %d\n", args[0].u.v_int, args[1].u.v_int, result);
    
    ret_val->t = TypeIndex::Int;
    ret_val->u.v_int = result;
    return 0;
}
```

**模块上下文的设置过程**：
```cpp
Module CreateModuleFromLibrary(object_p<Library> p) {
    // 1. 创建模块节点
    auto n = MakeObject<LibraryModuleNode>(p);
    Module root = Module(n);
    
    // 2. 关键：设置模块上下文
    if (auto* ctx_addr = reinterpret_cast<void**>(
          p->GetSymbol(Symbol::ModuleCtx))) {  // 查找 "__mc_module_ctx"
        *ctx_addr = root.operator->();  // 将模块对象地址赋值给动态库中的全局变量
    }
    
    return root;
}
```

**类比理解**：
- 就像一个 **类的成员函数** 需要 `this` 指针来访问其他成员函数和成员变量
- **动态库中的函数** 需要 **模块上下文指针** 来访问同模块中的其他函数和资源
- 模块上下文 = 动态库的 "this" 指针

**关键价值**：
- **模块化**：动态库不是独立函数的集合，而是有机的功能模块
- **协作性**：函数之间可以互相协作，形成函数生态系统
- **扩展性**：支持复杂的模块内部逻辑和资源管理

### 8.2 动态库的核心结构

**完整的 test_func.cc 文件结构**：

```cpp
#include "runtime_value.h"
#include "parameters.h"
#include "registry.h"
#include "c_api.h"

using namespace mc::runtime;

// 1. 模块上下文指针（必须）
extern "C" void* __mc_module_ctx = nullptr;

namespace {
    // 2. 业务逻辑函数（C++ 实现）
    int32_t test_func(int32_t a, int32_t b) {
        int32_t c = (a + b);
        int32_t ssa = (c * b);
        c = ssa;
        return c;
    }

    // 3. C API 包装函数（必须）
    int test_func__c_api(Value* args, int num_args, Value* ret_val, void* resource_handle) {
        // 参数数量检查
        if (num_args != 2) {
            char error_msg[100];
            snprintf(error_msg, sizeof(error_msg), 
                    "test_func() takes 2 positional arguments but %d were given", 
                    num_args);
            SetError(error_msg);
            return -1;
        }

        // 参数类型检查
        if (args[0].t != TypeIndex::Int) {
            SetError("test_func argument 0 type mismatch, expect 'int' type");
            return -1;
        }
        if (args[1].t != TypeIndex::Int) {
            SetError("test_func argument 1 type mismatch, expect 'int' type");
            return -1;
        }

        // 调用实际的业务函数
        auto result = test_func(args[0].u.v_int, args[1].u.v_int);
        
        // 设置返回值
        ret_val->t = TypeIndex::Int;
        ret_val->u.v_int = result;
        return 0;
    }
}

extern "C" {
    // 4. 函数指针数组（必须）
    BackendFunc __mc_func_array__[] = {
        (BackendFunc)test_func__c_api,
    };

    // 5. 函数注册表（必须）
    FuncRegistry __mc_func_registry__ = {
        "\1test_func",        // 编码的函数名列表
        __mc_func_array__,    // 函数指针数组
    };

    // 6. 闭包函数名称（可选）
    const char* __mc_closures_names__ = "0\000";  // 没有闭包

    // 7. 模块初始化函数（可选）
    __attribute__((constructor))
    void init_module() {
        printf("Module loaded, registry at: %p\n", &__mc_func_registry__);
    }
}
```

### 8.3 关键组件详解

**1. 模块上下文指针**：
```cpp
extern "C" void* __mc_module_ctx = nullptr;
```
- **用途**：存储模块对象的引用，在 `CreateModuleFromLibrary` 中被设置
- **必须性**：必须存在，用于模块自身的引用管理
- **详细解释**：参见上面第8.1节的详细说明

**2. C API 包装函数**：
```cpp
int test_func__c_api(Value* args, int num_args, Value* ret_val, void* resource_handle)
```
- **签名**：必须符合 `BackendFunc` 类型定义
- **职责**：参数验证、类型转换、错误处理、调用业务逻辑
- **返回值**：0 表示成功，非 0 表示失败

**3. 函数注册表**：
```cpp
FuncRegistry __mc_func_registry__ = {
    "\1test_func",        // 函数名编码
    __mc_func_array__,    // 函数指针数组
};
```
- **名称编码**：`\1test_func` 表示 1 个函数，名称为 "test_func"
- **函数数组**：与名称一一对应的函数指针数组

**4. 闭包函数标记**：
```cpp
const char* __mc_closures_names__ = "0\000";
```
- **格式**：与函数名编码相同，"0" 表示没有闭包函数

### 8.4 编译和使用流程

**编译为动态库**：
```bash
g++ -shared -fPIC test_func.cc -I./src -L./build -lcase -o test_func.so
```

**Python 端加载和使用**：
```python
# 1. 加载动态库
module_loader_ = matx_script_api.GetGlobal("runtime.ModuleLoader", True)
test_module = module_loader_("./test_func.so")

# 2. 获取函数
test_func = test_module.get_function("test_func")

# 3. 调用函数
result = test_func(3, 4)  # 调用 test_func(3, 4)
print(f"Result: {result}")  # 输出：Result: 28  ((3+4)*4)
```

### 8.5 函数调用的完整链路

**分为两个阶段：函数获取阶段和函数调用阶段**

#### 阶段1：函数获取流程
```
1. Python: test_func = test_module.get_function("test_func")
   ↓
2. new_ffi.py: Module.get_function() 调用 ctypes
   ↓
3. ctypes: _LIB.GetBackendFunction(ModuleHandle, "test_func", 0, &ret_handle)
   ↓
4. c_api.cc: GetBackendFunction()
   ↓
5. 类型转换: ModuleHandle → ModuleNode* → LibraryModuleNode*
   ↓
6. runtime_module.cc: LibraryModuleNode::GetFunction()
   ↓
7. 函数查找: 在 func_table_ 中查找 "test_func"
   ↓
8. 函数包装: WrapFunction(test_func__c_api, sptr_to_self, false)
   ↓
9. 返回: Function 对象 → new Function(pn) → FunctionHandle
   ↓
10. Python: matx_script_api.PackedFuncBase(ret_handle.value, False)
```

#### 阶段2：函数调用流程
```
1. Python: test_func(3, 4)
   ↓
2. case_ext.cc: PyPackedFuncBase_call()
   ↓
3. 参数转换: Python int → Value{t=TypeIndex::Int, u.v_int=3}
   ↓
4. c_api.cc: FuncCall_PYTHON_C_API()
   ↓
5. runtime_module.cc: WrapFunction() 包装的 lambda
   ↓
6. test_func.so: test_func__c_api(Value* args, int num_args, Value* ret_val, void* resource_handle)
   ↓
7. 参数验证: 检查 num_args == 2, args[0].t == TypeIndex::Int, args[1].t == TypeIndex::Int
   ↓
8. 参数提取: int32_t a = args[0].u.v_int, int32_t b = args[1].u.v_int
   ↓
9. 业务逻辑: test_func(a, b) → int32_t result = (a + b) * b
   ↓
10. 返回值设置: ret_val->t = TypeIndex::Int, ret_val->u.v_int = result
    ↓
11. 逆向传播: Value → McValue → Python int
    ↓
12. Python: 接收到结果 28
```

**关键观察**：
- **函数获取是一次性的**：第一次调用 `get_function` 时会进行完整的查找和包装
- **函数调用是重复的**：之后每次调用 `test_func(3, 4)` 都直接执行已包装的函数
- **GetBackendFunction 的作用**：它是 Python 端获取 C++ 函数的关键桥梁

#### 完整的系统交互图
```
┌─────────────────────────────────────────────────────────────────┐
│                          Python 层                             │
│                                                                 │
│  test_module.get_function("test_func")                         │
│              ↓                                                 │
│  Module.get_function() [new_ffi.py]                           │
│              ↓                                                 │
│  _LIB.GetBackendFunction() [ctypes调用]                       │
└─────────────────────────┬───────────────────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────────────────┐
│                       C API 层                                 │
│                                                                 │
│  GetBackendFunction() [c_api.cc]                               │
│    ├─ ModuleHandle → ModuleNode* 类型转换                      │
│    ├─ me->GetFunction(func_name, use_imports)                  │
│    └─ new Function(pn) → FunctionHandle                       │
└─────────────────────────┬───────────────────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────────────────┐
│                   C++ 运行时层                                 │
│                                                                 │
│  LibraryModuleNode::GetFunction() [runtime_module.cc]         │
│    ├─ func_table_.find(name) 查找预加载函数                   │
│    ├─ WrapFunction(backend_func, sptr_to_self, is_closure)     │
│    └─ 返回 Function 对象                                       │
└─────────────────────────┬───────────────────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────────────────┐
│                      动态库层                                   │
│                                                                 │
│  test_func.so                                                  │
│    ├─ __mc_func_registry__ 函数注册表                         │
│    ├─ test_func__c_api BackendFunc 实现                       │
│    └─ test_func(int32_t, int32_t) 业务逻辑                    │
└─────────────────────────────────────────────────────────────────┘
```

这个图表清楚地展示了 `GetBackendFunction` 如何连接 Python 层和 C++ 层，实现动态库函数的获取和包装。

### 8.6 动态库的设计模式

**模板方法模式**：
- **抽象流程**：参数验证 → 类型转换 → 业务逻辑 → 结果设置
- **具体实现**：每个函数实现自己的参数验证和业务逻辑

**适配器模式**：
- **源接口**：C++ 业务函数 `int32_t test_func(int32_t a, int32_t b)`
- **目标接口**：`BackendFunc` 签名
- **适配器**：`test_func__c_api` 函数

**工厂模式**：
- **产品**：Function 对象
- **工厂**：函数注册表和 `WrapFunction`
- **配置**：编码的函数名和指针数组

### 8.7 错误处理和类型安全

**多层错误检查**：
```cpp
// 1. 参数数量检查
if (num_args != 2) {
    SetError("test_func() takes 2 positional arguments but %d were given");
    return -1;
}

// 2. 参数类型检查
if (args[0].t != TypeIndex::Int) {
    SetError("test_func argument 0 type mismatch, expect 'int' type");
    return -1;
}

// 3. 业务逻辑执行
auto result = test_func(args[0].u.v_int, args[1].u.v_int);

// 4. 返回值设置
ret_val->t = TypeIndex::Int;
ret_val->u.v_int = result;
return 0;  // 成功
```

**类型安全保证**：
- **编译时**：C++ 类型系统确保业务逻辑的类型正确
- **运行时**：C API 包装函数进行动态类型检查
- **接口层**：Value 结构体提供统一的类型标识

### 8.8 性能考虑

**零拷贝设计**：
- 参数通过 `Value` 结构体直接传递，避免序列化
- 基本类型直接存储在联合体中
- 对象类型通过指针传递

**缓存优化**：
- 函数注册表在模块加载时一次性解析
- 函数指针直接存储，调用时无需查找
- 字符串使用 `string_view` 避免复制

## 9. 总结

动态库加载系统是 MC 项目的关键基础设施，它成功解决了以下核心问题：

1. **运行时扩展能力**：让编译器能够在运行时加载新的功能模块
2. **跨语言互操作**：通过 FFI 系统实现 Python 和 C++ 的无缝集成
3. **性能优化**：通过缓存和预加载机制提供高性能的函数调用
4. **平台抽象**：隐藏操作系统差异，提供统一的接口

这个系统的设计展示了优秀的软件架构原则：
- **分层抽象**：每一层都有明确的职责
- **接口设计**：简洁而强大的抽象接口
- **性能考虑**：在保证功能的同时优化性能
- **可维护性**：清晰的模块划分和错误处理

这是一个值得学习的工程实践案例，展示了如何设计一个既强大又灵活的动态模块系统。