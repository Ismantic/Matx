# MC REPL 语言设计文档

## 概述

基于现有的 MC Runtime 系统开发一个支持 REPL (Read-Eval-Print Loop) 的动态编程语言。该语言将具备现代脚本语言的特性，同时享受编译到 C++ 的性能优势。

## 1. 语言特性

### 1.1 基本语法
```mc
# 变量定义
let x = 42
let name = "Hello, MC!"
let data = [1, 2, 3, 4, 5]

# 函数定义
fn add(a: int, b: int) -> int {
    return a + b
}

# 类定义
class Point {
    x: int
    y: int
    
    fn init(x: int, y: int) {
        self.x = x
        self.y = y
    }
    
    fn distance() -> float {
        return sqrt(self.x * self.x + self.y * self.y)
    }
}
```

### 1.2 REPL 特性
```mc
mc> let x = 10
mc> let y = 20
mc> x + y
30
mc> fn factorial(n: int) -> int { return n <= 1 ? 1 : n * factorial(n-1) }
mc> factorial(5)
120
mc> let p = Point(3, 4)
mc> p.distance()
5.0
```

## 2. 架构设计

### 2.1 REPL 执行流程
```
用户输入 → 词法分析 → 语法解析 → AST → IR → 即时执行/编译 → 结果显示
```

### 2.2 核心组件

#### 2.2.1 REPL 引擎
```cpp
class REPLEngine {
    RuntimeContext context_;
    SymbolTable global_symbols_;
    FunctionCache compiled_functions_;
    
public:
    Value evaluate(const std::string& input);
    void define_variable(const std::string& name, const Value& value);
    void define_function(const std::string& name, const Function& func);
    std::string format_result(const Value& result);
};
```

#### 2.2.2 增量编译器
```cpp
class IncrementalCompiler {
    ModuleBuilder current_module_;
    
public:
    // 即时编译表达式
    CompiledExpr compile_expression(const Expr& expr);
    
    // 编译函数定义
    CompiledFunc compile_function(const FunctionDef& func_def);
    
    // 编译类定义
    CompiledClass compile_class(const ClassDef& class_def);
};
```

#### 2.2.3 运行时上下文
```cpp
class RuntimeContext {
    std::unordered_map<std::string, Value> variables_;
    std::unordered_map<std::string, PackedFunc> functions_;
    std::unordered_map<std::string, ClassInfo> classes_;
    
public:
    Value get_variable(const std::string& name);
    void set_variable(const std::string& name, const Value& value);
    PackedFunc get_function(const std::string& name);
    void register_function(const std::string& name, const PackedFunc& func);
};
```

## 3. 实现策略

### 3.1 两层执行策略

#### 解释执行层（快速响应）
- 简单表达式直接在 Runtime 中解释执行
- 适用于交互式计算、调试
- 基于现有的 `McValue` 系统

#### 编译执行层（高性能）
- 复杂函数、类编译为 C++ 代码
- 动态加载为共享库
- 适用于性能关键代码

### 3.2 智能执行选择
```cpp
enum class ExecutionStrategy {
    INTERPRET,  // 解释执行
    JIT_COMPILE, // 即时编译
    AOT_COMPILE  // 预编译
};

ExecutionStrategy choose_strategy(const ASTNode& node) {
    if (is_simple_expression(node)) {
        return INTERPRET;
    } else if (is_hot_function(node)) {
        return JIT_COMPILE;
    } else {
        return AOT_COMPILE;
    }
}
```

## 4. 核心功能实现

### 4.1 变量管理
```cpp
class VariableManager {
    std::stack<std::unordered_map<std::string, Value>> scope_stack_;
    
public:
    void push_scope();
    void pop_scope();
    void define(const std::string& name, const Value& value);
    Value lookup(const std::string& name);
};
```

### 4.2 表达式解释器
```cpp
class ExpressionInterpreter {
    RuntimeContext& context_;
    
public:
    Value evaluate(const Expr& expr) {
        switch (expr.type()) {
            case ExprType::Literal:
                return evaluate_literal(expr);
            case ExprType::Variable:
                return context_.get_variable(expr.name());
            case ExprType::BinaryOp:
                return evaluate_binary_op(expr);
            case ExprType::FunctionCall:
                return evaluate_function_call(expr);
            // ...
        }
    }
};
```

### 4.3 即时编译缓存
```cpp
class JITCache {
    std::unordered_map<std::string, CompiledFunction> function_cache_;
    std::unordered_map<std::string, void*> dll_handles_;
    
public:
    CompiledFunction* get_compiled_function(const std::string& signature);
    void cache_function(const std::string& signature, CompiledFunction func);
    void invalidate_cache();
};
```

