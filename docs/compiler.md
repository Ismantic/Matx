# Compiler 编译器

## 概述

MC 编译器是一个从 Python 源代码到 C++ 运行时的编译系统。它通过 AST（抽象语法树）解析、IR（中间表示）生成和代码生成等步骤，将 Python 函数转换为可执行的机器代码。

**编译流程**：
```
┌─────────────────────────────────────────────────────────────────┐
│                     Python 源代码                               │
│  def test_add(a: int, b: int) -> int:                           │
│      c = a + b                                                  │
│      return c                                                   │
└─────────────────────────┬───────────────────────────────────────┘
                          │ inspect.getsource()
┌─────────────────────────▼───────────────────────────────────────┐
│                    Python AST                                  │
│  Module(body=[FunctionDef(name='test_add', args=...)])         │
└─────────────────────────┬───────────────────────────────────────┘
                          │ SimpleParser.visit()
┌─────────────────────────▼───────────────────────────────────────┐
│                   MC IR (PrimFunc)                             │
│  PrimFunc(params=[PrimVar('a'), PrimVar('b')],                 │
│           body=SeqStmt([AllocaVarStmt(), ReturnStmt()]))       │
└─────────────────────────┬───────────────────────────────────────┘
                          │ BuildFunction()
┌─────────────────────────▼───────────────────────────────────────┐
│                     C++ 源代码                                 │
│  int32_t test_add(int32_t a, int32_t b) {                      │
│      int32_t c = (a + b);                                      │
│      return c;                                                 │
│  }                                                             │
└─────────────────────────────────────────────────────────────────┘
```

## AST 类型系统

### 类型层次结构

MC 的 AST 类型系统建立在 Python 的类继承体系之上，通过 `@register_object` 装饰器与 C++ 运行时进行绑定：

```python
class Node(Object):
    """AST 节点基类"""
    def astext(self):
        fn = matx_script_api.GetGlobal("ast.AsText", True)
        text = fn(self)
        return text.decode('utf-8') if isinstance(text, bytes) else text

class Type(Node):
    """类型节点基类"""
    pass

class BaseExpr(Node):
    """表达式节点基类"""
    pass

class Stmt(Node):
    """语句节点基类"""
    pass
```

### 类型定义

**原始类型**：
```python
@register_object("PrimType")
class PrimType(Type):
    def __init__(self, dtype):
        self.__init_handle_by_constructor__(prim_type_, dtype)

# 使用示例
int_type = PrimType("int64")
float_type = PrimType("float32")
```

**表达式类型**：
```python
@register_object("PrimExpr")
class PrimExpr(BaseExpr):
    def __init__(self, v):
        self.__init_handle_by_constructor__(prim_expr_, v)

@register_object("PrimVar")
class PrimVar(PrimExprWithOp):
    def __init__(self, name, datatype):
        self.__init_handle_by_constructor__(prim_var_, name, datatype)

# 使用示例
var_x = PrimVar("x", "int64")
const_42 = PrimExpr(42)
```

**语句类型**：
```python
@register_object("AllocaVarStmt")
class AllocaVarStmt(Stmt):
    def __init__(self, name_hint, type_annotation, init_value=None):
        self.__init_handle_by_constructor__(allocavar_stmt_, 
                                          name_hint, 
                                          type_annotation, 
                                          init_value)

@register_object("AssignStmt")
class AssignStmt(Stmt):
    def __init__(self, lhs, rhs):
        self.__init_handle_by_constructor__(assign_stmt_, lhs, rhs)

@register_object("ReturnStmt")
class ReturnStmt(Stmt):
    def __init__(self, value):
        self.__init_handle_by_constructor__(return_stmt_, value)
```

**函数类型**：
```python
@register_object("PrimFunc")
class PrimFunc(BaseFunc):
    def __init__(self, gs, fs, body, rt=None):
        self.__init_handle_by_constructor__(prim_func_, gs, fs, body, rt)
    
    def with_attr(self, name, value=None):
        """为函数添加属性"""
        res = func_cp_(self)
        return with_attr_(res, Str(name), Str(value))
```

