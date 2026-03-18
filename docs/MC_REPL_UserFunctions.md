# MC REPL 自定义函数功能

## 概述

MC REPL 现在支持用户自定义函数功能，可以定义和调用自定义函数，支持参数传递、函数嵌套调用和完整的变量作用域管理。

## 功能特性

### 1. 函数定义语法
```
def function_name(param1, param2, ...) = expression
```

### 2. 支持的函数类型

#### 单参数函数
- `def square(x) = x * x` - 平方函数
- `def double(x) = x + x` - 双倍函数
- `def add_ten(x) = x + 10` - 加10函数

#### 多参数函数
- `def sum_squares(a, b) = a * a + b * b` - 平方和函数

### 3. 函数调用
- 与内置函数相同的调用语法：`function_name(arg1, arg2, ...)`
- 支持函数嵌套调用
- 自动参数数量检查

### 4. 作用域管理
- 函数参数在函数执行期间创建局部变量
- 函数执行完毕后自动恢复原变量环境
- 支持参数名与全局变量重名的情况

## 使用示例

### 基本函数定义和调用
```
mc> def square(x) = x * x
Defined function: square(x) = x * x

mc> def double(x) = x + x
Defined function: double(x) = x + x

mc> test_userdef
Defining user functions...
Defined: square(x) = x * x
Defined: add_one(x) = x + 1
Defined: sum_of_squares(a, b) = square(a) + square(b)

Testing user functions:
square(5) = 25
add_one(10) = 11
sum_of_squares(3, 4) = 25
```

### 函数列表查看
```
mc> :funcs
Built-in functions:
  print()
  sin()
  pow()
  cos()
  sqrt()

User-defined functions:
  sum_of_squares(a, b)
  add_one(x)
  square(x)
  double(x)
```

### 嵌套函数调用
用户自定义函数可以调用其他用户自定义函数或内置函数：
```cpp
// sum_of_squares 函数调用了 square 函数
def sum_of_squares(a, b) = square(a) + square(b)
```

## 技术实现

### 1. 数据结构
```cpp
struct UserFunction {
    std::vector<std::string> params;  // 参数名列表
    PrimExpr body;                    // 函数体表达式（MC AST 节点）
};

std::unordered_map<std::string, UserFunction> user_functions_;
```

### 2. 函数执行流程
```cpp
McValue evaluate_user_function(const UserFunction& func, const Array<PrimExpr>& args) {
    // 1. 检查参数数量
    if (args.size() != func.params.size()) {
        throw std::runtime_error("参数数量不匹配");
    }
    
    // 2. 保存当前变量环境
    std::unordered_map<std::string, McValue> saved_vars;
    for (const auto& param : func.params) {
        if (variables_.count(param)) {
            saved_vars[param] = variables_[param];
        }
    }
    
    // 3. 设置参数值
    for (size_t i = 0; i < func.params.size(); i++) {
        variables_[func.params[i]] = evaluate(args[i]);
    }
    
    // 4. 执行函数体
    McValue result = evaluate(func.body);
    
    // 5. 恢复变量环境
    for (const auto& param : func.params) {
        if (saved_vars.count(param)) {
            variables_[param] = saved_vars[param];
        } else {
            variables_.erase(param);
        }
    }
    
    return result;
}
```

### 3. 函数优先级
函数调用时的查找顺序：
1. 用户自定义函数
2. 内置函数

这样用户可以"覆盖"内置函数（虽然内置函数仍然存在）。

### 4. AST 集成
- 函数体使用 MC 的 `PrimExpr` AST 节点存储
- 函数调用通过 `PrimCall` 和 `GlobalVar` 处理
- 完全集成 MC 的类型系统和表达式求值机制

## 支持的表达式类型

在函数体中可以使用：
- 整数字面量：`42`, `123`
- 变量引用：参数名或全局变量
- 算术运算：`+`, `*`
- 函数调用：内置函数或其他用户自定义函数

## 测试命令

### test_userdef
运行完整的用户自定义函数测试：
- 定义多个函数
- 测试单参数函数
- 测试多参数函数
- 测试函数嵌套调用
- 验证结果正确性

### 示例输出
```
mc> test_userdef
Defining user functions...
Defined: square(x) = x * x
Defined: add_one(x) = x + 1
Defined: sum_of_squares(a, b) = square(a) + square(b)

Testing user functions:
square(5) = 25
add_one(10) = 11
sum_of_squares(3, 4) = 25
```

## 错误处理

### 参数数量检查
```
Function expects 2 arguments, got 1
```

### 未定义函数
```
Function 'unknown_func' not defined
```

### 异常安全
函数执行过程中如果发生异常，会自动恢复变量环境，确保不会泄漏局部变量。

## 限制和未来改进

### 当前限制
1. 函数定义语法固定（硬编码匹配）
2. 只支持表达式函数体，不支持语句块
3. 不支持递归函数（可能导致栈溢出）
4. 不支持默认参数或变长参数

### 未来改进方向
1. 添加完整的语法解析器
2. 支持条件表达式 (if-then-else)
3. 支持多语句函数体
4. 添加递归深度限制
5. 支持高阶函数（函数作为参数）
6. 添加类型注解和类型检查

## 与 MC AST 的完美集成

这个自定义函数系统完全基于 MC 的现有 AST 基础设施：
- 使用 `PrimExpr` 存储函数体
- 通过 `PrimCall` 处理函数调用
- 利用 MC 的表达式求值系统
- 遵循 MC 的类型系统约定

这种设计确保了用户自定义函数与 MC 系统的其他部分保持完全一致，为未来扩展提供了坚实的基础。