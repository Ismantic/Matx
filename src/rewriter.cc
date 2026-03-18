#include "rewriter.h"

#include "registry.h"
#include "str.h"
#include <sstream>
#include <map>

namespace mc {
namespace runtime {

void SourceRewriter::Init() {
    Rewriter::Init();

    EmitHeaders();

    EmitModuleContext();

    BeginAnonymousNamespace();
}

void SourceRewriter::InsertFunction(const PrimFunc& f) {
    Rewriter::InsertFunction(f);
        
    DefineCAPIFunction(f);
        
    func_names_.push_back(GetFuncName(f));
}

void SourceRewriter::InsertClass(const ClassStmt& cls) {
    Rewriter::InsertClass(cls);
    
    // 为类的每个方法定义C API函数
    DefineClassCAPIFunctions(cls);
    
    // 记录类名
    class_names_.push_back(cls->name.c_str());
}

str SourceRewriter::Done() {
    EndAnonymousNamespace();

    EmitModuleRegistry();

    return Rewriter::Done();
}

void SourceRewriter::EmitHeaders() {
    const char* headers[] = {
            "<stdint.h>",
            "<string.h>",
            "<stdlib.h>",
            "<stdexcept>",
            "<stdio.h>",
            "\"runtime_value.h\"",
            "\"parameters.h\"",
            "\"registry.h\"",
            "\"c_api.h\"",
            "\"container.h\"",
            "\"str.h\""
    };
        
    for (const auto& header : headers) {
        stream_ << "#include " << header << "\n";
    }
    stream_ << "\nusing namespace mc::runtime;\n\n";
}

void SourceRewriter::EmitModuleContext() {
        // 模块版本信息 - 直接定义即可，不需要extern
        stream_ << "const char* __mc_module_version = \"1.0.0\";\n\n";
        
        // 模块上下文指针 - 直接定义
        stream_ << "void* __mc_module_ctx = nullptr;\n\n";
        
        // 错误信息缓冲区
        stream_ << "static thread_local char error_buffer[1024];\n\n";
}

    void SourceRewriter::EmitArgumentTypeCheck(const PrimFunc& f, int scope) {
        // 参数个数检查
        stream_ << "    if (num_args != " << f->gs.size() << ") {\n";
        stream_ << "        snprintf(error_buffer, sizeof(error_buffer),\n";
        stream_ << "                \"" << GetFuncName(f) << "() takes " << f->gs.size() 
                << " positional arguments but %d were given\",\n";
        stream_ << "                num_args);\n";
        stream_ << "        SetError(error_buffer);\n";
        stream_ << "        return -1;\n";
        stream_ << "    }\n\n";

        // 参数类型检查
        for (size_t i = 0; i < f->gs.size(); ++i) {
            auto type_info = GetTypeInfo(f->gs[i]->datatype);
            if (f->gs[i]->datatype.IsHandle()) {
                continue;
            }
            stream_ << "    if (args[" << i << "].t != " << type_info.index_name << ") {\n";
            stream_ << "        snprintf(error_buffer, sizeof(error_buffer),\n";
            stream_ << "                \"" << GetFuncName(f) << " argument " << i 
                    << " type mismatch, expect '" << type_info.type_name << "' type\");\n";
            stream_ << "        SetError(error_buffer);\n";
            stream_ << "        return -1;\n";
            stream_ << "    }\n";
        }
        stream_ << "\n";
    }

