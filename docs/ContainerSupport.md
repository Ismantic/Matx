# Container Compilation Support

本文档描述了mc项目中容器编译功能的实现。

## 概述

容器编译支持允许将Python风格的容器操作（List、Dict、Set）编译为高效的C++代码。此功能借鉴了matxscript的设计，但采用了更简化的实现方式。

## 支持的容器类型

### 1. List（列表）
- **语法**: `[1, 2, 3]`
- **C++生成**: `mc::runtime::List{McValue(1), McValue(2), McValue(3)}`
- **支持操作**: append, insert, remove, 索引访问

### 2. Dict（字典）
- **语法**: `{'a': 1, 'b': 2}`
- **C++生成**: `mc::runtime::Dict{{McValue("a"), McValue(1)}, {McValue("b"), McValue(2)}}`
- **支持操作**: get, set, contains, 键值访问

### 3. Set（集合）
- **语法**: `{1, 2, 3}`
- **C++生成**: `mc::runtime::Set{McValue(1), McValue(2), McValue(3)}`
- **支持操作**: add, discard, contains

## AST节点实现

### 容器字面量节点

#### ListLiteralNode
```cpp
class ListLiteralNode : public PrimExprNode {
public:
    Array<PrimExpr> elements;
    
    void VisitAttrs(AttrVisitor* v) {
        v->Visit("elements", &elements);
    }
};
```

#### DictLiteralNode
```cpp
class DictLiteralNode : public PrimExprNode {
public:
    Array<PrimExpr> keys;
    Array<PrimExpr> values;
    
    void VisitAttrs(AttrVisitor* v) {
        v->Visit("keys", &keys);
        v->Visit("values", &values);
    }
};
```

#### SetLiteralNode
```cpp
class SetLiteralNode : public PrimExprNode {
public:
    Array<PrimExpr> elements;
    
    void VisitAttrs(AttrVisitor* v) {
        v->Visit("elements", &elements);
    }
};
```

### 容器操作节点

#### ContainerGetItemNode
用于容器元素访问：`container[index]`

```cpp
class ContainerGetItemNode : public PrimExprNode {
public:
    BaseExpr object;
    PrimExpr index;
    
    void VisitAttrs(AttrVisitor* v) {
        v->Visit("object", &object);
        v->Visit("index", &index);
    }
};
```

#### ContainerSetItemNode
用于容器元素赋值：`container[index] = value`

```cpp
class ContainerSetItemNode : public PrimExprNode {
public:
    BaseExpr object;
    PrimExpr index;
    PrimExpr value;
    
    void VisitAttrs(AttrVisitor* v) {
        v->Visit("object", &object);
        v->Visit("index", &index);
        v->Visit("value", &value);
    }
};
```

#### ContainerMethodCallNode
用于容器方法调用：`container.method(args)`

```cpp
class ContainerMethodCallNode : public PrimExprNode {
public:
    BaseExpr object;
    StrImm method;
    Array<PrimExpr> args;
    
    void VisitAttrs(AttrVisitor* v) {
        v->Visit("object", &object);
        v->Visit("method", &method);
        v->Visit("args", &args);
    }
};
```

## 代码生成

### C++代码生成示例

原始Python代码：
```python
def container_demo():
    my_list = [1, 2, 3]
    my_list.append(4)
    my_list[0] = 10
    first = my_list[0]
    
    my_dict = {'a': 1, 'b': 2}
    my_dict['c'] = 3
    value = my_dict.get('a', 0)
    
    my_set = {1, 2, 3}
    my_set.add(4)
    
    return first
```

生成的C++代码：
```cpp
McValue container_demo() {
    auto my_list = mc::runtime::List{McValue(1), McValue(2), McValue(3)};
    my_list.append(McValue(4));
    my_list[0] = McValue(10);
    auto first = my_list[0];
    
    auto my_dict = mc::runtime::Dict{{McValue("a"), McValue(1)}, {McValue("b"), McValue(2)}};
    my_dict["c"] = McValue(3);
    auto value = my_dict.contains(McValue("a")) ? my_dict[McValue("a")] : McValue(0);
    
    auto my_set = mc::runtime::Set{McValue(1), McValue(2), McValue(3)};
    my_set.insert(McValue(4));
    
    return first;
}
```

