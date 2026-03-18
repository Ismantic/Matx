#include "function.h"
#include "registry.h"
#include "debug_log.h"


namespace mc {
namespace runtime {

DictAttrs::DictAttrs(Map<Str, Object> dict) {
    auto node = MakeObject<DictAttrsNode>();
    node->dict = std::move(dict);
    data_ = std::move(node);
}

REGISTER_TYPEINDEX(DictAttrsNode);

void IRModuleNode::VisitAttrs(AttrVisitor* v) {

}

IRModule::IRModule(Map<GlobalVar, BaseFunc> functions,
                   Map<GlobalTypeVar, ClassType> ts) {
    auto node = MakeObject<IRModuleNode>();
    node->func_ = std::move(functions);
    node->class_ = std::move(ts);
    node->vars_ = {};
    node->class_vars_ = {};

    for (const auto& p : node->func_) {
        node->vars_.emplace(p.first->var_name, p.first);
    }

    for (const auto& p : node->class_) {
        node->class_vars_.emplace(p.first->var_name, p.first);
    }

    data_ = std::move(node);
}

IRModule IRModule::From(const AstExpr& expr,
                        const Map<GlobalVar, BaseFunc>& funcs, 
                        const Map<GlobalTypeVar, ClassType>& clss) {
    auto m = IRModule(funcs, clss);
    BaseFunc func;
    std::string name = "main";

    if (auto* func_node = expr.As<BaseFuncNode>()) {
        func = RTcast<BaseFunc>(func_node);
        //auto value = func->GetAttr<Str>(Str("GlobalSymbol"))
    }

    auto main = GlobalVar(name);

    m->Insert(main, func);

    return m;
}

REGISTER_TYPEINDEX(IRModuleNode);
REGISTER_GLOBAL("ast.IRModule")
    .SetBody([](Map<GlobalVar, BaseFunc> funcs,
                Map<GlobalTypeVar, ClassType> clss){
        return IRModule(std::move(funcs), std::move(clss));
});

PrimType::PrimType(DataType datatype) {
    auto node = MakeObject<PrimTypeNode>();
    node->datatype = std::move(datatype);
    data_ = std::move(node);
}

REGISTER_TYPEINDEX(PrimTypeNode);
REGISTER_GLOBAL("ast.PrimType")
    .SetBody([](const std::string& type_str) {
        auto dt = StrToDt(type_str);
        return PrimType(DataType(dt));
});

TypeVar::TypeVar(std::string name) {
    auto node = MakeObject<TypeVarNode>();
    node->name = std::move(name);
    data_ = std::move(node);
}

REGISTER_TYPEINDEX(TypeVarNode);
REGISTER_GLOBAL("ast.TypeVar")
    .SetBody([](std::string name){
        return TypeVar(name);
});

GlobalTypeVar::GlobalTypeVar(std::string name) {
    auto node = MakeObject<GlobalTypeVarNode>();    
    node->var_name = std::move(name);
    data_ = std::move(node);
}

REGISTER_TYPEINDEX(GlobalTypeVarNode);
REGISTER_GLOBAL("ast.GlobalType")
    .SetBody([](std::string name){
        return GlobalTypeVar(name);
});

PrimFunc::PrimFunc(Str name,
                   Array<PrimVar> gs,
                   Array<PrimExpr> fs,
                   Stmt body, Type rt) {
    auto node = MakeObject<PrimFuncNode>();
    node->name = std::move(name);
    node->gs = std::move(gs);
    node->fs = std::move(fs);
    node->body = std::move(body);
    node->rt = std::move(rt);
    data_ = std::move(node);
}

PrimFunc::PrimFunc(std::string name,
                   Array<PrimVar> gs,
                   Array<PrimExpr> fs,
                   Stmt body, Type rt) 
    : PrimFunc(Str(std::move(name)), std::move(gs), std::move(fs), std::move(body), std::move(rt)) {
}

PrimFunc::PrimFunc(const char* name,
                   Array<PrimVar> gs,
                   Array<PrimExpr> fs,
                   Stmt body, Type rt) 
    : PrimFunc(Str(name), std::move(gs), std::move(fs), std::move(body), std::move(rt)) {
}

PrimFunc::PrimFunc(Array<PrimVar> gs,
                   Array<PrimExpr> fs,
                   Stmt body, Type rt) 
    : PrimFunc(Str(""), std::move(gs), std::move(fs), std::move(body), std::move(rt)) {
} 

REGISTER_TYPEINDEX(PrimFuncNode);
REGISTER_GLOBAL("ast.PrimFunc")
    .SetBody([](Array<PrimVar> gs, 
                Array<PrimExpr> fs,
                Stmt body, Type rt){
        MC_DLOG_STREAM(std::cout << "C++ PrimFunc" << "\n");
        return PrimFunc(std::move(gs),
                        std::move(fs),
                        std::move(body),
                        std::move(rt));
});


AstFunc::AstFunc(Str name,
                 Array<BaseExpr> gs, 
                 Array<BaseExpr> fs, 
                 Stmt body, Type rt,
                 Array<TypeVar> ts) {
    auto node = MakeObject<AstFuncNode>();
    node->name = std::move(name);
    node->gs = std::move(gs);
    node->fs = std::move(fs);
    node->body = std::move(body);
    node->rt = std::move(rt);
    node->ts = std::move(ts);
    data_ = std::move(node);
}

AstFunc::AstFunc(std::string name,
                 Array<BaseExpr> gs, 
                 Array<BaseExpr> fs, 
                 Stmt body, Type rt,
                 Array<TypeVar> ts) 
    : AstFunc(Str(std::move(name)), std::move(gs), std::move(fs), std::move(body), std::move(rt), std::move(ts)) {
}

AstFunc::AstFunc(const char* name,
                 Array<BaseExpr> gs, 
                 Array<BaseExpr> fs, 
                 Stmt body, Type rt,
                 Array<TypeVar> ts) 
    : AstFunc(Str(name), std::move(gs), std::move(fs), std::move(body), std::move(rt), std::move(ts)) {
}

AstFunc::AstFunc(Array<BaseExpr> gs, 
                 Array<BaseExpr> fs, 
                 Stmt body, Type rt,
                 Array<TypeVar> ts) 
    : AstFunc(Str(""), std::move(gs), std::move(fs), std::move(body), std::move(rt), std::move(ts)) {
}

REGISTER_TYPEINDEX(AstFuncNode);
REGISTER_GLOBAL("ast.AstFunc")
    .SetBody([](Array<BaseExpr> gs, 
                Array<BaseExpr> fs, 
                Stmt body, Type rt,
                Array<TypeVar> ts){
        return AstFunc(std::move(gs),
                       std::move(fs),
                       std::move(body),
                       std::move(rt),
                       std::move(ts));
});

DataType GetRuntimeDataType(const Type& t) {
    if (auto* n = t.As<PrimTypeNode>()) {
        return n->datatype;
    }
    return DataType::Handle();
}

bool IsRuntimeDataType(const Type& t) {
    if (auto* n = t.As<PrimTypeNode>()) {
        return true;
    } else {
        return false;
    }
}

REGISTER_GLOBAL("ast.BaseFuncCopy")
    .SetBody([](BaseFunc func) { return func; });

REGISTER_GLOBAL("ast.BaseFuncWithAttr")
    .SetBody([](BaseFunc func, Str n, Str v){
        return WithAttr(Downcast<PrimFunc>(
            std::move(func)), std::move(n), std::move(v));
});


} // namespace runtime

} // namespace mc
