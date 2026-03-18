#pragma once

#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <set>
#include <optional>
#include <map>

#include "expression.h"
#include "function.h"
#include "visitor.h"

namespace mc {
namespace runtime {

using str = std::string;

class Rewriter : public PrimExprVisitor<void(const PrimExpr&, std::ostream&)>,
                 public StmtVisitor<void(const Stmt&, std::ostream&)>,
                 public TypeVisitor<void(const Type&, std::ostream&)> {
public:
    virtual ~Rewriter() = default;

    void Init() {}

    void ResetState() {
        name_dict_.clear();
        var_dict_.clear();
        m_vec_.clear();
        t_ = 0;
    }

    virtual str Done() {
        return stream_.str();
    }

    virtual void InsertFunction(const PrimFunc& func) {
        ResetState();

        // 对于容器函数，显式生成McValue返回类型
        if (auto prim_type = func->rt.As<PrimTypeNode>()) {
            if (prim_type->datatype.IsHandle()) {
                stream_ << "McValue";
            } else {
                PrintType(func->rt, stream_);
            }
        } else {
            PrintType(func->rt, stream_);
        }
        stream_ << " " << GetFuncName(func) << "(";
        
        for (size_t i = 0; i < func->gs.size(); ++i) {
            if (i > 0) stream_ << ", ";
            const auto& param = func->gs[i];
            AllocVarID(param.operator->()); 
            PrintDataType(param->datatype, stream_);
            stream_ << " " << GetVarID(param.operator->());
        }
        stream_ << ") {\n";

        int scope = BeginScope();
        PrintStmt(func->body, stream_);
        EndScope(scope);
        
        stream_ << "}\n\n";
    }

    virtual void InsertClass(const ClassStmt& ct) {
        ResetState();
        
        // 设置当前类上下文
        current_class_name_ = ct->name.c_str();
        
        EmitClassDefinition(ct);
        
        // 清除类上下文
        current_class_name_.reset();
    }

protected:
    void PrintIndent(std::ostream& os) {
        for (int i = 0; i < t_; ++i) {
            os << "  ";
        }
    }

    str AllocVarID(const PrimVarNode* v) {
        if (var_dict_.count(v)) {
            std::cerr << "Cannot Allocate var ID for repeated var: " << v->var_name;
            return "";
        }
        str n = v->var_name;        
        str i = GetUniqueName(n);
        var_dict_[v] = i;
        return i;
    }

    str GetVarID(const PrimVarNode* v) {
        auto it = var_dict_.find(v);
        if (it == var_dict_.end()) {
            std::cerr << "Cannot Get var ID for var : " << v->var_name;
            std::cerr << "\n";
            return "";
        }
        return it->second;
    }


    int BeginScope() {
        int s = m_vec_.size();
        m_vec_.push_back(true);
        t_ += 1;
        return s;
    }

    void EndScope(int s) {
        m_vec_[s] = false;
        t_ -= 1;
    }

    void PrintExpr(const PrimExpr& expr, std::ostream& os) {
        VisitExpr(expr, os);
    }

    void PrintStmt(const Stmt& stmt, std::ostream& os) {
        VisitStmt(stmt, os);
    }

    void VisitExpr_(const IntImmNode* op, std::ostream& os) override {
        os << op->value;
    }

    void VisitExpr_(const FloatImmNode* op, std::ostream& os) override {
        os << op->value;
    }

    void VisitExpr_(const NullImmNode* op, std::ostream& os) override {
        os << "nullptr";
    }

    void VisitExpr_(const PrimVarNode* op, std::ostream& os) override {
        os << GetVarID(op);
    }

    void VisitExpr_(const PrimCallNode* op, std::ostream& os) override {
        if (auto callee = op->op.As<PrimExprNode>()) {
            PrintExpr(PrimExpr(object_p<PrimExprNode>(const_cast<PrimExprNode*>(callee))), os);
        } else {
            os << "/* unsupported callee */";
        }
        os << "(";
        for (size_t i = 0; i < op->gs.size(); ++i) {
            if (i > 0) os << ", ";
            PrintExpr(op->gs[i], os);
        }
        os << ")";
    }

    void VisitExpr_(const PrimAddNode* op, std::ostream& os) override {
        os << "(";
        PrintExpr(op->a, os);
        os << " + ";
        PrintExpr(op->b, os);
        os << ")";
    }

    void VisitExpr_(const PrimMulNode* op, std::ostream& os) override {
        os << "(";
        PrintExpr(op->a, os);
        os << " * ";
        PrintExpr(op->b, os);
        os << ")";
    }

    void VisitExpr_(const PrimSubNode* op, std::ostream& os) override {
        os << "(";
        PrintExpr(op->a, os);
        os << " - ";
        PrintExpr(op->b, os);
        os << ")";
    }

    void VisitExpr_(const PrimDivNode* op, std::ostream& os) override {
        os << "(";
        PrintExpr(op->a, os);
        os << " / ";
        PrintExpr(op->b, os);
        os << ")";
    }

    void VisitExpr_(const PrimModNode* op, std::ostream& os) override {
        os << "(";
        PrintExpr(op->a, os);
        os << " % ";
        PrintExpr(op->b, os);
        os << ")";
    }
    void VisitExpr_(const PrimEqNode* op, std::ostream& os) override {
        os << "(";
        PrintExpr(op->a, os);
        os << " == ";
        PrintExpr(op->b, os);
        os << ")";
    }

    void VisitExpr_(const PrimNeNode* op, std::ostream& os) override {
        os << "(";
        PrintExpr(op->a, os);
        os << " != ";
        PrintExpr(op->b, os);
        os << ")";
    }

    void VisitExpr_(const PrimLtNode* op, std::ostream& os) override {
        os << "(";
        PrintExpr(op->a, os);
        os << " < ";
        PrintExpr(op->b, os);
        os << ")";
    }

    void VisitExpr_(const PrimLeNode* op, std::ostream& os) override {
        os << "(";
        PrintExpr(op->a, os);
        os << " <= ";
        PrintExpr(op->b, os);
        os << ")";
    }

    void VisitExpr_(const PrimGtNode* op, std::ostream& os) override {
        os << "(";
        PrintExpr(op->a, os);
        os << " > ";
        PrintExpr(op->b, os);
        os << ")";
    }

    void VisitExpr_(const PrimGeNode* op, std::ostream& os) override {
        os << "(";
        PrintExpr(op->a, os);
        os << " >= ";
        PrintExpr(op->b, os);
        os << ")";
    }

    void VisitExpr_(const PrimAndNode* op, std::ostream& os) override {
        os << "(";
        PrintExpr(op->a, os);
        os << " && ";
        PrintExpr(op->b, os);
        os << ")";
    }

    void VisitExpr_(const PrimOrNode* op, std::ostream& os) override {
        os << "(";
        PrintExpr(op->a, os);
        os << " || ";
        PrintExpr(op->b, os);
        os << ")";
    }

    void VisitExpr_(const PrimNotNode* op, std::ostream& os) override {
        os << "(!";
        PrintExpr(op->a, os);
        os << ")";
    }

    void VisitExpr_(const StrImmNode* op, std::ostream& os) override {
        os << "Str(\"" << op->value.c_str() << "\")";
    }

    void VisitExpr_(const ClassGetItemNode* op, std::ostream& os) override {
        if (auto prim_expr = op->object.As<PrimExprNode>()) {
            PrintExpr(PrimExpr(object_p<PrimExprNode>(const_cast<PrimExprNode*>(prim_expr))), os);
        } else {
            os << "/* non-prim expr */";
        }
        os << "->" << op->item->value.c_str();
    }

    // Container AST nodes code generation
    void VisitExpr_(const ListLiteralNode* op, std::ostream& os) override;
    void VisitExpr_(const DictLiteralNode* op, std::ostream& os) override;
    void VisitExpr_(const SetLiteralNode* op, std::ostream& os) override;
    void VisitExpr_(const ContainerGetItemNode* op, std::ostream& os) override;
    void VisitExpr_(const ContainerSetItemNode* op, std::ostream& os) override;
    void VisitExpr_(const ContainerMethodCallNode* op, std::ostream& os) override;

    void VisitStmt_(const AllocaVarStmtNode* op, std::ostream& os) override {
        if (auto var = op->var.As<PrimVarNode>()) {
            PrintIndent(os);
            str var_id = AllocVarID(var);
            
            // 对于容器类型，使用 auto 简化声明
            if (var->datatype.IsHandle()) {
                os << "auto " << var_id;
            } else {
                PrintType(PrimType(var->datatype), os);
                os << " " << var_id;
            }

            if (!op->init_value.none()) {
                os << " = ";
                PrintExpr(Downcast<PrimExpr>(op->init_value), os);
            }
            os << ";\n";
        }
    }

    void VisitStmt_(const AssignStmtNode* op, std::ostream& os) override {
        if (auto var = op->u.As<PrimVarNode>()) {
            PrintIndent(os);
            os << GetVarID(var) << " = ";
            PrintExpr(Downcast<PrimExpr>(op->v), os);
            os << ";\n";
        }
    }

    void VisitStmt_(const ReturnStmtNode* op, std::ostream& os) override {
        PrintIndent(os);
        os << "return ";
        if (!op->value.none()) {
            PrintExpr(Downcast<PrimExpr>(op->value), os);
        }
        os << ";\n";
    }

    void VisitStmt_(const EvaluateNode* op, std::ostream& os) override {
        PrintIndent(os);
        PrintExpr(op->value, os);
        os << ";\n";
    }

    void VisitStmt_(const SeqStmtNode* op, std::ostream& os) override {
        for (const Stmt& stmt : op->s) {
            PrintStmt(stmt, os);
        }
    }

    void VisitStmt_(const IfStmtNode* op, std::ostream& os) override {
        PrintIndent(os);
        os << "if (";
        PrintExpr(op->cond, os);
        os << ") {\n";
        int then_scope = BeginScope();
        PrintStmt(op->then_case, os);
        EndScope(then_scope);
        PrintIndent(os);
        os << "}";
        if (!op->else_case.none()) {
            os << " else {\n";
            int else_scope = BeginScope();
            PrintStmt(op->else_case, os);
            EndScope(else_scope);
            PrintIndent(os);
            os << "}";
        }
        os << "\n";
    }

    void VisitStmt_(const WhileStmtNode* op, std::ostream& os) override {
        PrintIndent(os);
        os << "while (";
        PrintExpr(op->cond, os);
        os << ") {\n";
        int body_scope = BeginScope();
        PrintStmt(op->body, os);
        EndScope(body_scope);
        PrintIndent(os);
        os << "}\n";
    }

    void VisitStmt_(const ClassStmtNode* op, std::ostream& os) override {
        // 基类只是占位符实现，实际的类定义生成在 EmitClassDefinition 中
        EmitClassDefinition(ClassStmt(object_p<ClassStmtNode>(const_cast<ClassStmtNode*>(op))));
    }

    void PrintType(const Type& type, std::ostream& os) {
        if (auto* prim_type = type.As<PrimTypeNode>()) {
            PrintDataType(prim_type->datatype, os);
        }
    }

    void PrintDataType(DataType dtype, std::ostream& os) {
        if (dtype.IsVoid()) {
            os << "void";
        } else if (dtype.IsInt()) {
            os << "int" << dtype.b() << "_t";
        } else if (dtype.IsFloat()) {
            if (dtype.b() == 32) {
                os << "float";
            } else {
                os << "double";
            }
        } else if (dtype.IsBool()) {
            os << "bool";
        } else if (dtype.IsHandle()) {
            os << "McValue";
        }
    }

protected:
    // 辅助方法：从属性获取字符串值
    std::optional<std::string> GetStringAttr(const PrimFunc& func, const std::string& key) {
        if (func->attrs.none()) return std::nullopt;
        
        for (const auto& p : func->attrs->dict) {
            if (p.first.c_str() == std::string(key)) {
                if (auto str_val = p.second.As<StrNode>()) {
                    return Downcast<Str>(p.second).c_str();
                }
            }
        }
        return std::nullopt;
    }
    
    str GetFuncName(const PrimFunc& func) {
        // 1. 优先使用 name 字段
        if (!func.name().none() && func.name().c_str()[0] != '\0') {
            return func.name().c_str();
        }
        
        // 2. 向后兼容：检查 GlobalSymbol 属性
        if (!func->attrs.none()) {
            for (const auto& p : func->attrs->dict) {
                if (p.first.c_str() == std::string("GlobalSymbol")) {
                    return Downcast<Str>(p.second).c_str();
                }
            }
        }
        
        // 3. 如果在类上下文中，尝试组合类名和方法名
        if (current_class_name_.has_value()) {
            if (auto method_name = GetStringAttr(func, "MethodName")) {
                return *current_class_name_ + "__" + *method_name;
            }
        }
        
        // 4. 最后才使用 fallback，并输出警告
        std::cerr << "Warning: Function missing name information, using fallback" << std::endl;
        return "test_func";  // fallback
    }

    std::ostringstream stream_;
    
    // 类上下文管理
    std::optional<std::string> current_class_name_;

    // 类定义生成的虚函数
    virtual void EmitClassDefinition(const ClassStmt& cls) {
        // 基类默认实现：只生成结构体定义
        stream_ << "struct " << cls->name.c_str() << "_Data {\n";
        stream_ << "    // TODO: 添加成员变量\n";
        stream_ << "};\n\n";
    }

private:
    str GetUniqueName(str p) {
        for (size_t i = 0; i < p.size(); ++i) {
            if (p[i] == '.') {
                p[i] = '_';
            }
        }

        auto it = name_dict_.find(p);
        if (it != name_dict_.end()) {
            int counter = it->second + 1;
            it->second = counter;
            return p + "_" + std::to_string(counter);
        }

        name_dict_[p] = 0;
        return p;
    }

    std::unordered_map<const BaseExprNode*, str> var_dict_;
    std::unordered_map<str, int> name_dict_;
    std::vector<bool> m_vec_;
    int t_{0};
};


class SourceRewriter : public Rewriter {
public:
    SourceRewriter() = default;