### 方法映射

| Python方法 | C++方法 | 说明 |
|------------|---------|------|
| `list.append(x)` | `list.append(McValue(x))` | 添加元素到列表末尾 |
| `list.insert(i, x)` | `list.insert(i, McValue(x))` | 在指定位置插入元素 |
| `list.remove(x)` | `list.erase(McValue(x))` | 删除第一个匹配的元素 |
| `dict.get(key, default)` | `dict.contains(key) ? dict[key] : default` | 获取字典值 |
| `set.add(x)` | `set.insert(McValue(x))` | 添加元素到集合 |
| `set.discard(x)` | `set.erase(McValue(x))` | 删除集合中的元素 |

## 编译流程

### 1. AST构建
```cpp
// 创建ListLiteral节点
Array<PrimExpr> elements = {IntImm(1), IntImm(2), IntImm(3)};
ListLiteral list_node = ListLiteral(elements);
```

### 2. 代码生成
```cpp
void Rewriter::VisitExpr_(const ListLiteralNode* op, std::ostream& os) {
    os << "mc::runtime::List{";
    for (size_t i = 0; i < op->elements.size(); ++i) {
        if (i > 0) os << ", ";
        os << "McValue(";
        PrintExpr(op->elements[i], os);
        os << ")";
    }
    os << "}";
}
```

### 3. 编译和执行
```bash
# 生成C++代码
auto cpp_code = rewriter.Done();

# 编译为共享库
g++ -shared -fPIC -I src cpp_code.cpp -o container_demo.so

# 加载和执行
ModuleLoader loader;
auto module = loader.LoadModule("./container_demo.so");
auto func = module.GetFunction("container_demo");
auto result = func();
```

## 测试验证

### C++端测试
在 `apps/test.cc` 中实现了完整的容器编译测试：
```cpp
void test_container_compilation() {
    // 1. 创建容器AST节点
    // 2. 构建函数
    // 3. 生成C++代码
    // 4. 编译为.so文件
    // 5. 加载并执行
    // 6. 验证结果
}
```

### Python端测试
在 `new_ffi.py` 中添加了 `test_container()` 函数：
```python
def test_container():
    """测试容器编译功能"""
    # 验证所有容器AST节点都已注册
    # 展示支持的功能特性
    # 确认编译流程正常
```

## 文件清单

### 核心实现文件
- `src/expression.h/cc` - 容器AST节点定义
- `src/visitor.h` - 访问者模式支持
- `src/printer.h/cc` - 容器AST打印
- `src/rewriter.h/cc` - 容器代码生成
- `src/container.h/cc` - 容器运行时实现

### 测试文件
- `apps/test.cc` - C++端完整测试
- `new_ffi.py` - Python端FFI测试
- `test_container_*.py` - 各种测试脚本

## 技术特点

### 1. 简化设计
- 直接使用mc现有的容器类，无需复杂的IR系统
- 使用auto声明简化变量类型处理
- 统一的McValue封装处理不同数据类型

### 2. 高效实现
- 直接生成C++容器操作代码
- 避免复杂的类型推断和优化
- 利用现有的FFI架构

### 3. 完整集成
- 与现有的类编译功能兼容
- 支持ModuleLoader动态加载
- 完整的错误处理和类型检查

## 未来扩展

### 1. 更多容器操作
- 容器切片操作
- 容器迭代器支持
- 容器推导式

### 2. 性能优化
- 容器类型特化
- 内存分配优化
- 操作融合

### 3. 语法糖
- 运算符重载
- 链式调用
- 函数式操作

## 结论

容器编译功能为mc项目提供了Python风格的容器操作能力，同时保持了C++的高性能特性。通过简化的设计和完整的测试，该功能已经可以投入实际使用。