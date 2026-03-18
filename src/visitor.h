#pragma once

#include <string_view>
#include <vector>
#include <iostream>

#include "object.h"
#include "expression.h"
#include "statement.h"
#include "function.h"

#include "runtime_value.h"

namespace mc {
namespace runtime {

template <typename F>
class NodeVisitor;

template <typename R, typename... Args>
class NodeVisitor<R(const object_r& n, Args...)> {
private:
    using FnPointer = R (*)(const object_r& n, Args...);
    using Self = NodeVisitor<R(const object_r& n, Args...)>;

    std::vector<FnPointer> func_;

public:
    using result_type = R;

    bool InDispatch(const object_r& n) const {
        uint32_t t = n->Index();
        return t <= func_.size() && func_[t] != nullptr;
    }

    template<typename Node>
    Self& SetDispatch(FnPointer fn) {
        uint32_t t = Node::RuntimeTypeIndex();
        if (func_.size() <= t) {
            func_.resize(t+1, nullptr);
        }
        if (func_[t] != nullptr) {
            std::cerr << "Dispatch for" << Node::NAME
                      << " error" << "\n";
            return *this;
        }
        func_[t] = fn;
        return *this;
    }

    R operator()(const object_r& n, Args... args) const {
        if (!InDispatch(n)) {
            std::cerr << "NodeVisitor " 
                      << n->Name() << "\n";
            return R();
        }
        return (*func_[n->Index()])(n, std::forward<Args>(args)...);
    }

};

template <typename F>
class PrimExprVisitor;

template <typename R, typename... Args>
class PrimExprVisitor<R(const PrimExpr& n, Args...)> {
private:
    using Self = PrimExprVisitor<R(const PrimExpr& n, Args...)>;
    using Fn = NodeVisitor<R(const object_r& n, Self*, Args...)>;

public:
    virtual ~PrimExprVisitor() = default;

    R operator()(const PrimExpr& n, Args... args) {
        return VisitExpr(n, std::forward<Args>(args)...);
    }

    virtual R VisitExpr(const PrimExpr& n, Args... args) {
        static Fn vt = InitVTable();
        return vt(n, this, std::forward<Args>(args)...);
    }

    virtual R VisitExprDefault_(const object_t* op, Args... args) {
        std::cerr << "Do not hava a default case for " << op->Name() << "\n";
        return R();
    }

    virtual R VisitExpr_(const IntImmNode* op, Args... args) { 
        return VisitExprDefault_(op, std::forward<Args>(args)...); 
    }
    virtual R VisitExpr_(const FloatImmNode* op, Args... args) {
        return VisitExprDefault_(op, std::forward<Args>(args)...);
    }
    virtual R VisitExpr_(const NullImmNode* op, Args... args) {
        return VisitExprDefault_(op, std::forward<Args>(args)...);
    }
    virtual R VisitExpr_(const PrimVarNode* op, Args... args) {
        return VisitExprDefault_(op, std::forward<Args>(args)...);
    }
    virtual R VisitExpr_(const PrimAddNode* op, Args... args) {
        return VisitExprDefault_(op, std::forward<Args>(args)...);
    }
    virtual R VisitExpr_(const PrimMulNode* op, Args... args) {
        return VisitExprDefault_(op, std::forward<Args>(args)...);
    }
    virtual R VisitExpr_(const PrimSubNode* op, Args... args) {
        return VisitExprDefault_(op, std::forward<Args>(args)...);
    }
    virtual R VisitExpr_(const PrimDivNode* op, Args... args) {
        return VisitExprDefault_(op, std::forward<Args>(args)...);
    }
    virtual R VisitExpr_(const PrimModNode* op, Args... args) {
        return VisitExprDefault_(op, std::forward<Args>(args)...);
    }
    virtual R VisitExpr_(const PrimEqNode* op, Args... args) {
        return VisitExprDefault_(op, std::forward<Args>(args)...);
    }
    virtual R VisitExpr_(const PrimNeNode* op, Args... args) {
        return VisitExprDefault_(op, std::forward<Args>(args)...);
    }
    virtual R VisitExpr_(const PrimLtNode* op, Args... args) {
        return VisitExprDefault_(op, std::forward<Args>(args)...);
    }
    virtual R VisitExpr_(const PrimLeNode* op, Args... args) {
        return VisitExprDefault_(op, std::forward<Args>(args)...);
    }
    virtual R VisitExpr_(const PrimGtNode* op, Args... args) {
        return VisitExprDefault_(op, std::forward<Args>(args)...);
    }
    virtual R VisitExpr_(const PrimGeNode* op, Args... args) {
        return VisitExprDefault_(op, std::forward<Args>(args)...);
    }
    virtual R VisitExpr_(const PrimAndNode* op, Args... args) {
        return VisitExprDefault_(op, std::forward<Args>(args)...);
    }
    virtual R VisitExpr_(const PrimOrNode* op, Args... args) {
        return VisitExprDefault_(op, std::forward<Args>(args)...);
    }
    virtual R VisitExpr_(const PrimNotNode* op, Args... args) {
        return VisitExprDefault_(op, std::forward<Args>(args)...);
    }
    virtual R VisitExpr_(const PrimCallNode* op, Args... args) {
        return VisitExprDefault_(op, std::forward<Args>(args)...);
    }
    virtual R VisitExpr_(const StrImmNode* op, Args... args) {
        return VisitExprDefault_(op, std::forward<Args>(args)...);
    }
    virtual R VisitExpr_(const ClassGetItemNode* op, Args... args) {
        return VisitExprDefault_(op, std::forward<Args>(args)...);
    }
    