    void Init();

    void InsertFunction(const PrimFunc& fn);

    void InsertClass(const ClassStmt& cls);

    str Done();

private:
    void EmitHeaders();

    void EmitModuleContext();

    void BeginAnonymousNamespace();

    void EndAnonymousNamespace();

    void DefineCAPIFunction(const PrimFunc& fn);

    void EmitModuleRegistry();

    struct TypeInfo {
        const char* index_name;
        const char* type_name;
    };

    TypeInfo GetTypeInfo(DataType dt) {
        if (dt.IsInt()) {
            return {"TypeIndex::Int", "int"};
        } else if (dt.IsFloat()) {
            return {"TypeIndex::Float", "float"};
        } else if (dt.IsBool()) {
            return {"TypeIndex::Int", "int"};
        } else if (dt.IsVoid()) {
            return {"TypeIndex::Void", "void"};
        }
        return {"TypeIndex::Object", "pointer"};
    }

    void EmitArgumentTypeCheck(const PrimFunc& f, int scope);

    void EmitModuleInitFunction();

    // 类相关的方法
    void DefineClassCAPIFunctions(const ClassStmt& cls);
    void EmitClassDefinition(const ClassStmt& cls) override;
    void ExtractMembersFromMethod(const PrimFuncNode* func, std::map<std::string, std::string>& member_types);
    std::string ExtractMethodName(const PrimFuncNode* func);
    void GenerateMethodCAPI(const std::string& class_name, const std::string& method_name, const PrimFuncNode* func);

private:
    std::vector<str> func_names_;
    std::vector<str> class_names_;
};

} // namespace runtime
} // namespace mc
