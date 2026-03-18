#include <iostream>
#include <string>
#include <stdint.h>
#include <fstream>

#include "object.h"
#include "runtime_value.h"
#include "parameters.h"
#include "registry.h"
#include "iterator.h"
#include "container.h"
#include "expression.h"
#include "statement.h"
#include "function.h"
#include "array.h"
#include "ops.h"
#include "printer.h"
#include "visitor.h"
#include "rewriter.h"
#include "runtime_module.h"

namespace mc {
namespace runtime {

class StrObject : public object_t {
public:
  static constexpr const std::string_view NAME = "Str";
  DEFINE_TYPEINDEX(StrObject, object_t);

  explicit StrObject(std::string value) 
    : value_(std::move(value)) {
  }

  const std::string& Value() const { return value_; }


private:
  std::string value_;
};


void test_object() {
  auto str_obj = MakeObject<StrObject>("x");    
  McValue obj_val(str_obj);
  McView obj_view(obj_val);

  //if (obj_view.IsObjectType<StrObject>()) {
  //  const StrObject* p = obj_view.As<const StrObject*>();
  //  std::cout << p->Value() << "\n";
  //}

}

static McValue TestAsText(Parameters ps) {
    Any value = ps[0];
    auto raw_ptr = static_cast<object_t*>(value.As_<void*>());
    object_r node(object_p<object_t>(
                static_cast<object_t*>(value.As_<void*>())));
    Doc doc;
    doc << TextPrinter().Print(node); 
    //std::cout << doc.str() << std::endl;
    return McValue(doc.str());
}


void test_register() {
    auto func = [](Parameters ps) -> McValue {
        return McValue();
    };

    REGISTER_FUNCTION("func1", func);
    REGISTER_FUNCTION("func2", func);

    if (auto f = FunctionRegistry::Get("func1")) {
    }

    auto rs = FunctionRegistry::ListNames();
    for (auto s : rs) {
        std::cout << s << "\n";
    }

    auto add = [](int a, int b) -> McValue {
        return McValue(a+b);
    };

    REGISTER_GLOBAL("add").SetBody(add);

    auto a = FunctionRegistry::Get("add");

    Any args[2];
    args[0] = McValue(10);
    args[1] = McValue(20);
    Parameters params(args, 2);

    if (a != nullptr) {
        McValue result = (*a)(params);
        std::cout << result.As<int>() << std::endl;
    }
}

void test_convert() {
    auto test = [](PrimExpr x) -> McValue {
        PrimVar z("n", DataType::Bool());
        return McValue(z);
    };

    //REGISTER_GLOBAL("test").SetBody(test);

    //auto a = FunctionRegistry::Get("test");

    Any args[1];
    auto v = PrimExpr(1);
    args[0] = McValue(v);
    Parameters params(args, 1);

    //if (a != nullptr) {
        //McValue result = (*a)(params);
        //std::cout << result.As<int>() << std::endl;
    //}
}

class VectorIteratorNode : public IteratorNode {
private:
  std::vector<McValue> data_;
  size_t current_ = 0;
};

void test_dict() {
    Dict x{{"u", "v"}};
    std::vector<Dict::value_type> xs{{"u","v"}};
    auto new_dict = Dict(x.begin(), x.end());

    auto v = x["u"];
    std::cout << v.As<const char*>() << "\n";

    std::cout << "Name " << x->Name() << std::endl;
}

void test_set() {
    Set x{McValue(1L), McValue(2L), McValue("s")};
    for (const auto& item : x) {
        if (item.IsInt()) {
            std::cout << item.As<int64_t>() << " ";
        } else if (item.IsStr()) {
            std::cout << item.As<const char*>() << " ";
        }
    }
    std::cout << "\n";
}

void test_list() {
  object_p<ListNode> empty_node = MakeObject<ListNode>();
  List empty_list{object_p<object_t>(empty_node)};

  List list1{McValue(1L), McValue(2.5F), McValue("hello")};

  List list2(3, McValue(0L));

  std::vector<McValue> vec{McValue(1L), McValue(2L), McValue(3L)};
  List list3(vec.begin(), vec.end());

    list1.push_back(McValue(42L));
    list1.append(3.14F);
    
    std::cout << "Size: " << list1.size() << "\n";
    
    for (const auto& item : list1) {
        if (item.IsInt()) {
            std::cout << item.As<int64_t>() << " ";
        } else if (item.IsFloat()) {
            std::cout << item.As<double>() << " ";
        } else if (item.IsStr()) {
            std::cout << item.As<const char*>() << " ";
        }
    }
    std::cout << "\n";

}

static std::string AsText(const object_r& node) {
    std::cout << "AsText" << std::endl;
    Doc doc;
    doc << TextPrinter().Print(node);
    return doc.str();
}

static std::string AsText_u(const Any& value) {
    auto node = object_r(object_p<object_t>(
                static_cast<object_t*>(value.As_<void*>())));
    std::cout << "AsText" << std::endl;
    Doc doc;
    doc << TextPrinter().Print(node);
    return doc.str();
}

//REGISTER_GLOBAL("ir.AsText").SetBody(AsText);

void test_expr() {
    PrimExpr a(3);
    AstPrinter pt;

    Doc doc = pt.Print(a);
    std::cout << "PrintAst" << "\n";
    std::cout << doc.str() << "\n";

    PrimType t(DataType::Int(32));

    doc = pt.Print(t);
    std::cout << doc.str() << "\n";
}

void test_op() {
    PrimVar a("a", DataType::Int(64));
    Any x;
    x = McView(McValue(a));

    std::cout << (x.Is<PrimExpr>()) << "\n";

    auto u = x.As<PrimExpr>();


    PrimVar b("b", DataType::Int(64));
    auto c = op_add(a, b);

    AstPrinter pt;

    Doc doc = pt.Print(c);
    std::cout << "PrintAst" << "\n";
    std::cout << doc.str() << "\n";
}

void test_expression() {

    PrimExpr a(3);
    PrimExpr b(4);
    PrimAdd c(a, b);
    PrimMul d(c, a);
    Bool cond(true);
    //PrimExpr r = op_if_then_else(cond, d, c);
    PrimCall custome(d->datatype, if_then_else(), {cond, d, c});

    Array<Stmt> seq_stmt;
    seq_stmt.push_back(Evaluate(custome));

    SeqStmt body(seq_stmt);
    Array<PrimVar> params{PrimVar("n", DataType::Bool())};
    PrimFunc func(params, {}, body, PrimType(DataType::Int(32)));

    //AstPrinter pt;
    //Doc doc = pt.Print(func);
    //std::cout << "PrintAst" << "\n";
    //std::cout << doc.str() << "\n";

    //FunctionRegistry::Register("ir.AsText").SetBody(AsText);
    //FunctionRegistry::SetFunction("ir.AsText", TestAsText);

    const auto* printer = FunctionRegistry::Get("ir.AsText");
    if (!printer || !printer->operator bool()) {  
        throw std::runtime_error("Function ir.AsText not found");
    }

    IRModule module = IRModule::From(func);
    Any args[1];
    args[0] = McValue(module);
    Parameters ps(args, 1);
    McValue r = (*printer)(ps);

    std::cout << r.As<const char*>() << std::endl;
    std::cout << "Done\n";

    TextPrinter pt;
    Doc doc = pt.Print(module);
    std::cout << "PrintAst" << "\n";
    std::cout << doc.str() << "\n";
}

void test_stmt() {
    DataType int_t = DataType::Int(64);
    const auto* printer = FunctionRegistry::Get("ast.AsText");

    AllocaVarStmt st("x", int_t, IntImm(int_t, 0));

    AstPrinter pt;
    Doc doc = pt.Print(st);
    std::cout << doc.str() << std::endl;

    AssignStmt a(st->var, PrimExpr(10));

    doc = pt.Print(a);
    std::cout << doc.str() << std::endl;

    ReturnStmt r(st->var);

    doc = pt.Print(r);
    std::cout << doc.str() << std::endl;

    Array<Stmt> s;
    s.push_back(st);
    s.push_back(a);
    s.push_back(r);

    SeqStmt body(s);

    doc = pt.Print(body);
    std::cout << doc.str() << std::endl;

    Array<PrimVar> gs{};
    PrimFunc func(gs, {}, body, PrimType(DataType::Int(32)));

    std::cout << "PrintFunc" << std::endl;
    doc = pt.Print(func);
    std::cout << doc.str() << std::endl;
    func = WithAttr(std::move(func), Str("GlobalSymbol"), Str("test"));
    doc = pt.Print(func);
    std::cout << doc.str() << std::endl;


    //auto mcv = McValue(func); 
    //McView view = mcv;  
    //Parameters params{view};
    //auto ir_text = (*printer)({func});
    //std::cout << ir_text.As<const char*>() << std::endl;
}

void test_tuple() {
    Tuple e{McValue(1L), McValue(3.14F), McValue("x")};

    std::cout << e[0].As<int64_t>() << "\n";
    std::cout << e[1].As<double>() << "\n";
    std::cout << e[2].As<const char*>() << "\n";

    for (const auto& item : e) {
        if (item.IsInt()) {
            std::cout << item.As<int64_t>() << " ";
        } else if (item.IsStr()) {
            std::cout << item.As<const char*>() << " ";
        }
    }
    std::cout << "\n";

    const auto* new_tuple = FunctionRegistry::Get("runtime.Tuple");

    PrimVar x("x", DataType::Int(32));
    Any gs[1];
    gs[0] = McValue(x);
    Parameters ps(gs,1);

    std::cout << "B new" << "\n";
    McValue r = (*new_tuple)(ps);

    std::cout << "B As" << "\n";
    Tuple t = r.As<Tuple>();

    std::cout << t.size() << std::endl;
}

void test_rewriter() {

    // int32_t test_func(int32_t a, int32_t b) {
    //   int32_t c = a + b;
    //   c = c * b;
    //   return c;
    // }

    auto a = PrimVar("a", DataType::Int(32));
    auto b = PrimVar("b", DataType::Int(32));
    Array<PrimVar> params;
    params.push_back(a);
    params.push_back(b);

    Array<Stmt> stmts;

    auto init_c = AllocaVarStmt("c", DataType::Int(32), PrimAdd(a, PrimExpr(b)));
    stmts.push_back(init_c);
    auto c = init_c->var;

    // c = c * b;
    auto mul_expr = PrimMul(c, PrimExpr(b));
    auto assign_c = AssignStmt(c, mul_expr);
    stmts.push_back(assign_c);

    // return c;
    auto ret = ReturnStmt(c);
    stmts.push_back(ret);

    auto body = SeqStmt(stmts);
    
    auto ret_type = PrimType(DataType::Int(32));
    auto func = PrimFunc(params, {}, body, ret_type);

    SourceRewriter rt;
    //rt.ResetState();
    rt.Init();
    rt.InsertFunction(func);

    std::cout << rt.Done() << std::endl;
}

void test_if_comparison_rewriter() {
    std::cout << "\n=== test_if_comparison_rewriter ===" << std::endl;

    // fn(a: int64, b: int64) -> int64:
    //   if a < b:
    //     return a
    //   else:
    //     return b
    Array<PrimVar> params;
    params.push_back(PrimVar("a", DataType::Int(64)));
    params.push_back(PrimVar("b", DataType::Int(64)));

    PrimExpr cond = PrimLt(params[0], params[1]);
    ReturnStmt then_ret(params[0]);
    ReturnStmt else_ret(params[1]);

    Array<Stmt> then_body;
    then_body.push_back(then_ret);
    SeqStmt then_stmt(then_body);

    Array<Stmt> else_body;
    else_body.push_back(else_ret);
    SeqStmt else_stmt(else_body);

    IfStmt if_stmt(cond, then_stmt, else_stmt);
    Array<Stmt> stmts;
    stmts.push_back(if_stmt);
    SeqStmt body(stmts);

    PrimFunc func(params, {}, body, PrimType(DataType::Int(64)));
    SourceRewriter rewriter;
    rewriter.Init();
    rewriter.InsertFunction(func);
    std::cout << rewriter.Done() << std::endl;
}

void test_pointer_mcvalue() {
    std::cout << "\n=== test_pointer_mcvalue ===" << std::endl;
    
    // Test 1: Direct void* constructor
    void* test_ptr = (void*)0x12345678;
    McValue ptr_val(test_ptr);
    std::cout << "Created McValue with void* constructor" << std::endl;
    std::cout << "TypeIndex: " << ptr_val.T() << std::endl;
    std::cout << "Expected TypeIndex::Pointer: " << TypeIndex::Pointer << std::endl;
    std::cout << "Retrieved pointer: " << ptr_val.As<void*>() << std::endl;
    std::cout << "Original pointer: " << test_ptr << std::endl;
    
    // Test 2: Value -> McValue conversion
    Value v;
    v.t = TypeIndex::Pointer;
    v.u.v_pointer = test_ptr;
    McView view(&v);
    McValue val_from_value{view};
    std::cout << "Created McValue from Value with TypeIndex::Pointer" << std::endl;
    std::cout << "TypeIndex: " << val_from_value.T() << std::endl;
    std::cout << "Retrieved pointer: " << val_from_value.As<void*>() << std::endl;
    
    std::cout << "=== test_pointer_mcvalue completed ===" << std::endl;
}

void test_end_to_end_class_compilation() {
    std::cout << "\n=== test_end_to_end_class_compilation ===" << std::endl;
    
    // 1. 生成类的C++代码并写入文件
    std::cout << "Step 1: Generating C++ code for TestClass..." << std::endl;
    
    // 构建类定义 (复用之前的代码)
    PrimVar self_param("self", DataType::Handle());
    PrimVar value_param("value", DataType::Int(64));
    Array<PrimVar> init_params;
    init_params.push_back(self_param);
    init_params.push_back(value_param);

    StrImm value_member("value");
    ClassGetItem self_value(self_param, value_member);
    AssignStmt assign_value(self_value, value_param);

    Array<Stmt> init_body;
    init_body.push_back(assign_value);
    SeqStmt init_stmt_body(init_body);

    PrimFunc init_method(init_params, {}, init_stmt_body, PrimType(DataType::Void()));
    init_method = WithAttr(init_method, Str("MethodName"), Str("__init__"));
    init_method = WithAttr(init_method, Str("ClassMembers"), Str("value:int64_t"));

    Array<PrimVar> get_params;
    get_params.push_back(PrimVar("self", DataType::Handle()));

    ClassGetItem get_self_value(PrimVar("self", DataType::Handle()), StrImm("value"));
    ReturnStmt return_value(get_self_value);

    Array<Stmt> get_body;
    get_body.push_back(return_value);
    SeqStmt get_stmt_body(get_body);

    PrimFunc get_method(get_params, {}, get_stmt_body, PrimType(DataType::Int(64)));
    get_method = WithAttr(get_method, Str("MethodName"), Str("get_value"));
    get_method = WithAttr(get_method, Str("MethodType"), Str("getter"));
    get_method = WithAttr(get_method, Str("ClassMembers"), Str("value:int64_t"));

    Array<BaseExpr> class_methods;
    class_methods.push_back(init_method);
    class_methods.push_back(get_method);

    ClassStmt test_class(Str("TestClass"), class_methods);

    // 生成C++代码
    SourceRewriter rewriter;
    rewriter.Init();
    rewriter.InsertClass(test_class);
    std::string cpp_code = rewriter.Done();
    
    // 2. 将C++代码写入文件
    std::cout << "Step 2: Writing C++ code to test_class.cpp..." << std::endl;
    std::ofstream cpp_file("test_class.cpp");
    cpp_file << cpp_code;
    cpp_file.close();
    
    // 3. 编译为.so文件 (需要系统调用)
    std::cout << "Step 3: Compiling to shared library..." << std::endl;
    std::string compile_cmd = "g++ -shared -fPIC -O2 test_class.cpp -I./src -L./build -lcase -o test_class.so";
    int result = system(compile_cmd.c_str());
    if (result != 0) {
        std::cout << "Compilation failed!" << std::endl;
        return;
    }
    
    // 4. 使用ModuleLoader加载.so文件
    std::cout << "Step 4: Loading shared library with ModuleLoader..." << std::endl;
    
    const auto* module_loader = FunctionRegistry::Get("runtime.ModuleLoader");
    if (!module_loader) {
        std::cout << "ModuleLoader not found!" << std::endl;
        return;
    }
    std::cout << "ModuleLoader found, calling with test_class.so..." << std::endl;
    
    Any args[1];
    //args[0] = McValue("./test_class.so");
    auto v = McValue("./test_class.so");
    args[0] = v;
    Parameters ps(args, 1);

    std::cout << "About to call ModuleLoader..." << std::endl;
    McValue module_result = (*module_loader)(ps);
    std::cout << "ModuleLoader returned successfully!" << std::endl;
    Module loaded_module = module_result.As<Module>();
    
    std::cout << "Module loaded successfully!" << std::endl;
    
    // 5. 测试类方法调用
    std::cout << "Step 5: Testing class method calls..." << std::endl;
    
    // 首先查看生成的C++代码内容
    std::cout << "Generated C++ code:" << std::endl;
    std::cout << "===========================================" << std::endl;
    std::cout << cpp_code << std::endl;
    std::cout << "===========================================" << std::endl;
    
    // 获取实际生成的函数名列表
    std::cout << "Looking for class functions..." << std::endl;
    
    // 根据class_methods_的存储规则，函数名应该是：
    // TestClass.init__ 和 TestClass.get_value
    // 在函数作用域中声明变量，确保生命周期足够长
    McValue init_result;
    void* created_object = nullptr;
    
    Function init_func = loaded_module.GetFunction("TestClass.init__", NTcast<object_t>(const_cast<object_t*>(loaded_module.get())));
    if (init_func) {
        std::cout << "✓ Found TestClass.init__ function!" << std::endl;
        
        // 测试构造函数调用和对象生命周期管理
        std::cout << "Testing constructor call..." << std::endl;
        
        try {
            // 构造函数参数：只需要初始值，不需要对象指针
            Any args[1];
            args[0] = McValue(42); // 初始值
            Parameters params(args, 1);
            
            init_result = init_func(params);
            created_object = init_result.As<void*>();
            std::cout << "Constructor executed successfully!" << std::endl;
            std::cout << "Constructor returned object pointer: " << created_object << std::endl;
            
            // 验证对象确实被创建且包含正确的值
            // 直接访问生成的结构体来验证
            if (created_object != nullptr) {
                // 根据生成的代码，对象结构是TestClass_Data
                struct TestClass_Data {
                    int64_t value;
                };
                TestClass_Data* obj = static_cast<TestClass_Data*>(created_object);
                std::cout << "Object created with value: " << obj->value << std::endl;
                std::cout << "Expected value: 42, Actual value: " << obj->value << std::endl;
                
                if (obj->value == 42) {
                    std::cout << "✓ Object initialization verified!" << std::endl;
                } else {
                    std::cout << "✗ Object initialization failed!" << std::endl;
                }
            }
            
        } catch (const std::exception& e) {
            std::cout << "Constructor execution failed: " << e.what() << std::endl;
        }
        
        // 在这里init_result仍然有效，对象指针不会悬空
        
    } else {
        std::cout << "✗ TestClass.init__ function not found!" << std::endl;
    }
    
    // 现在测试get_value方法，使用生命周期安全的对象指针
    std::cout << "Testing get_value method..." << std::endl;
    Function get_value_func = loaded_module.GetFunction("TestClass.get_value", NTcast<object_t>(const_cast<object_t*>(loaded_module.get())));
    if (get_value_func && created_object != nullptr) {
        std::cout << "✓ Found TestClass.get_value function!" << std::endl;
        
        try {
            // 创建另一个对象用于测试get_value
            Any init_args[1];
            init_args[0] = McValue(123); // 不同的初始值
            Parameters init_params(init_args, 1);
            
            McValue test_obj_result = init_func(init_params);
            void* test_obj_ptr = test_obj_result.As<void*>();
            
            // 使用直接的Value构造来传递对象指针
            // 这是临时解决方案，理想情况下应该有更好的API
            std::cout << "Attempting to call get_value method..." << std::endl;
            std::cout << "Test object pointer: " << test_obj_ptr << std::endl;
            
            // 现在尝试真正的函数调用
            if (test_obj_ptr != nullptr) {
                // 先验证直接访问
                struct TestClass_Data {
                    int64_t value;
                };
                TestClass_Data* test_obj = static_cast<TestClass_Data*>(test_obj_ptr);
                std::cout << "Test object contains value: " << test_obj->value << std::endl;
                
                // 现在尝试通过get_value函数调用获取值
                std::cout << "Attempting actual get_value function call..." << std::endl;
                
                // 使用新的McValue(void*)构造函数直接传递对象指针
                Any get_args[1];
                get_args[0] = McValue(test_obj_ptr);  // 现在支持void*了！
                Parameters get_params(get_args, 1);
                
                try {
                    McValue get_result = get_value_func(get_params);
                    int64_t returned_value = get_result.As<int64_t>();
                    std::cout << "✓ get_value function call successful!" << std::endl;
                    std::cout << "Function returned value: " << returned_value << std::endl;
                    std::cout << "Expected: 123, Actual: " << returned_value << std::endl;
                    
                    if (returned_value == 123) {
                        std::cout << "✓ get_value function call verification successful!" << std::endl;
                    } else {
                        std::cout << "✗ get_value function call returned wrong value!" << std::endl;
                    }
                } catch (const std::exception& e) {
                    std::cout << "✗ get_value function call failed: " << e.what() << std::endl;
                }
                std::cout << "✓ get_value method verification by direct access successful!" << std::endl;
            }
            
        } catch (const std::exception& e) {
            std::cout << "get_value test failed: " << e.what() << std::endl;
        }
        
    } else if (!get_value_func) {
        std::cout << "✗ TestClass.get_value function not found!" << std::endl;
    } else {
        std::cout << "✗ No valid object to test get_value method!" << std::endl;
    }
    
    // 列出模块中所有可用的函数
    std::cout << "\nAll available functions in the module:" << std::endl;
    // 这需要实现一个ListFunctions方法，或者通过其他方式获取函数列表
    
    std::cout << "End-to-end class compilation test completed!" << std::endl;
}

void test_class_rewrite() {
    std::cout << "\n=== test_class_rewrite ===" << std::endl;

    // 构建一个简单的类定义
    // class TestClass:
    //     def __init__(self, value: int):
    //         self.value = value
    //     def get_value(self) -> int:
    //         return self.value

    // 1. 创建 __init__ 方法
    PrimVar self_param("self", DataType::Handle());
    PrimVar value_param("value", DataType::Int(64));
    Array<PrimVar> init_params;
    init_params.push_back(self_param);
    init_params.push_back(value_param);

    // self.value = value
    StrImm value_member("value");
    ClassGetItem self_value(self_param, value_member);
    AssignStmt assign_value(self_value, value_param);

    Array<Stmt> init_body;
    init_body.push_back(assign_value);
    SeqStmt init_stmt_body(init_body);

    PrimFunc init_method(init_params, {}, init_stmt_body, PrimType(DataType::Void()));
    init_method = WithAttr(init_method, Str("MethodName"), Str("__init__"));
    init_method = WithAttr(init_method, Str("ClassMembers"), Str("value:int64_t"));

    // 2. 创建 get_value 方法
    Array<PrimVar> get_params;
    get_params.push_back(PrimVar("self", DataType::Handle()));

    ClassGetItem get_self_value(PrimVar("self", DataType::Handle()), StrImm("value"));
    ReturnStmt return_value(get_self_value);

    Array<Stmt> get_body;
    get_body.push_back(return_value);
    SeqStmt get_stmt_body(get_body);

    PrimFunc get_method(get_params, {}, get_stmt_body, PrimType(DataType::Int(64)));
    get_method = WithAttr(get_method, Str("MethodName"), Str("get_value"));
    get_method = WithAttr(get_method, Str("MethodType"), Str("getter"));
    get_method = WithAttr(get_method, Str("ClassMembers"), Str("value:int64_t"));

    // 3. 创建类定义
    Array<BaseExpr> class_methods;
    class_methods.push_back(init_method);
    class_methods.push_back(get_method);

    ClassStmt test_class(Str("TestClass"), class_methods);

    // 4. 使用 SourceRewriter 生成 C++ 代码
    SourceRewriter rewriter;
    rewriter.Init();
    rewriter.InsertClass(test_class);

    std::cout << "Generated C++ code:\n";
    std::cout << "==================\n";
    std::cout << rewriter.Done() << std::endl;
    std::cout << "==================\n";
}

void test_container_compilation() {
    std::cout << "\n=== test_container_compilation ===" << std::endl;
    
    // 创建一个使用容器的函数
    // def container_demo():
    //     my_list = [1, 2, 3]
    //     my_list.append(4)
    //     my_list[0] = 10
    //     first = my_list[0]
    //     
    //     my_dict = {'a': 1, 'b': 2}
    //     my_dict['c'] = 3
    //     value = my_dict.get('a', 0)
    //     
    //     my_set = {1, 2, 3}
    //     my_set.add(4)
    //     
    //     return first + value
    
    Array<PrimVar> params;  // 无参数函数
    Array<Stmt> stmts;
    
    // my_list = [1, 2, 3]
    Array<PrimExpr> list_elements;
    list_elements.push_back(PrimExpr(1));
    list_elements.push_back(PrimExpr(2));
    list_elements.push_back(PrimExpr(3));
    ListLiteral my_list_literal(list_elements);
    auto my_list_alloc = AllocaVarStmt("my_list", DataType::Handle(), my_list_literal);
    stmts.push_back(my_list_alloc);
    auto my_list_var = my_list_alloc->var;
    
    // my_list.append(4)
    Array<PrimExpr> append_args;
    append_args.push_back(PrimExpr(4));
    ContainerMethodCall append_call(my_list_var, StrImm("append"), append_args);
    stmts.push_back(Evaluate(append_call));
    
    // my_list[0] = 10
    ContainerSetItem set_item(my_list_var, PrimExpr(0), PrimExpr(10));
    stmts.push_back(Evaluate(set_item));
    
    // first = my_list[0]
    ContainerGetItem get_first(my_list_var, PrimExpr(0));
    auto first_alloc = AllocaVarStmt("first", DataType::Handle(), get_first);
    stmts.push_back(first_alloc);
    auto first_var = first_alloc->var;
    
    // my_dict = {'a': 1, 'b': 2}
    Array<PrimExpr> dict_keys;
    Array<PrimExpr> dict_values;
    dict_keys.push_back(StrImm("a"));
    dict_keys.push_back(StrImm("b"));
    dict_values.push_back(PrimExpr(1));
    dict_values.push_back(PrimExpr(2));
    DictLiteral my_dict_literal(dict_keys, dict_values);
    auto my_dict_alloc = AllocaVarStmt("my_dict", DataType::Handle(), my_dict_literal);
    stmts.push_back(my_dict_alloc);
    auto my_dict_var = my_dict_alloc->var;
    
    // my_dict['c'] = 3
    ContainerSetItem dict_set(my_dict_var, StrImm("c"), PrimExpr(3));
    stmts.push_back(Evaluate(dict_set));
    
    // value = my_dict.get('a', 0)
    Array<PrimExpr> get_args;
    get_args.push_back(StrImm("a"));
    get_args.push_back(PrimExpr(0));
    ContainerMethodCall dict_get(my_dict_var, StrImm("get"), get_args);
    auto value_alloc = AllocaVarStmt("value", DataType::Handle(), dict_get);
    stmts.push_back(value_alloc);
    auto value_var = value_alloc->var;
    
    // my_set = {1, 2, 3}
    Array<PrimExpr> set_elements;
    set_elements.push_back(PrimExpr(1));
    set_elements.push_back(PrimExpr(2));
    set_elements.push_back(PrimExpr(3));
    SetLiteral my_set_literal(set_elements);
    auto my_set_alloc = AllocaVarStmt("my_set", DataType::Handle(), my_set_literal);
    stmts.push_back(my_set_alloc);
    auto my_set_var = my_set_alloc->var;
    
    // my_set.add(4)
    Array<PrimExpr> add_args;
    add_args.push_back(PrimExpr(4));
    ContainerMethodCall set_add(my_set_var, StrImm("add"), add_args);
    stmts.push_back(Evaluate(set_add));
    
    // return first + value (simplified, just return first for now)
    ReturnStmt return_stmt(first_var);
    stmts.push_back(return_stmt);
    
    auto body = SeqStmt(stmts);
    auto ret_type = PrimType(DataType::Handle());
    auto func = PrimFunc(params, {}, body, ret_type);
    func = WithAttr(std::move(func), Str("GlobalSymbol"), Str("container_demo"));
    
    // 使用 SourceRewriter 生成 C++ 代码
    SourceRewriter rewriter;
    rewriter.Init();
    rewriter.InsertFunction(func);
    
    std::string cpp_code = rewriter.Done();
    
    std::cout << "Generated C++ code for container operations:\n";
    std::cout << "==========================================\n";
    std::cout << cpp_code << std::endl;
    std::cout << "==========================================\n";
    
    // 将C++代码写入文件
    std::cout << "Step 2: Writing C++ code to container_demo.cpp..." << std::endl;
    std::ofstream cpp_file("container_demo.cpp");
    cpp_file << cpp_code;
    cpp_file.close();
    
    // 编译为.so文件
    std::cout << "Step 3: Compiling to shared library..." << std::endl;
    std::string compile_cmd = "g++ -shared -fPIC -O2 container_demo.cpp -I./src -L./build -lcase -o container_demo.so";
    int result = system(compile_cmd.c_str());
    if (result != 0) {
        std::cout << "Compilation failed!" << std::endl;
        return;
    }
    
    // 使用ModuleLoader加载.so文件
    std::cout << "Step 4: Loading shared library with ModuleLoader..." << std::endl;
    
    const auto* module_loader = FunctionRegistry::Get("runtime.ModuleLoader");
    if (!module_loader) {
        std::cout << "ModuleLoader not found!" << std::endl;
        return;
    }
    
    Any args[1];
    auto v = McValue("./container_demo.so");
    args[0] = v;
    Parameters ps(args, 1);
    
    std::cout << "About to call ModuleLoader..." << std::endl;
    McValue module_result = (*module_loader)(ps);
    std::cout << "ModuleLoader returned successfully!" << std::endl;
    Module loaded_module = module_result.As<Module>();
    
    std::cout << "Module loaded successfully!" << std::endl;
    
    // 测试函数调用
    std::cout << "Step 5: Testing container function execution..." << std::endl;
    
    Function container_func = loaded_module.GetFunction("container_demo", NTcast<object_t>(const_cast<object_t*>(loaded_module.get())));
    if (container_func) {
        std::cout << "Found container_demo function!" << std::endl;
        
        // 调用函数（无参数）
        Any func_args[0];
        Parameters func_params(func_args, 0);
        
        std::cout << "Calling container_demo()..." << std::endl;
        McValue result = container_func(func_params);
        std::cout << "Function executed successfully!" << std::endl;
        
        // 输出结果
        if (result.IsObject()) {
            std::cout << "Result type: Object" << std::endl;
        } else {
            std::cout << "Result: " << result.As<int64_t>() << std::endl;
        }
    } else {
        std::cout << "container_demo function not found!" << std::endl;
    }
    
    std::cout << "Container compilation test completed!" << std::endl;
}

void test_container_print() {
    std::cout << "\n=== test_container_print ===" << std::endl;
    
    AstPrinter pt;
    Doc doc;

    std::cout << "\n1. Testing ListLiteral" << std::endl;
    // [1, 2, 3]
    Array<PrimExpr> list_elements;
    list_elements.push_back(PrimExpr(1));
    list_elements.push_back(PrimExpr(2));
    list_elements.push_back(PrimExpr(3));
    ListLiteral list_literal(list_elements);
    doc = pt.Print(list_literal);
    std::cout << "ListLiteral: " << doc.str() << std::endl;

    std::cout << "\n2. Testing DictLiteral" << std::endl;
    // {'a': 1, 'b': 2}
    Array<PrimExpr> dict_keys;
    Array<PrimExpr> dict_values;
    dict_keys.push_back(StrImm("a"));
    dict_keys.push_back(StrImm("b"));
    dict_values.push_back(PrimExpr(1));
    dict_values.push_back(PrimExpr(2));
    DictLiteral dict_literal(dict_keys, dict_values);
    doc = pt.Print(dict_literal);
    std::cout << "DictLiteral: " << doc.str() << std::endl;

    std::cout << "\n3. Testing SetLiteral" << std::endl;
    // {1, 2, 3}
    Array<PrimExpr> set_elements;
    set_elements.push_back(PrimExpr(1));
    set_elements.push_back(PrimExpr(2));
    set_elements.push_back(PrimExpr(3));
    SetLiteral set_literal(set_elements);
    doc = pt.Print(set_literal);
    std::cout << "SetLiteral: " << doc.str() << std::endl;

    std::cout << "\n4. Testing ContainerGetItem" << std::endl;
    // my_list[0]
    PrimVar my_list("my_list", DataType::Handle());
    ContainerGetItem get_item(my_list, PrimExpr(0));
    doc = pt.Print(get_item);
    std::cout << "ContainerGetItem: " << doc.str() << std::endl;

    std::cout << "\n5. Testing ContainerSetItem" << std::endl;
    // my_list[0] = 42
    ContainerSetItem set_item(my_list, PrimExpr(0), PrimExpr(42));
    doc = pt.Print(set_item);
    std::cout << "ContainerSetItem: " << doc.str() << std::endl;

    std::cout << "\n6. Testing ContainerMethodCall" << std::endl;
    // my_list.append(4)
    Array<PrimExpr> append_args;
    append_args.push_back(PrimExpr(4));
    ContainerMethodCall method_call(my_list, StrImm("append"), append_args);
    doc = pt.Print(method_call);
    std::cout << "ContainerMethodCall: " << doc.str() << std::endl;

    std::cout << "\n7. Testing nested containers" << std::endl;
    // [[1, 2], [3, 4]]
    Array<PrimExpr> inner_list1;
    inner_list1.push_back(PrimExpr(1));
    inner_list1.push_back(PrimExpr(2));
    ListLiteral inner1(inner_list1);
    
    Array<PrimExpr> inner_list2;
    inner_list2.push_back(PrimExpr(3));
    inner_list2.push_back(PrimExpr(4));
    ListLiteral inner2(inner_list2);
    
    Array<PrimExpr> outer_list;
    outer_list.push_back(inner1);
    outer_list.push_back(inner2);
    ListLiteral nested_list(outer_list);
    doc = pt.Print(nested_list);
    std::cout << "Nested ListLiteral: " << doc.str() << std::endl;

    std::cout << "\nContainer print test completed!" << std::endl;
}

void test_class() {

    AstPrinter pt;
    Doc doc;

    std::cout << "\n1. StrImm" << std::endl;
    StrImm str_("_i");
    doc = pt.Print(str_); 
    std::cout << "StrImm: " << doc.str() << std::endl;

    std::cout << "\n2. ClassGetItem" << std::endl;
    PrimVar var_("self", DataType::Handle());
    ClassGetItem attr_access(var_, str_);
    doc = pt.Print(attr_access);
    std::cout << "ClassGetItem: " << doc.str() << std::endl;

    std::cout << "\n3. __init__" << std::endl;
    // def __init__(self, i:int) -> None:
    //      self._i = i

    PrimVar i_("i", DataType::Int(64));
    Array<PrimVar> init_params;
    init_params.push_back(PrimVar("self", DataType::Handle()));
    init_params.push_back(i_);

    StrImm m_("_i");
    ClassGetItem s_m(PrimVar("self", DataType::Handle()), m_);
    AssignStmt a_m(s_m, i_);

    Array<Stmt> init_body;
    init_body.push_back(a_m);
    SeqStmt init_stmt_body(init_body);

    PrimFunc init_method(init_params, {}, init_stmt_body, PrimType(DataType::Handle()));

    doc = pt.Print(init_method);
    std::cout << "__init__ method:\n" << doc.str() << std::endl;

    std::cout << "\n4. test " << std::endl;
    // def test(self, j:int) -> int:
    //     return self._i + j
    PrimVar j_("j", DataType::Int(64));
    Array<PrimVar> test_params;
    test_params.push_back(PrimVar("self", DataType::Handle()));
    test_params.push_back(j_);

    ClassGetItem s_i(PrimVar("self", DataType::Handle()), StrImm("_i"));
    PrimAdd a_e(s_i, j_);
    ReturnStmt return_stmt(a_e);

    Array<Stmt> test_body;
    test_body.push_back(return_stmt);
    SeqStmt test_stmt_body(test_body);

    PrimFunc test_method(test_params, {}, test_stmt_body, PrimType(DataType::Int(64)));

    doc = pt.Print(test_method);
    std::cout << "test method:\n" << doc.str() << std::endl;

    std::cout << "\n5. ClassStmt" << std::endl;

    Array<BaseExpr> class_methods;
    class_methods.push_back(init_method);
    class_methods.push_back(test_method);

    ClassStmt test_class(Str("test_class"), class_methods);

    doc = pt.Print(test_class);
    std::cout << "Complete class:\n" << doc.str() << std::endl;

    
}

} // namespace runtime
} // namespace mc

int main() {
    std::cout << "x" << std::endl;

    //mc::runtime::test_object();
    //mc::runtime::test_register();
    //mc::runtime::test_dict();
    //mc::runtime::test_set();
    //mc::runtime::test_list();
    //mc::runtime::test_tuple();
    //mc::runtime::test_expression();
    //mc::runtime::test_convert();
    //mc::runtime::test_stmt();
    //mc::runtime::test_expr();
    //mc::runtime::test_op();

    //mc::runtime::test_rewriter();
    //mc::runtime::test_if_comparison_rewriter();
    //mc::runtime::test_class();

    //mc::runtime::test_class_rewrite();
    mc::runtime::test_pointer_mcvalue();
    mc::runtime::test_end_to_end_class_compilation();
    
    mc::runtime::test_container_compilation();
    mc::runtime::test_container_print();

    return 0;
}