    // Container AST nodes
    virtual R VisitExpr_(const ListLiteralNode* op, Args... args) {
        return VisitExprDefault_(op, std::forward<Args>(args)...);
    }
    virtual R VisitExpr_(const DictLiteralNode* op, Args... args) {
        return VisitExprDefault_(op, std::forward<Args>(args)...);
    }
    virtual R VisitExpr_(const SetLiteralNode* op, Args... args) {
        return VisitExprDefault_(op, std::forward<Args>(args)...);
    }
    virtual R VisitExpr_(const ContainerGetItemNode* op, Args... args) {
        return VisitExprDefault_(op, std::forward<Args>(args)...);
    }
    virtual R VisitExpr_(const ContainerSetItemNode* op, Args... args) {
        return VisitExprDefault_(op, std::forward<Args>(args)...);
    }
    virtual R VisitExpr_(const ContainerMethodCallNode* op, Args... args) {
        return VisitExprDefault_(op, std::forward<Args>(args)...);
    }

private:
    static Fn InitVTable() {
        Fn vt;
        vt.template SetDispatch<IntImmNode>(
            [](const object_r& n, Self* self, Args... args) {
                return self->VisitExpr_(static_cast<const IntImmNode*>(n.get()), 
                                      std::forward<Args>(args)...);
            });
        vt.template SetDispatch<FloatImmNode>(
            [](const object_r& n, Self* self, Args... args) {
                return self->VisitExpr_(static_cast<const FloatImmNode*>(n.get()),
                                      std::forward<Args>(args)...);
            });
        vt.template SetDispatch<NullImmNode>(
            [](const object_r& n, Self* self, Args... args) {
                return self->VisitExpr_(static_cast<const NullImmNode*>(n.get()),
                                      std::forward<Args>(args)...);
            });
        vt.template SetDispatch<PrimVarNode>(
            [](const object_r& n, Self* self, Args... args) {
                return self->VisitExpr_(static_cast<const PrimVarNode*>(n.get()),
                                      std::forward<Args>(args)...);
            });
        vt.template SetDispatch<PrimAddNode>(
            [](const object_r& n, Self* self, Args... args) {
                return self->VisitExpr_(static_cast<const PrimAddNode*>(n.get()),
                                      std::forward<Args>(args)...);
            });
        vt.template SetDispatch<PrimMulNode>(
            [](const object_r& n, Self* self, Args... args) {
                return self->VisitExpr_(static_cast<const PrimMulNode*>(n.get()),
                                      std::forward<Args>(args)...);
            });
        vt.template SetDispatch<PrimSubNode>(
            [](const object_r& n, Self* self, Args... args) {
                return self->VisitExpr_(static_cast<const PrimSubNode*>(n.get()),
                                      std::forward<Args>(args)...);
            });
        vt.template SetDispatch<PrimDivNode>(
            [](const object_r& n, Self* self, Args... args) {
                return self->VisitExpr_(static_cast<const PrimDivNode*>(n.get()),
                                      std::forward<Args>(args)...);
            });
        vt.template SetDispatch<PrimModNode>(
            [](const object_r& n, Self* self, Args... args) {
                return self->VisitExpr_(static_cast<const PrimModNode*>(n.get()),
                                      std::forward<Args>(args)...);
            });
        vt.template SetDispatch<PrimEqNode>(
            [](const object_r& n, Self* self, Args... args) {
                return self->VisitExpr_(static_cast<const PrimEqNode*>(n.get()),
                                      std::forward<Args>(args)...);
            });
        vt.template SetDispatch<PrimNeNode>(
            [](const object_r& n, Self* self, Args... args) {
                return self->VisitExpr_(static_cast<const PrimNeNode*>(n.get()),
                                      std::forward<Args>(args)...);
            });
        vt.template SetDispatch<PrimLtNode>(
            [](const object_r& n, Self* self, Args... args) {
                return self->VisitExpr_(static_cast<const PrimLtNode*>(n.get()),
                                      std::forward<Args>(args)...);
            });
        vt.template SetDispatch<PrimLeNode>(
            [](const object_r& n, Self* self, Args... args) {
                return self->VisitExpr_(static_cast<const PrimLeNode*>(n.get()),
                                      std::forward<Args>(args)...);
            });
        vt.template SetDispatch<PrimGtNode>(
            [](const object_r& n, Self* self, Args... args) {
                return self->VisitExpr_(static_cast<const PrimGtNode*>(n.get()),
                                      std::forward<Args>(args)...);
            });
        vt.template SetDispatch<PrimGeNode>(
            [](const object_r& n, Self* self, Args... args) {
                return self->VisitExpr_(static_cast<const PrimGeNode*>(n.get()),
                                      std::forward<Args>(args)...);
            });
        vt.template SetDispatch<PrimAndNode>(
            [](const object_r& n, Self* self, Args... args) {
                return self->VisitExpr_(static_cast<const PrimAndNode*>(n.get()),
                                      std::forward<Args>(args)...);
            });
        vt.template SetDispatch<PrimOrNode>(
            [](const object_r& n, Self* self, Args... args) {
                return self->VisitExpr_(static_cast<const PrimOrNode*>(n.get()),
                                      std::forward<Args>(args)...);
            });
        vt.template SetDispatch<PrimNotNode>(
            [](const object_r& n, Self* self, Args... args) {
                return self->VisitExpr_(static_cast<const PrimNotNode*>(n.get()),
                                      std::forward<Args>(args)...);
            });
        vt.template SetDispatch<PrimCallNode>(
            [](const object_r& n, Self* self, Args... args) {
                return self->VisitExpr_(static_cast<const PrimCallNode*>(n.get()),
                                      std::forward<Args>(args)...);
            });
        vt.template SetDispatch<StrImmNode>(
            [](const object_r& n, Self* self, Args... args) {
                return self->VisitExpr_(static_cast<const StrImmNode*>(n.get()),
                                      std::forward<Args>(args)...);
            });
        vt.template SetDispatch<ClassGetItemNode>(
            [](const object_r& n, Self* self, Args... args) {
                return self->VisitExpr_(static_cast<const ClassGetItemNode*>(n.get()),
                                      std::forward<Args>(args)...);
            });
        
        // Container AST nodes dispatch
        vt.template SetDispatch<ListLiteralNode>(
            [](const object_r& n, Self* self, Args... args) {
                return self->VisitExpr_(static_cast<const ListLiteralNode*>(n.get()),
                                      std::forward<Args>(args)...);
            });
        vt.template SetDispatch<DictLiteralNode>(
            [](const object_r& n, Self* self, Args... args) {
                return self->VisitExpr_(static_cast<const DictLiteralNode*>(n.get()),
                                      std::forward<Args>(args)...);
            });
        vt.template SetDispatch<SetLiteralNode>(
            [](const object_r& n, Self* self, Args... args) {
                return self->VisitExpr_(static_cast<const SetLiteralNode*>(n.get()),
                                      std::forward<Args>(args)...);
            });
        vt.template SetDispatch<ContainerGetItemNode>(
            [](const object_r& n, Self* self, Args... args) {
                return self->VisitExpr_(static_cast<const ContainerGetItemNode*>(n.get()),
                                      std::forward<Args>(args)...);
            });
        vt.template SetDispatch<ContainerSetItemNode>(
            [](const object_r& n, Self* self, Args... args) {
                return self->VisitExpr_(static_cast<const ContainerSetItemNode*>(n.get()),
                                      std::forward<Args>(args)...);
            });
        vt.template SetDispatch<ContainerMethodCallNode>(
            [](const object_r& n, Self* self, Args... args) {
                return self->VisitExpr_(static_cast<const ContainerMethodCallNode*>(n.get()),
                                      std::forward<Args>(args)...);
            });
 
        return vt;
    }
};

template <typename F>
class StmtVisitor;

template <typename R, typename... Args>
class StmtVisitor<R(const Stmt& n, Args...)> {
private:
    using Self = StmtVisitor<R(const Stmt& n, Args...)>;
    using Fn = NodeVisitor<R(const object_r& n, Self*, Args...)>;

public:
    virtual ~StmtVisitor() = default;