## SimpleParser 解析器

### 解析器结构

`SimpleParser` 是一个基于 Python `ast.NodeVisitor` 的递归下降解析器，将 Python AST 转换为 MC IR：

```python
class SimpleParser(ast.NodeVisitor):
    _op_maker = {
        ast.Add: lambda lhs, rhs: add(lhs, rhs)
    }
    
    _ty_maker = {
        'int': lambda: PrimType("int64")
    }
    
    def __init__(self):
        self.context = None
        self.functions = {}
```

### 上下文管理

解析器使用 `ScopeContext` 管理作用域和符号表：

```python
def init_function_parsing_env(self):
    """初始化函数解析环境"""
    self.context = ScopeContext()
    
def visit_FunctionDef(self, node: ast.FunctionDef):
    """函数定义解析"""
    argtypes = []
    argnames = []
    
    # 初始化解析环境
    self.init_function_parsing_env()
    self.context.new_scope(nodes=node.body)
    
    # 处理函数参数
    for arg in node.args.args:
        var_type = self._ty_maker[arg.annotation.id]()
        argtypes.append(var_type)
        argnames.append(arg.arg)
        arg_var = PrimVar(arg.arg, var_type)
        self.context.update_symbol(arg.arg, arg_var)
        self.context.func_params.append(arg_var)
    
    # 处理返回类型
    ret_type = self._ty_maker[node.returns.id]()
    self.context.func_ret_type = ret_type
    
    # 构造函数 IR
    func = PrimFunc(
        Array(self.context.func_params),
        Array([]),
        self.parse_body(),
        ret_type
    )
    
    # 添加函数属性
    func = func.with_attr("GlobalSymbol", node.name)
    self.functions[node.name] = func
    
    self.context.pop_scope()
    return func
```

### 语句解析

**变量赋值**：
```python
def visit_Assign(self, node: ast.Assign):
    """赋值语句解析"""
    if len(node.targets) != 1:
        raise RuntimeError('Only one-valued assignment is supported')
    
    lhs_node = node.targets[0]
    rhs_node = node.value
    rhs_value = self.visit(rhs_node)
    lhs_name = lhs_node.id
    
    return self.lookup_or_alloca(lhs_name, rhs_value)

def lookup_or_alloca(self, name_hint, init_value):
    """查找或分配变量"""
    inf_ty = init_value.datatype
    symbol = self.context.lookup_symbol(name_hint)
    
    if symbol is None:
        # 首次使用，创建分配语句
        alloca_stmt = AllocaVarStmt(name_hint, inf_ty, init_value)
        self.context.update_symbol(name_hint, alloca_stmt.var)
        return alloca_stmt
    else:
        # 重新赋值，创建赋值语句
        return AssignStmt(symbol, init_value)
```

**二元运算**：
```python
def visit_BinOp(self, node):
    """二元运算解析"""
    lhs = self.visit(node.left)
    rhs = self.visit(node.right)
    op = self._op_maker[type(node.op)]
    return op(lhs, rhs)
```

**返回语句**：
```python
def visit_Return(self, node):
    """返回语句解析"""
    rt_expr = self.visit(node.value)
    return ReturnStmt(rt_expr)
```

### 作用域管理

`ScopeContext` 提供了完整的作用域管理机制：

```python
class ScopeContext:
    def __init__(self):
        self.node_stack = []      # AST 节点栈
        self.symbols = []         # 符号表栈
        self.func_params = []     # 函数参数列表
        self.func_ret_type = None # 函数返回类型
    
    def new_scope(self, nodes=None):
        """创建新作用域"""
        if nodes is None:
            nodes = []
        self.node_stack.append(list(reversed(nodes)))
        self.symbols.append(dict())
    
    def update_symbol(self, name, symbol):
        """更新符号表"""
        self.symbols[-1][name] = symbol
    
    def lookup_symbol(self, name):
        """查找符号"""
        for symbols in reversed(self.symbols):
            if name in symbols:
                return symbols[name]
        return None
```