    void SourceRewriter::DefineCAPIFunction(const PrimFunc& f) {
        std::string func_name = GetFuncName(f);
        
        // C API 包装函数声明
        stream_ << "int " << func_name << "__c_api"
                << "(Value* args, int num_args, Value* ret_val, void* resource_handle) {\n";
        
        auto scope = BeginScope();

        // 生成参数检查
        EmitArgumentTypeCheck(f, scope);

        // 调用原始函数
        stream_ << "    auto result = " << func_name << "(";
        for (size_t i = 0; i < f->gs.size(); ++i) {
            if (i > 0) stream_ << ", ";
            auto dt = f->gs[i]->datatype;
            if (dt.IsHandle()) {
                stream_ << "McValue(McView(&args[" << i << "]))";
            } else {
                stream_ << "args[" << i << "].u.";
                if (dt.IsInt() || dt.IsBool()) {
                    stream_ << "v_int";
                } else if (dt.IsFloat()) {
                    stream_ << "v_float";
                }
            }
        }
        stream_ << ");\n\n";
        
        // 设置返回值
        auto ret_type = GetTypeInfo(f->rt.As<PrimTypeNode>()->datatype);
        stream_ << "    ret_val->t = " << ret_type.index_name << ";\n";
        if (!f->rt.As<PrimTypeNode>()->datatype.IsVoid()) {
            if (f->rt.As<PrimTypeNode>()->datatype.IsHandle()) {
                // 对于McValue类型，需要转换
                stream_ << "    result.AsValue(ret_val);\n";
            } else {
                stream_ << "    ret_val->u.v_" << ret_type.type_name << " = result;\n";
            }
        }
        stream_ << "    return 0;\n";

        EndScope(scope);
        stream_ << "}\n\n";
    }

    void SourceRewriter::EmitModuleRegistry() {
        stream_ << "extern \"C\" {\n\n";

        // 函数数组
        stream_ << "BackendFunc __mc_func_array__[] = {\n";
        for (const auto& name : func_names_) {
            stream_ << "    (BackendFunc)" << name << "__c_api,\n";
        }
        stream_ << "};\n\n";

        // 注册表
        stream_ << "FuncRegistry __mc_func_registry__ = {\n";
        // 构建函数名字符串 - 修正为支持多个函数
        std::string func_names_str = "\\" + std::to_string(func_names_.size());
        for (const auto& name : func_names_) {
            func_names_str += name + "\\000";
        }
        stream_ << "    \"" << func_names_str << "\",\n";
        stream_ << "    __mc_func_array__,\n";
        stream_ << "};\n\n";

        // 闭包名称表(当前未使用)
        stream_ << "const char* __mc_closures_names__ = \"0\\000\";  // 无闭包\n\n";

        // 模块初始化函数
        EmitModuleInitFunction();

        stream_ << "} // extern \"C\"\n";
    }