    R operator()(const Stmt& n, Args... args) {
        return VisitStmt(n, std::forward<Args>(args)...);
    }

    virtual R VisitStmt(const Stmt& n, Args... args) {
        static Fn vt = InitVTable();
        return vt(n, this, std::forward<Args>(args)...);
    }

    virtual R VisitStmtDefault_(const object_t* op, Args... args) {
        std::cerr << "Do not have a default case for " << op->Name() << "\n";
        return R();
    }

    virtual R VisitStmt_(const AllocaVarStmtNode* op, Args... args) {
        return VisitStmtDefault_(op, std::forward<Args>(args)...);
    }
    virtual R VisitStmt_(const AssignStmtNode* op, Args... args) {
        return VisitStmtDefault_(op, std::forward<Args>(args)...);
    }
    virtual R VisitStmt_(const ReturnStmtNode* op, Args... args) {
        return VisitStmtDefault_(op, std::forward<Args>(args)...);
    }
    virtual R VisitStmt_(const SeqStmtNode* op, Args... args) {
        return VisitStmtDefault_(op, std::forward<Args>(args)...);
    }
    virtual R VisitStmt_(const EvaluateNode* op, Args... args) {
        return VisitStmtDefault_(op, std::forward<Args>(args)...);
    }
    virtual R VisitStmt_(const ClassStmtNode* op, Args... args) {
        return VisitStmtDefault_(op, std::forward<Args>(args)...);
    }
    virtual R VisitStmt_(const IfStmtNode* op, Args... args) {
        return VisitStmtDefault_(op, std::forward<Args>(args)...);
    }
    virtual R VisitStmt_(const WhileStmtNode* op, Args... args) {
        return VisitStmtDefault_(op, std::forward<Args>(args)...);
    }

private:
    static Fn InitVTable() {
        Fn vt;
        vt.template SetDispatch<AllocaVarStmtNode>(
            [](const object_r& n, Self* self, Args... args) {
                return self->VisitStmt_(static_cast<const AllocaVarStmtNode*>(n.get()),
                                      std::forward<Args>(args)...);
            });
        vt.template SetDispatch<AssignStmtNode>(
            [](const object_r& n, Self* self, Args... args) {
                return self->VisitStmt_(static_cast<const AssignStmtNode*>(n.get()),
                                      std::forward<Args>(args)...);
            });
        vt.template SetDispatch<ReturnStmtNode>(
            [](const object_r& n, Self* self, Args... args) {
                return self->VisitStmt_(static_cast<const ReturnStmtNode*>(n.get()),
                                      std::forward<Args>(args)...);
            });
        vt.template SetDispatch<SeqStmtNode>(
            [](const object_r& n, Self* self, Args... args) {
                return self->VisitStmt_(static_cast<const SeqStmtNode*>(n.get()),
                                      std::forward<Args>(args)...);
            });
        vt.template SetDispatch<EvaluateNode>(
            [](const object_r& n, Self* self, Args... args) {
                return self->VisitStmt_(static_cast<const EvaluateNode*>(n.get()),
                                      std::forward<Args>(args)...);
            });
        vt.template SetDispatch<ClassStmtNode>(
            [](const object_r& n, Self* self, Args... args) {
                return self->VisitStmt_(static_cast<const ClassStmtNode*>(n.get()),
                                      std::forward<Args>(args)...);
            });
        vt.template SetDispatch<IfStmtNode>(
            [](const object_r& n, Self* self, Args... args) {
                return self->VisitStmt_(static_cast<const IfStmtNode*>(n.get()),
                                      std::forward<Args>(args)...);
            });
        vt.template SetDispatch<WhileStmtNode>(
            [](const object_r& n, Self* self, Args... args) {
                return self->VisitStmt_(static_cast<const WhileStmtNode*>(n.get()),
                                      std::forward<Args>(args)...);
            });
        return vt;
    }
};

template <typename F>
class TypeVisitor;

template <typename R, typename... Args>
class TypeVisitor<R(const Type& n, Args...)> {
private:
    using Self = TypeVisitor<R(const Type& n, Args...)>;
    using Fn = NodeVisitor<R(const object_r& n, Self*, Args...)>;

public:
    using result_type = R;

