#include "statement.h"
#include "function.h"
#include "registry.h"
#include "visitor.h"

namespace mc {
namespace runtime {

ExprStmt::ExprStmt(BaseExpr expr) {
    auto node = MakeObject<ExprStmtNode>();
    node->expr = std::move(expr);
    data_ = std::move(node);
}

REGISTER_TYPEINDEX(ExprStmtNode);
REGISTER_GLOBAL("ast.ExprStmt")
    .SetBody([](BaseExpr expr){
        return ExprStmt(std::move(expr));
});

AllocaVarStmt::AllocaVarStmt(std::string name, DataType datatype, BaseExpr init_value) {
    auto node = MakeObject<AllocaVarStmtNode>();
    node->var = std::move(PrimVar(name, datatype));
    node->init_value = std::move(init_value);
    data_ = std::move(node);
}

void AllocaVarStmtNode::VisitAttrs(AttrVisitor* v) {
    v->Visit("var", &var);
    v->Visit("init_value", &init_value);
}

REGISTER_TYPEINDEX(AllocaVarStmtNode);
REGISTER_GLOBAL("ast.AllocaVarStmt")
   .SetBody([](std::string n, DataType t, BaseExpr e) {
        return AllocaVarStmt(n, t, e);
});

AssignStmt::AssignStmt(BaseExpr u, BaseExpr v) {
    auto node = MakeObject<AssignStmtNode>();
    node->u = std::move(u);
    node->v = std::move(v);
    data_ = std::move(node);
}

REGISTER_TYPEINDEX(AssignStmtNode);
REGISTER_GLOBAL("ast.AssignStmt")
    .SetBody([](BaseExpr u, BaseExpr v){
        return AssignStmt(u, v);
});

ReturnStmt::ReturnStmt(BaseExpr value) {
    auto node = MakeObject<ReturnStmtNode>();
    node->value = std::move(value);
    data_ = std::move(node);
}

REGISTER_TYPEINDEX(ReturnStmtNode);
REGISTER_GLOBAL("ast.ReturnStmt")
    .SetBody([](BaseExpr value){
        return ReturnStmt(value);
});

Evaluate::Evaluate(PrimExpr value) {
    auto node = MakeObject<EvaluateNode>();
    node->value = std::move(value);
    data_ = std::move(node);
}

REGISTER_TYPEINDEX(EvaluateNode);
REGISTER_GLOBAL("ast.Evaluate")
    .SetBody([](PrimExpr value){
        return Evaluate(value);
});

SeqStmt::SeqStmt(Array<Stmt> s) {
    auto node = MakeObject<SeqStmtNode>();
    node->s = std::move(s);
    data_ = std::move(node);
}

REGISTER_TYPEINDEX(SeqStmtNode);
REGISTER_GLOBAL("ast.SeqStmt")
    .SetBody([](Array<Stmt> s){
        return SeqStmt(std::move(s));
});

ClassStmt::ClassStmt(Str name, Array<BaseExpr> body) {
    auto node = MakeObject<ClassStmtNode>();
    node->name = std::move(name);
    node->body = std::move(body);
    data_ = std::move(node);
}

ClassStmt::ClassStmt(std::string name, Array<BaseExpr> body)
    : ClassStmt(Str(name), std::move(body)) {}

void ClassStmtNode::VisitAttrs(AttrVisitor* v) {
    v->Visit("name", &name);
    v->Visit("body", &body);
}

REGISTER_TYPEINDEX(ClassStmtNode);
REGISTER_GLOBAL("ast.ClassStmt")
    .SetBody([](Str name, Array<BaseExpr> body) {
        return ClassStmt(name, body);
});

IfStmt::IfStmt(PrimExpr cond, Stmt then_case, Stmt else_case) {
    auto node = MakeObject<IfStmtNode>();
    node->cond = std::move(cond);
    node->then_case = std::move(then_case);
    node->else_case = std::move(else_case);
    data_ = std::move(node);
}

void IfStmtNode::VisitAttrs(AttrVisitor* v) {
    v->Visit("cond", &cond);
    v->Visit("then_case", &then_case);
    v->Visit("else_case", &else_case);
}

REGISTER_TYPEINDEX(IfStmtNode);
REGISTER_GLOBAL("ast.IfStmt")
    .SetBody([](PrimExpr cond, Stmt then_case, Stmt else_case) {
        return IfStmt(std::move(cond), std::move(then_case), std::move(else_case));
});

WhileStmt::WhileStmt(PrimExpr cond, Stmt body) {
    auto node = MakeObject<WhileStmtNode>();
    node->cond = std::move(cond);
    node->body = std::move(body);
    data_ = std::move(node);
}

void WhileStmtNode::VisitAttrs(AttrVisitor* v) {
    v->Visit("cond", &cond);
    v->Visit("body", &body);
}

REGISTER_TYPEINDEX(WhileStmtNode);
REGISTER_GLOBAL("ast.WhileStmt")
    .SetBody([](PrimExpr cond, Stmt body) {
        return WhileStmt(std::move(cond), std::move(body));
});

} // namespace runtime
} // namespace mc