    void SourceRewriter::EmitModuleInitFunction() {
        // 构造函数 - 模块加载时调用
        stream_ << "__attribute__((constructor))\n";
        stream_ << "void init_module() {\n";
        stream_ << "    // 初始化模块资源(如果需要)\n";
        stream_ << "    const char* _matx_dbg = getenv(\"MATX_DEBUG_LOG\");\n";
        stream_ << "    if (_matx_dbg && _matx_dbg[0] != '\\0' && _matx_dbg[0] != '0') {\n";
        stream_ << "        printf(\"Module loaded, registry at: %p\\n\", &__mc_func_registry__);\n";
        stream_ << "    }\n";
        stream_ << "}\n\n";

        // 析构函数 - 模块卸载时调用
        stream_ << "__attribute__((destructor))\n";
        stream_ << "void cleanup_module() {\n";
        stream_ << "    if (__mc_module_ctx) {\n";
        stream_ << "        // 清理模块资源\n";
        stream_ << "        __mc_module_ctx = nullptr;\n";
        stream_ << "    }\n";
        stream_ << "}\n";
    }

void SourceRewriter::DefineClassCAPIFunctions(const ClassStmt& cls) {
    std::string class_name = cls->name.c_str();
    
    // 生成构造函数的C API包装
    stream_ << "int " << class_name << "__init____c_api"
            << "(Value* args, int num_args, Value* ret_val, void* resource_handle) {\n";
    stream_ << "    " << class_name << "_Data* obj = new " << class_name << "_Data();\n";
    stream_ << "    // 初始化成员变量\n";
    stream_ << "    if (num_args >= 1) {\n";
    stream_ << "        obj->value = args[0].u.v_int;  // 第一个参数是value\n";
    stream_ << "    } else {\n";
    stream_ << "        obj->value = 0;  // 默认值\n";
    stream_ << "    }\n";
    stream_ << "    ret_val->u.v_pointer = obj;\n";
    stream_ << "    ret_val->t = TypeIndex::Pointer;\n";
    stream_ << "    return 0;\n";
    stream_ << "}\n\n";
    
    // 生成析构函数的C API包装
    stream_ << "int " << class_name << "__del____c_api"
            << "(Value* args, int num_args, Value* ret_val, void* resource_handle) {\n";
    stream_ << "    " << class_name << "_Data* obj = static_cast<" << class_name << "_Data*>(args[0].u.v_pointer);\n";
    stream_ << "    delete obj;\n";
    stream_ << "    ret_val->t = TypeIndex::Null;\n";
    stream_ << "    return 0;\n";
    stream_ << "}\n\n";
    
    // 生成类中的其他方法
    for (const auto& method : cls->body) {
        if (auto func = method.As<PrimFuncNode>()) {
            std::string method_name = ExtractMethodName(func);
            if (method_name != "__init__" && method_name != "__del__") {
                GenerateMethodCAPI(class_name, method_name, func);
            }
        }
    }
    
    // 添加函数名到列表
    func_names_.push_back(class_name + "__init__");
    func_names_.push_back(class_name + "__del__");
    
    // 添加其他方法到函数列表
    for (const auto& method : cls->body) {
        if (auto func = method.As<PrimFuncNode>()) {
            std::string method_name = ExtractMethodName(func);
            if (method_name != "__init__" && method_name != "__del__") {
                func_names_.push_back(class_name + "__" + method_name);
            }
        }
    }
}

void SourceRewriter::EmitClassDefinition(const ClassStmt& cls) {
    // 生成数据结构定义
    stream_ << "struct " << cls->name.c_str() << "_Data {\n";
    
    // 从类体中的方法分析成员变量
    std::map<std::string, std::string> member_types;
    for (const auto& method : cls->body) {
        if (auto func = method.As<PrimFuncNode>()) {
            ExtractMembersFromMethod(func, member_types);
        }
    }
    
    // 生成成员变量
    if (member_types.empty()) {
        // 如果没有成员信息，生成空结构体并输出警告
        std::cerr << "Warning: Class " << cls->name.c_str() << " has no ClassMembers attribute, generating empty struct" << std::endl;
        stream_ << "    // 警告：无成员变量信息，请使用 ClassMembers 属性指定\n";
    } else {
        for (const auto& [name, type] : member_types) {
            if (type == "unknown") {
                stream_ << "    // " << name << " (类型未知，请在 ClassMembers 中指定)\n";
            } else {
                stream_ << "    " << type << " " << name << ";  // 来自 ClassMembers 属性\n";
            }
        }
    }
    
    stream_ << "};\n\n";
}

void SourceRewriter::ExtractMembersFromMethod(const PrimFuncNode* func, std::map<std::string, std::string>& member_types) {
    // 使用 ClassMembers 属性获取成员列表和类型信息
    // 格式：\"name1:type1,name2:type2\" 例如 \"value:int64_t,name:string\"
    PrimFunc prim_func(object_p<PrimFuncNode>(const_cast<PrimFuncNode*>(func)));
    
    if (auto class_members = GetStringAttr(prim_func, "ClassMembers")) {
        std::string members_str = *class_members;
        std::istringstream iss(members_str);
        std::string member_def;
        while (std::getline(iss, member_def, ',')) {
            // 去除前后空格
            size_t start = member_def.find_first_not_of(" \t");
            size_t end = member_def.find_last_not_of(" \t");
            if (start != std::string::npos && end != std::string::npos) {
                std::string clean_def = member_def.substr(start, end - start + 1);
                
                // 解析 name:type 格式
                size_t colon_pos = clean_def.find(':');
                if (colon_pos != std::string::npos) {
                    std::string name = clean_def.substr(0, colon_pos);
                    std::string type = clean_def.substr(colon_pos + 1);
                    member_types[name] = type;
                } else {
                    // 如果没有类型信息，输出警告
                    std::cerr << "Warning: Member " << clean_def << " missing type, use format 'name:type'" << std::endl;
                }
            }
        }
        return;
    }
    
    // Fallback: 简单的成员变量提取，但无法确定类型
    if (auto seq = func->body.As<SeqStmtNode>()) {
        for (const auto& stmt : seq->s) {
            if (auto assign = stmt.As<AssignStmtNode>()) {
                if (auto getitem = assign->u.As<ClassGetItemNode>()) {
                    std::string member_name = getitem->item->value.c_str();
                    member_types[member_name] = "unknown"; // 无法推断类型
                }
            }
        }
    }
}

std::string SourceRewriter::ExtractMethodName(const PrimFuncNode* func) {
    // 使用 MethodName 属性获取方法名
    PrimFunc prim_func(object_p<PrimFuncNode>(const_cast<PrimFuncNode*>(func)));
    
    if (auto method_name = GetStringAttr(prim_func, "MethodName")) {
        return *method_name;
    }
    
    // 如果没有 MethodName 属性，输出警告并返回占位符
    std::cerr << "Warning: Method missing MethodName attribute, using placeholder" << std::endl;
    return "unknown_method";
}

void SourceRewriter::GenerateMethodCAPI(const std::string& class_name, const std::string& method_name, const PrimFuncNode* func) {
    stream_ << "int " << class_name << "__" << method_name << "__c_api"
            << "(Value* args, int num_args, Value* ret_val, void* resource_handle) {\n";
    stream_ << "    " << class_name << "_Data* obj = static_cast<" << class_name << "_Data*>(args[0].u.v_pointer);\n";
    
    // 使用 MethodType 属性确定方法行为
    PrimFunc prim_func(object_p<PrimFuncNode>(const_cast<PrimFuncNode*>(func)));
    auto method_type = GetStringAttr(prim_func, "MethodType");
    
    if (method_type.has_value()) {
        if (*method_type == "getter") {
            // 获取器方法：返回成员变量值
            stream_ << "    ret_val->u.v_int = obj->" << method_name.substr(4) << ";  // 去掉 'get_' 前缀\n";
            stream_ << "    ret_val->t = TypeIndex::Int;\n";
        } else if (*method_type == "setter") {
            // 设置器方法：设置成员变量值
            stream_ << "    obj->" << method_name.substr(4) << " = args[1].u.v_int;  // 去掉 'set_' 前缀\n";
            stream_ << "    ret_val->t = TypeIndex::Void;\n";
        } else if (*method_type == "action") {
            // 动作方法：执行某种操作
            stream_ << "    // 执行 " << method_name << " 动作\n";
            stream_ << "    ret_val->t = TypeIndex::Void;\n";
        } else {
            // 其他类型的方法
            stream_ << "    // " << *method_type << " 类型方法: " << method_name << "\n";
            stream_ << "    ret_val->t = TypeIndex::Object;\n";
        }
    } else {
        // Fallback: 基于方法名推断行为
        if (method_name == "get_value") {
            stream_ << "    ret_val->u.v_int = obj->value;\n";
            stream_ << "    ret_val->t = TypeIndex::Int;\n";
        } else {
            stream_ << "    // TODO: 实现 " << method_name << " 方法\n";
            stream_ << "    ret_val->t = TypeIndex::Object;\n";
        }
    }
    
    stream_ << "    return 0;\n";
    stream_ << "}\n\n";
}

void SourceRewriter::BeginAnonymousNamespace() {
    stream_ << "namespace {\n\n";
}

void SourceRewriter::EndAnonymousNamespace() {
    stream_ << "\n} // namespace\n\n";
}

Str GetFuncSource(PrimFunc fn, Str name = "__main__") {
    auto func = WithAttr(std::move(fn), "Global", name);
    SourceRewriter s;
    s.Init();
    s.InsertFunction(func);
    auto c = s.Done();
    return Str(c);
}

REGISTER_GLOBAL("rewriter.BuildFunction").SetBody(GetFuncSource);

Str GetFuncsSource(Array<PrimFunc> funcs, Str name = "__main__") {
    SourceRewriter s;
    s.Init();
    for (const auto& fn : funcs) {
        s.InsertFunction(fn);
    }
    auto c = s.Done();
    return Str(c);
}

REGISTER_GLOBAL("rewriter.BuildFunctions").SetBody(GetFuncsSource);

Str GetClassSource(ClassStmt cls, Str name = "__main_class__") {
    SourceRewriter s;
    s.Init();
    s.InsertClass(cls);
    auto c = s.Done();
    return Str(c);
}

REGISTER_GLOBAL("rewriter.BuildClass").SetBody(GetClassSource);

// Container AST nodes implementation
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

void Rewriter::VisitExpr_(const DictLiteralNode* op, std::ostream& os) {
    os << "mc::runtime::Dict{";
    for (size_t i = 0; i < op->keys.size(); ++i) {
        if (i > 0) os << ", ";
        os << "{McValue(";
        PrintExpr(op->keys[i], os);
        os << "), McValue(";
        PrintExpr(op->values[i], os);
        os << ")}";
    }
    os << "}";
}

void Rewriter::VisitExpr_(const SetLiteralNode* op, std::ostream& os) {
    os << "mc::runtime::Set{";
    for (size_t i = 0; i < op->elements.size(); ++i) {
        if (i > 0) os << ", ";
        os << "McValue(";
        PrintExpr(op->elements[i], os);
        os << ")";
    }
    os << "}";
}

void Rewriter::VisitExpr_(const ContainerGetItemNode* op, std::ostream& os) {
    bool need_cast = !op->datatype.IsHandle();
    if (need_cast) {
        os << "(";
    }
    if (auto prim_expr = op->object.As<PrimExprNode>()) {
        PrintExpr(PrimExpr(object_p<PrimExprNode>(const_cast<PrimExprNode*>(prim_expr))), os);
    } else {
        os << "/* non-prim expr */";
    }
    os << "[";
    PrintExpr(op->index, os);
    os << "]";
    if (need_cast) {
        if (op->datatype.IsInt() || op->datatype.IsBool()) {
            os << ").As<int64_t>()";
        } else if (op->datatype.IsFloat()) {
            os << ").As<double>()";
        } else {
            os << ")";
        }
    }
}

void Rewriter::VisitExpr_(const ContainerSetItemNode* op, std::ostream& os) {
    // This is used in assignment context, generate the assignment
    if (auto prim_expr = op->object.As<PrimExprNode>()) {
        PrintExpr(PrimExpr(object_p<PrimExprNode>(const_cast<PrimExprNode*>(prim_expr))), os);
    } else {
        os << "/* non-prim expr */";
    }
    os << "[";
    PrintExpr(op->index, os);
    os << "] = McValue(";
    PrintExpr(op->value, os);
    os << ")";
}

void Rewriter::VisitExpr_(const ContainerMethodCallNode* op, std::ostream& os) {
    if (auto prim_expr = op->object.As<PrimExprNode>()) {
        PrintExpr(PrimExpr(object_p<PrimExprNode>(const_cast<PrimExprNode*>(prim_expr))), os);
    } else {
        os << "/* non-prim expr */";
    }
    
    // Map Python method names to C++ method names
    std::string method_name = op->method->value.c_str();
    if (method_name == "append") {
        os << ".append(";
    } else if (method_name == "insert") {
        os << ".insert(";
    } else if (method_name == "remove") {
        os << ".erase(";
    } else if (method_name == "clear") {
        os << ".clear(";
    } else if (method_name == "add") {
        os << ".insert(";  // Set.add -> Set.insert
    } else if (method_name == "discard") {
        os << ".erase(";   // Set.discard -> Set.erase
    } else if (method_name == "get") {
        // Dict.get(key, default) needs special handling
        os << ".contains(McValue(";
        if (op->args.size() > 0) {
            PrintExpr(op->args[0], os);
        }
        os << ")) ? ";
        if (auto prim_expr2 = op->object.As<PrimExprNode>()) {
            PrintExpr(PrimExpr(object_p<PrimExprNode>(const_cast<PrimExprNode*>(prim_expr2))), os);
        }
        os << "[McValue(";
        if (op->args.size() > 0) {
            PrintExpr(op->args[0], os);
        }
        os << ")] : McValue(";
        if (op->args.size() > 1) {
            PrintExpr(op->args[1], os);
        } else {
            os << "nullptr";
        }
        os << ")";
        return; // Special case, don't add closing parenthesis
    } else {
        // Default: use the method name as-is
        os << "." << method_name << "(";
    }
    
    // Add arguments
    for (size_t i = 0; i < op->args.size(); ++i) {
        if (i > 0) os << ", ";
        os << "McValue(";
        PrintExpr(op->args[i], os);
        os << ")";
    }
    os << ")";
}

} // namespace runtime
} // namespace mc