    virtual ~TypeVisitor() {}

    R operator()(const Type& n, Args... args) {
        return VisitType(n, std::forward<Args>(args)...);
    }

    virtual R VisitType(const Type& n, Args... args) {
        static Fn vt = InitTable();
        return vt(n, this, std::forward<Args>(args)...);
    }

    virtual R VisitTypeDefault_(const object_t* op, Args...) {
        std::cerr << "Do not have a default case for " << op->Name() << "\n";
        return R();
    }

    virtual R VisitType_(const PrimTypeNode* op, Args... args) {
        return VisitTypeDefault_(op, std::forward<Args>(args)...);
    }

private:
    static Fn InitTable() {
        Fn vt;
        vt.template SetDispatch<PrimTypeNode>(
            [](const object_r& n, Self* self, Args... args) {     
                return self->VisitType_(static_cast<const PrimTypeNode*>(n.get()), std::forward<Args>(args)...); \
        });
        return vt;
    }

};

class AttrVisitor {
public:
    virtual ~AttrVisitor() = default;
    virtual void Visit(std::string_view n, int64_t* v) = 0;
    virtual void Visit(std::string_view n, double* v) = 0;
    virtual void Visit(std::string_view n, std::string* v) = 0;
    virtual void Visit(std::string_view n, Object* v) = 0;
    virtual void Visit(std::string_view n, DataType* v) = 0;
};

class NodeAttrNameCollector : public AttrVisitor {
public:
    std::vector<std::string> names;

