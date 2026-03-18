#include "expression.h"
#include "registry.h"
#include "function.h" // For GetRuntimeDataType
#include "visitor.h" // For AttrVisitor

namespace mc {
namespace runtime {

PrimExpr::PrimExpr(int32_t value) 
    : PrimExpr(IntImm(DataType::Int(32), value)) {}
    
REGISTER_TYPEINDEX(PrimExprNode);
REGISTER_GLOBAL("ast.PrimExpr")
    .SetBody([](int32_t v){
        return PrimExpr(v);
});

PrimVar::PrimVar(std::string name, DataType datatype) {
    auto node = MakeObject<PrimVarNode>();
    node->var_name = std::move(name);
    node->datatype = std::move(datatype);
    data_ = std::move(node);
}

PrimVar::PrimVar(std::string name, Type t) {
    auto node = MakeObject<PrimVarNode>();
    node->var_name = std::move(name);
    node->datatype = GetRuntimeDataType(t);
    data_ = std::move(node);
}

void PrimVarNode::VisitAttrs(AttrVisitor* v) {
    v->Visit("var_name", &var_name);
    v->Visit("datatype", &datatype);
}

REGISTER_TYPEINDEX(PrimVarNode);
REGISTER_GLOBAL("ast.PrimVar")
    .SetBody([](std::string name, McValue t){
        if (t.Is<Type>()) {
            return PrimVar(name, t.As<Type>());
        } else {
            return PrimVar(name, t.As<DataType>());
        }
});

IntImm::IntImm(DataType datatype, int64_t value) {
    auto node = MakeObject<IntImmNode>();
    node->datatype = std::move(datatype);
    node->value = value;
    data_ = std::move(node);
}

Bool::Bool(bool value) {
    auto node = MakeObject<IntImmNode>();
    node->value = value ? 1 : 0;
    node->datatype = DataType::Bool();
    data_ = std::move(node);
}

REGISTER_TYPEINDEX(IntImmNode);
REGISTER_GLOBAL("ast.Bool")
    .SetBody([](bool v){
        return Bool(v);
});

FloatImm::FloatImm(DataType datatype, double value) {
    auto node = MakeObject<FloatImmNode>();
    node->datatype = std::move(datatype);
    node->value = value;
    data_ = std::move(node);
}

REGISTER_TYPEINDEX(FloatImmNode);
REGISTER_GLOBAL("ast.FloatImm")
    .SetBody([](double v){
        return FloatImm(DataType::Float(64), v);
});

REGISTER_TYPEINDEX(NullImmNode);
REGISTER_GLOBAL("ast.NullImm")
    .SetBody([](){
        auto node = MakeObject<NullImmNode>();
        node->datatype = DataType::Handle();
        return NullImm(std::move(node));
});

PrimAdd::PrimAdd(PrimExpr a, PrimExpr b) {
    auto node = MakeObject<PrimAddNode>();
    node->a = std::move(a);
    node->b = std::move(b);
    node->datatype = node->a->datatype;
    data_ = std::move(node);
}

template <typename T>
void PrimBinaryOpNode<T>::VisitAttrs(AttrVisitor* v) {
    v->Visit("datatype", &(this->datatype));
    v->Visit("a", &a);
    v->Visit("b", &b);
}

REGISTER_TYPEINDEX(PrimAddNode);
REGISTER_GLOBAL("ast.PrimAdd")
    .SetBody([](PrimExpr a, PrimExpr b){
        return PrimAdd(std::move(a), std::move(b));
});

PrimMul::PrimMul(PrimExpr a, PrimExpr b) {
    auto node = MakeObject<PrimMulNode>();
    node->a = std::move(a);
    node->b = std::move(b);
    node->datatype = node->a->datatype;
    data_ = std::move(node);
}

REGISTER_TYPEINDEX(PrimMulNode);
REGISTER_GLOBAL("ast.PrimMul")
    .SetBody([](PrimExpr a, PrimExpr b){
        return PrimMul(std::move(a), std::move(b));
});

PrimSub::PrimSub(PrimExpr a, PrimExpr b) {
    auto node = MakeObject<PrimSubNode>();
    node->a = std::move(a);
    node->b = std::move(b);
    node->datatype = node->a->datatype;
    data_ = std::move(node);
}

REGISTER_TYPEINDEX(PrimSubNode);
REGISTER_GLOBAL("ast.PrimSub")
    .SetBody([](PrimExpr a, PrimExpr b){
        return PrimSub(std::move(a), std::move(b));
});

PrimDiv::PrimDiv(PrimExpr a, PrimExpr b) {
    auto node = MakeObject<PrimDivNode>();
    node->a = std::move(a);
    node->b = std::move(b);
    node->datatype = node->a->datatype;
    data_ = std::move(node);
}

REGISTER_TYPEINDEX(PrimDivNode);
REGISTER_GLOBAL("ast.PrimDiv")
    .SetBody([](PrimExpr a, PrimExpr b){
        return PrimDiv(std::move(a), std::move(b));
});

PrimMod::PrimMod(PrimExpr a, PrimExpr b) {
    auto node = MakeObject<PrimModNode>();
    node->a = std::move(a);
    node->b = std::move(b);
    node->datatype = node->a->datatype;
    data_ = std::move(node);
}

REGISTER_TYPEINDEX(PrimModNode);
REGISTER_GLOBAL("ast.PrimMod")
    .SetBody([](PrimExpr a, PrimExpr b){
        return PrimMod(std::move(a), std::move(b));
});

PrimEq::PrimEq(PrimExpr a, PrimExpr b) {
    auto node = MakeObject<PrimEqNode>();
    node->a = std::move(a);
    node->b = std::move(b);
    node->datatype = DataType::Bool();
    data_ = std::move(node);
}

REGISTER_TYPEINDEX(PrimEqNode);
REGISTER_GLOBAL("ast.PrimEq")
    .SetBody([](PrimExpr a, PrimExpr b){
        return PrimEq(std::move(a), std::move(b));
});

PrimNe::PrimNe(PrimExpr a, PrimExpr b) {
    auto node = MakeObject<PrimNeNode>();
    node->a = std::move(a);
    node->b = std::move(b);
    node->datatype = DataType::Bool();
    data_ = std::move(node);
}

REGISTER_TYPEINDEX(PrimNeNode);
REGISTER_GLOBAL("ast.PrimNe")
    .SetBody([](PrimExpr a, PrimExpr b){
        return PrimNe(std::move(a), std::move(b));
});

PrimLt::PrimLt(PrimExpr a, PrimExpr b) {
    auto node = MakeObject<PrimLtNode>();
    node->a = std::move(a);
    node->b = std::move(b);
    node->datatype = DataType::Bool();
    data_ = std::move(node);
}

REGISTER_TYPEINDEX(PrimLtNode);
REGISTER_GLOBAL("ast.PrimLt")
    .SetBody([](PrimExpr a, PrimExpr b){
        return PrimLt(std::move(a), std::move(b));
});

PrimLe::PrimLe(PrimExpr a, PrimExpr b) {
    auto node = MakeObject<PrimLeNode>();
    node->a = std::move(a);
    node->b = std::move(b);
    node->datatype = DataType::Bool();
    data_ = std::move(node);
}

REGISTER_TYPEINDEX(PrimLeNode);
REGISTER_GLOBAL("ast.PrimLe")
    .SetBody([](PrimExpr a, PrimExpr b){
        return PrimLe(std::move(a), std::move(b));
});

PrimGt::PrimGt(PrimExpr a, PrimExpr b) {
    auto node = MakeObject<PrimGtNode>();
    node->a = std::move(a);
    node->b = std::move(b);
    node->datatype = DataType::Bool();
    data_ = std::move(node);
}

REGISTER_TYPEINDEX(PrimGtNode);
REGISTER_GLOBAL("ast.PrimGt")
    .SetBody([](PrimExpr a, PrimExpr b){
        return PrimGt(std::move(a), std::move(b));
});

PrimGe::PrimGe(PrimExpr a, PrimExpr b) {
    auto node = MakeObject<PrimGeNode>();
    node->a = std::move(a);
    node->b = std::move(b);
    node->datatype = DataType::Bool();
    data_ = std::move(node);
}

REGISTER_TYPEINDEX(PrimGeNode);
REGISTER_GLOBAL("ast.PrimGe")
    .SetBody([](PrimExpr a, PrimExpr b){
        return PrimGe(std::move(a), std::move(b));
});

PrimAnd::PrimAnd(PrimExpr a, PrimExpr b) {
    auto node = MakeObject<PrimAndNode>();
    node->a = std::move(a);
    node->b = std::move(b);
    node->datatype = DataType::Bool();
    data_ = std::move(node);
}

REGISTER_TYPEINDEX(PrimAndNode);
REGISTER_GLOBAL("ast.PrimAnd")
    .SetBody([](PrimExpr a, PrimExpr b){
        return PrimAnd(std::move(a), std::move(b));
});

PrimOr::PrimOr(PrimExpr a, PrimExpr b) {
    auto node = MakeObject<PrimOrNode>();
    node->a = std::move(a);
    node->b = std::move(b);
    node->datatype = DataType::Bool();
    data_ = std::move(node);
}

REGISTER_TYPEINDEX(PrimOrNode);
REGISTER_GLOBAL("ast.PrimOr")
    .SetBody([](PrimExpr a, PrimExpr b){
        return PrimOr(std::move(a), std::move(b));
});

PrimNot::PrimNot(PrimExpr a) {
    auto node = MakeObject<PrimNotNode>();
    node->datatype = DataType::Bool(a->datatype.a());
    node->a = std::move(a);
    data_ = std::move(node);
}

REGISTER_TYPEINDEX(PrimNotNode);
REGISTER_GLOBAL("ast.PrimNot")
    .SetBody([](PrimExpr a){
        return PrimNot(std::move(a));
});

PrimCall::PrimCall(DataType datatype, BaseExpr op, Array<PrimExpr> gs) {
    auto node = MakeObject<PrimCallNode>();
    node->datatype = std::move(datatype);
    node->op = std::move(op);
    node->gs = std::move(gs);
    data_ = std::move(node);
}

REGISTER_TYPEINDEX(PrimCallNode);
REGISTER_GLOBAL("ast.PrimCall")
    .SetBody([](DataType datatype, BaseExpr op, Array<PrimExpr> gs){
        return PrimCall(datatype, op, gs);
});

REGISTER_GLOBAL("ast.PrimCallByType")
    .SetBody([](std::string dtype, BaseExpr op, Array<PrimExpr> gs) {
        DataType datatype;
        if (dtype == "int64" || dtype == "int") {
            datatype = DataType::Int(64);
        } else if (dtype == "float64" || dtype == "float") {
            datatype = DataType::Float(64);
        } else if (dtype == "bool") {
            datatype = DataType::Bool();
        } else if (dtype == "handle" || dtype == "None") {
            datatype = DataType::Handle();
        } else {
            throw std::runtime_error("Unsupported PrimCall dtype: " + dtype);
        }
        return PrimCall(datatype, op, gs);
    });

GlobalVar::GlobalVar(std::string name) {
    auto node = MakeObject<GlobalVarNode>();
    node->var_name = std::move(name);
    data_ = std::move(node);
}

REGISTER_TYPEINDEX(GlobalVarNode);
REGISTER_GLOBAL("ast.GlobalVar")
    .SetBody([](std::string name) {
        return GlobalVar(name);
});

StrImm::StrImm(Str value) {
    auto node = MakeObject<StrImmNode>();
    node->value = std::move(value);
    node->datatype = DataType::Handle(); // TODO
    data_ = std::move(node);
}

StrImm::StrImm(std::string value) : StrImm(Str(value)) {}

StrImm::StrImm(const char* value) : StrImm(Str(value)) {}

void StrImmNode::VisitAttrs(AttrVisitor* v) {
    v->Visit("datatype", &datatype);
    v->Visit("value", &value);
}

REGISTER_TYPEINDEX(StrImmNode);
REGISTER_GLOBAL("ast.StrImm")
    .SetBody([](Str value) {
        return StrImm(value);
});

ClassGetItem::ClassGetItem(BaseExpr object, StrImm item) {
    auto node = MakeObject<ClassGetItemNode>();
    node->object = std::move(object);
    node->item = std::move(item);
    data_ = std::move(node);
}

void ClassGetItemNode::VisitAttrs(AttrVisitor* v) {
    v->Visit("object", &object);
    v->Visit("item", &item);
}

REGISTER_TYPEINDEX(ClassGetItemNode);
REGISTER_GLOBAL("ast.ClassGetItem")
    .SetBody([](BaseExpr obj, StrImm attr) {
        return ClassGetItem(obj, attr);
});

// Container literal implementations
ListLiteral::ListLiteral(Array<PrimExpr> elements) {
    auto node = MakeObject<ListLiteralNode>();
    node->elements = std::move(elements);
    data_ = std::move(node);
}

void ListLiteralNode::VisitAttrs(AttrVisitor* v) {
    v->Visit("elements", &elements);
}

REGISTER_TYPEINDEX(ListLiteralNode);
REGISTER_GLOBAL("ast.ListLiteral")
    .SetBody([](Array<PrimExpr> elements) {
        return ListLiteral(elements);
});

DictLiteral::DictLiteral(Array<PrimExpr> keys, Array<PrimExpr> values) {
    auto node = MakeObject<DictLiteralNode>();
    node->keys = std::move(keys);
    node->values = std::move(values);
    data_ = std::move(node);
}

void DictLiteralNode::VisitAttrs(AttrVisitor* v) {
    v->Visit("keys", &keys);
    v->Visit("values", &values);
}

REGISTER_TYPEINDEX(DictLiteralNode);
REGISTER_GLOBAL("ast.DictLiteral")
    .SetBody([](Array<PrimExpr> keys, Array<PrimExpr> values) {
        return DictLiteral(keys, values);
});

SetLiteral::SetLiteral(Array<PrimExpr> elements) {
    auto node = MakeObject<SetLiteralNode>();
    node->elements = std::move(elements);
    data_ = std::move(node);
}

void SetLiteralNode::VisitAttrs(AttrVisitor* v) {
    v->Visit("elements", &elements);
}

REGISTER_TYPEINDEX(SetLiteralNode);
REGISTER_GLOBAL("ast.SetLiteral")
    .SetBody([](Array<PrimExpr> elements) {
        return SetLiteral(elements);
});

// Container operation implementations
ContainerGetItem::ContainerGetItem(BaseExpr object, PrimExpr index) {
    auto node = MakeObject<ContainerGetItemNode>();
    node->object = std::move(object);
    node->index = std::move(index);
    data_ = std::move(node);
}

void ContainerGetItemNode::VisitAttrs(AttrVisitor* v) {
    v->Visit("object", &object);
    v->Visit("index", &index);
}

REGISTER_TYPEINDEX(ContainerGetItemNode);
REGISTER_GLOBAL("ast.ContainerGetItem")
    .SetBody([](BaseExpr object, PrimExpr index) {
        return ContainerGetItem(object, index);
});

ContainerSetItem::ContainerSetItem(BaseExpr object, PrimExpr index, PrimExpr value) {
    auto node = MakeObject<ContainerSetItemNode>();
    node->object = std::move(object);
    node->index = std::move(index);
    node->value = std::move(value);
    data_ = std::move(node);
}

void ContainerSetItemNode::VisitAttrs(AttrVisitor* v) {
    v->Visit("object", &object);
    v->Visit("index", &index);
    v->Visit("value", &value);
}

REGISTER_TYPEINDEX(ContainerSetItemNode);
REGISTER_GLOBAL("ast.ContainerSetItem")
    .SetBody([](BaseExpr object, PrimExpr index, PrimExpr value) {
        return ContainerSetItem(object, index, value);
});

ContainerMethodCall::ContainerMethodCall(BaseExpr object, StrImm method, Array<PrimExpr> args) {
    auto node = MakeObject<ContainerMethodCallNode>();
    node->object = std::move(object);
    node->method = std::move(method);
    node->args = std::move(args);
    data_ = std::move(node);
}

void ContainerMethodCallNode::VisitAttrs(AttrVisitor* v) {
    v->Visit("object", &object);
    v->Visit("method", &method);
    v->Visit("args", &args);
}

REGISTER_TYPEINDEX(ContainerMethodCallNode);
REGISTER_GLOBAL("ast.ContainerMethodCall")
    .SetBody([](BaseExpr object, StrImm method, Array<PrimExpr> args) {
        return ContainerMethodCall(object, method, args);
});

} // namespace runtime
} // namespace mc