## 编译函数

### simple_compile 函数

`simple_compile` 是编译的主要入口点，完成从 Python 函数到 C++ 代码的转换：

```python:514-545
def simple_compile(target, dso_path):
    """编译 Python 函数到 C++ 代码"""
    func_name = target.__name__
    
    # 1. 获取 Python 源代码
    source_code = inspect.getsource(target)
    
    # 2. 解析为 Python AST
    ast_tree = ast.parse(source_code)
    
    # 3. 转换为 MC IR
    parser = SimpleParser()
    func_ir = parser.visit(ast_tree)
    
    print(f"Generated IR: {func_ir}")
    print(f"IR type: {type(func_ir)}")
    
    # 4. 生成 C++ 代码
    to_source = matx_script_api.GetGlobal("rewriter.BuildFunction", True)
    code = to_source(func_ir, Str("fn"))
    
    print("Generated C++ code:")
    print(code.data)
```

### test_add 示例

```python:539-546
def test_add(a: int, b: int) -> int:
    c = a + b
    return c

def test_simple_compile():
    simple_compile(test_add, "add.so")

test_simple_compile()
```

**编译过程分析**：

1. **源代码提取**：`inspect.getsource(test_add)` 提取函数源代码
2. **AST 解析**：`ast.parse()` 生成 Python AST
3. **IR 转换**：`SimpleParser` 将 AST 转换为 MC IR
4. **代码生成**：`BuildFunction` 生成最终的 C++ 代码

**生成的 IR 结构**：
```
PrimFunc(
    params=[PrimVar('a', 'int64'), PrimVar('b', 'int64')],
    body=SeqStmt([
        AllocaVarStmt('c', 'int64', OpAdd(PrimVar('a'), PrimVar('b'))),
        ReturnStmt(PrimVar('c'))
    ]),
    ret_type=PrimType('int64')
)
```

## 测试与使用

### 基本使用流程

```python
# 1. 定义要编译的函数
def test_add(a: int, b: int) -> int:
    c = a + b
    return c

# 2. 编译函数
simple_compile(test_add, "add.so")

# 3. 加载编译后的模块
module_loader_ = matx_script_api.GetGlobal("runtime.ModuleLoader", True)
test_module = module_loader_("./test_func.so")

# 4. 获取函数并调用
test_func = test_module.get_function("test_func")
result = test_func(3, 4)
print(f"Result: {result}")
```

### 支持的语法特性

**当前支持**：
- 类型注解的函数定义
- 基本的二元运算（加法）
- 变量赋值
- 返回语句
- 整数和浮点数字面量

**限制**：
- 只支持单函数编译
- 类型系统限制为基本类型
- 运算符支持有限

## 扩展指南

### 添加新运算符

```python
class SimpleParser(ast.NodeVisitor):
    _op_maker = {
        ast.Add: lambda lhs, rhs: add(lhs, rhs),
        ast.Sub: lambda lhs, rhs: sub(lhs, rhs),  # 添加减法
        ast.Mult: lambda lhs, rhs: mul(lhs, rhs), # 添加乘法
    }
```

### 添加新类型

```python
class SimpleParser(ast.NodeVisitor):
    _ty_maker = {
        'int': lambda: PrimType("int64"),
        'float': lambda: PrimType("float64"),  # 添加浮点类型
        'bool': lambda: PrimType("bool"),      # 添加布尔类型
    }
```

### 添加新语句类型

```python
def visit_If(self, node):
    """条件语句解析"""
    condition = self.visit(node.test)
    then_body = self.parse_body(node.body)
    else_body = self.parse_body(node.orelse) if node.orelse else None
    return IfStmt(condition, then_body, else_body)
```

MC 编译器提供了一个完整的从 Python 到 C++ 的编译路径，通过 AST 解析、IR 生成和代码生成等步骤，实现了高效的跨语言编译。其模块化的设计使得扩展新的语法特性和类型系统变得相对容易。