    void Visit(std::string_view n, int64_t*) override {
        names.push_back(std::string(n));
    }

    void Visit(std::string_view n, double*) override {
        names.push_back(std::string(n));
    }

    void Visit(std::string_view n, std::string*) override {
        names.push_back(std::string(n));
    }

    void Visit(std::string_view n, Object*) override {
        names.push_back(std::string(n));
    }

    void Visit(std::string_view n, DataType* v) override {
        names.push_back(std::string(n));
    }
};

class NodeAttrGetter : public AttrVisitor {
public:
    NodeAttrGetter(std::string_view n) : n_(n) {}

    McValue GetValue() const { return v_; }
    bool IsNone() const { return v_.T() != TypeIndex::Null; }

    void Visit(std::string_view n, int64_t* v) override {
        if (n == n_) v_ = McValue(*v);
    }

    void Visit(std::string_view n, double* v) override {
        if (n == n_) v_ = McValue(*v);
    }

    void Visit(std::string_view n, std::string* v) override {
        if (n == n_) v_ = McValue(*v);
    }

    void Visit(std::string_view n, Object* v) override {
        if (n == n_) v_ = McValue(*v);
    }

    void Visit(std::string_view n, DataType* v) override {
        if (n == n_) v_ = McValue(*v);
    }

private:
    std::string_view n_;
    McValue v_;
};

McValue NodeGetAttrNames(object_t* o);

McValue NodeGetAttr(object_t o, const std::string& n);


} // namespace runtime
} // namespace mc
