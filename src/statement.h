#pragma once

#include "object.h"
#include "expression.h"
#include "array.h"
#include "str.h"

namespace mc {
namespace runtime {

class Type;

class StmtNode : public object_t {
public:
    static constexpr const std::string_view NAME = "Stmt";
    static constexpr const int32_t INDEX = TypeIndex::Dynamic;
    DEFINE_TYPEINDEX(StmtNode, object_t);
};

class Stmt : public object_r {
public:
    DEFINE_NODE_CLASS(Stmt, object_r, StmtNode);
};

class ExprStmtNode : public StmtNode {
public:
    static constexpr const std::string_view NAME = "ExprStmt";
    static constexpr const int32_t INDEX = TypeIndex::Dynamic;
    DEFINE_TYPEINDEX(ExprStmtNode, StmtNode);

    BaseExpr expr;
};

class ExprStmt : public Stmt {
public:
    DEFINE_NODE_CLASS(ExprStmt, Stmt, ExprStmtNode);

    explicit ExprStmt(BaseExpr expr);
};

class AllocaVarStmtNode : public StmtNode {
public:
    static constexpr const std::string_view NAME = "AllocaVarStmt";
    static constexpr const int32_t INDEX = TypeIndex::Dynamic;
    DEFINE_TYPEINDEX(AllocaVarStmtNode, StmtNode);

    //BaseExpr var;
    PrimVar var;
    BaseExpr init_value;

    void VisitAttrs(AttrVisitor* v) override;
};

class AllocaVarStmt : public Stmt {
public:
    DEFINE_NODE_CLASS(AllocaVarStmt, Stmt, AllocaVarStmtNode);

    explicit AllocaVarStmt(std::string name, DataType datatype, 
                           BaseExpr init_value = BaseExpr());
};

class AssignStmtNode : public StmtNode {
public:
    static constexpr const std::string_view NAME = "AssignStmt";
    static constexpr const int32_t INDEX = TypeIndex::Dynamic;
    DEFINE_TYPEINDEX(AssignStmtNode, StmtNode);

    BaseExpr u;
    BaseExpr v;
};

class AssignStmt : public Stmt {
public:
    DEFINE_NODE_CLASS(AssignStmt, Stmt, AssignStmtNode);

    explicit AssignStmt(BaseExpr u, BaseExpr v);
};

class ReturnStmtNode : public StmtNode {
public:
    static constexpr const std::string_view NAME = "ReturnStmt";
    static constexpr const int32_t INDEX = TypeIndex::Dynamic;
    DEFINE_TYPEINDEX(ReturnStmtNode, StmtNode);

    BaseExpr value;
};

class ReturnStmt : public Stmt {
public:
    DEFINE_NODE_CLASS(ReturnStmt, Stmt, ReturnStmtNode);

    explicit ReturnStmt(BaseExpr value);

    explicit ReturnStmt(int value) : ReturnStmt(PrimExpr(value)) {}
};

class EvaluateNode : public StmtNode {
public:
    static constexpr const std::string_view NAME = "Evaluate";
    static constexpr const int32_t INDEX = TypeIndex::Dynamic;
    DEFINE_TYPEINDEX(EvaluateNode, StmtNode);

    PrimExpr value;
};

class Evaluate : public Stmt {
public:
    DEFINE_NODE_CLASS(Evaluate, Stmt, EvaluateNode);

    explicit Evaluate(PrimExpr value);
};

class SeqStmtNode : public StmtNode {
public:
    static constexpr const std::string_view NAME = "SeqStmt";
    static constexpr const int32_t INDEX = TypeIndex::Dynamic;
    DEFINE_TYPEINDEX(SeqStmtNode, StmtNode);

    Array<Stmt> s;
};

class SeqStmt : public Stmt {
public:
    DEFINE_NODE_CLASS(SeqStmt, Stmt, SeqStmtNode);

    explicit SeqStmt(Array<Stmt> s);
};

class ClassStmtNode : public StmtNode {
public:
    static constexpr const std::string_view NAME = "ClassStmt";
    static constexpr const int32_t INDEX = TypeIndex::Dynamic;
    DEFINE_TYPEINDEX(ClassStmtNode, StmtNode);

    Str name;
    Array<BaseExpr> body;

    void VisitAttrs(AttrVisitor* v) override;
};

class ClassStmt : public Stmt {
public:
    DEFINE_NODE_CLASS(ClassStmt, Stmt, ClassStmtNode);

    explicit ClassStmt(Str name, Array<BaseExpr> body);
    explicit ClassStmt(std::string name, Array<BaseExpr> body);
};

class IfStmtNode : public StmtNode {
public:
    static constexpr const std::string_view NAME = "IfStmt";
    static constexpr const int32_t INDEX = TypeIndex::Dynamic;
    DEFINE_TYPEINDEX(IfStmtNode, StmtNode);

    PrimExpr cond;
    Stmt then_case;
    Stmt else_case;

    void VisitAttrs(AttrVisitor* v) override;
};

class IfStmt : public Stmt {
public:
    DEFINE_NODE_CLASS(IfStmt, Stmt, IfStmtNode);

    explicit IfStmt(PrimExpr cond, Stmt then_case, Stmt else_case);
};

class WhileStmtNode : public StmtNode {
public:
    static constexpr const std::string_view NAME = "WhileStmt";
    static constexpr const int32_t INDEX = TypeIndex::Dynamic;
    DEFINE_TYPEINDEX(WhileStmtNode, StmtNode);

    PrimExpr cond;
    Stmt body;

    void VisitAttrs(AttrVisitor* v) override;
};

class WhileStmt : public Stmt {
public:
    DEFINE_NODE_CLASS(WhileStmt, Stmt, WhileStmtNode);

    explicit WhileStmt(PrimExpr cond, Stmt body);
};

} // namespace runtime
} // namespace mc