## 5. REPL 界面设计

### 5.1 命令系统
```mc
# 内置命令
mc> :help              # 显示帮助
mc> :type x             # 显示变量类型
mc> :info factorial     # 显示函数信息
mc> :vars               # 列出所有变量
mc> :funcs              # 列出所有函数
mc> :clear              # 清除环境
mc> :load "script.mc"   # 加载脚本文件
mc> :save "session.mc"  # 保存会话
mc> :compile factorial  # 强制编译函数
mc> :profile on         # 开启性能分析
mc> :exit               # 退出
```

### 5.2 智能补全
```cpp
class AutoCompleter {
public:
    std::vector<std::string> complete_variable(const std::string& prefix);
    std::vector<std::string> complete_function(const std::string& prefix);
    std::vector<std::string> complete_method(const std::string& object_type, const std::string& prefix);
};
```

### 5.3 错误处理和调试
```cpp
class ErrorHandler {
public:
    void report_syntax_error(const SyntaxError& error);
    void report_runtime_error(const RuntimeError& error);
    void show_stack_trace();
    void suggest_fixes(const Error& error);
};
```

## 6. 高级特性

### 6.1 模块系统
```mc
# 导入模块
import math
import "my_library.mc" as lib

# 使用模块
let result = math.sin(3.14159)
let data = lib.process_data([1, 2, 3])
```

### 6.2 元编程
```mc
# 宏定义
macro unless(condition, body) {
    if (!condition) {
        body
    }
}

# 使用宏
unless(x > 0) {
    print("x is not positive")
}
```

### 6.3 性能分析
```mc
mc> :profile on
mc> factorial(1000)
...
mc> :profile report
Function        Calls    Time     Avg Time
factorial       1000     0.5ms    0.0005ms
```

## 7. 实现路线图

### Phase 1: 基础 REPL
- [ ] 基本表达式解释器
- [ ] 变量管理系统
- [ ] 简单的命令行界面
- [ ] 基本错误处理

### Phase 2: 函数和类支持
- [ ] 函数定义和调用
- [ ] 类定义和实例化
- [ ] 作用域管理
- [ ] 增强的错误报告

### Phase 3: 即时编译
- [ ] 热点函数检测
- [ ] 即时编译到 C++
- [ ] 编译缓存管理
- [ ] 性能优化

### Phase 4: 高级特性
- [ ] 模块系统
- [ ] 智能补全
- [ ] 调试器集成
- [ ] 性能分析工具

## 8. 技术优势

### 8.1 性能优势
- **零拷贝数据交换**：基于 `McValue` 的统一类型系统
- **即时编译**：热点代码自动编译为原生机器码
- **内存效率**：引用计数和智能指针管理

### 8.2 开发体验
- **快速原型**：解释执行支持快速迭代
- **渐进优化**：从解释执行无缝切换到编译执行
- **丰富诊断**：详细的错误信息和性能分析

### 8.3 生态兼容
- **C++ 互操作**：直接调用 C++ 函数和类
- **Python 兼容**：复用 Python 语法的子集
- **库生态**：可以编译为共享库供其他语言使用

## 9. 示例会话

```mc
MC REPL v1.0 - Interactive Programming Environment
Type :help for help, :exit to quit

mc> let numbers = [1, 2, 3, 4, 5]
mc> let sum = 0
mc> for n in numbers { sum = sum + n }
mc> sum
15

mc> fn fibonacci(n: int) -> int {
...     if n <= 1 { return n }
...     return fibonacci(n-1) + fibonacci(n-2)
... }
mc> fibonacci(10)
55

mc> :compile fibonacci
Compiling fibonacci... done (2.3ms)
mc> fibonacci(30)  # 现在运行更快
832040

mc> class Calculator {
...     value: float
...     fn init(initial: float) { self.value = initial }
...     fn add(x: float) { self.value = self.value + x }
...     fn get() -> float { return self.value }
... }
mc> let calc = Calculator(0.0)
mc> calc.add(10.5)
mc> calc.add(20.3)
mc> calc.get()
30.8

mc> :save "my_session.mc"
Session saved to my_session.mc
mc> :exit
Goodbye!
```

这个设计充分利用了现有的 Runtime 基础设施，同时提供了现代 REPL 语言的所有特性。最重要的是，它可以在解释执行和编译执行之间无缝切换，既保证了交互性又不损失性能。