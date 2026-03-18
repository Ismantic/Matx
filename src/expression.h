#pragma once

#include "object.h"
#include "datatype.h"
#include "array.h"
#include "str.h"

namespace mc {
namespace runtime {


#define DEFINE_NODE_CLASS(TypeName, ParentType, NodeType) \
  TypeName() noexcept = default;    \
  explicit TypeName(object_p<object_t> n) noexcept : ParentType(std::move(n)) {} \
  TypeName(const TypeName& other) noexcept = default; \
  TypeName(TypeName&& other) noexcept = default; \
  TypeName& operator=(const TypeName& other) noexcept = default;    \
  TypeName& operator=(TypeName&& other) noexcept = default; \
  const NodeType* operator->() const noexcept {  \
    return static_cast<const NodeType*>(get()); \
  } \
  NodeType* operator->() noexcept { \
    return static_cast<NodeType*>(get_mutable()); \
  } \
  using ContainerType = NodeType;


class BaseExprNode : public object_t {
public:
    static constexpr const std::string_view NAME = "BaseExpr";
    static constexpr const int32_t INDEX = TypeIndex::Dynamic;
    DEFINE_TYPEINDEX(BaseExprNode, object_t);
};

class BaseExpr : public object_r {
public:
    DEFINE_NODE_CLASS(BaseExpr, object_r, BaseExprNode);
};

class PrimExprNode : public BaseExprNode {
public:
    static constexpr const std::string_view NAME = "PrimExpr";
    static constexpr const int32_t INDEX = TypeIndex::Dynamic;
    DEFINE_TYPEINDEX(PrimExprNode, BaseExprNode);

    DataType datatype;
};

class PrimExpr : public BaseExpr {
public:
    DEFINE_NODE_CLASS(PrimExpr, BaseExpr, PrimExprNode);

    PrimExpr(int32_t value);
};

class AstExprNode : public BaseExprNode{
public:
    static constexpr const std::string_view NAME = "AstExpr";
    static constexpr const int32_t INDEX = TypeIndex::Dynamic;
    DEFINE_TYPEINDEX(AstExprNode, BaseExprNode);
};

class AstExpr : public BaseExpr {
public:
    DEFINE_NODE_CLASS(AstExpr, BaseExpr, AstExprNode);

};

class PrimVarNode : public PrimExprNode {
public:
    static constexpr const std::string_view NAME = "PrimVar";
    static constexpr const int32_t INDEX = TypeIndex::Dynamic;
    DEFINE_TYPEINDEX(PrimVarNode, PrimExprNode);

    std::string var_name; // TODO -> Str

    void VisitAttrs(AttrVisitor* v) override;
};

class Type;
class PrimVar : public PrimExpr {
public:
    DEFINE_NODE_CLASS(PrimVar, PrimExpr, PrimVarNode);

    explicit PrimVar(std::string name, DataType t);
    explicit PrimVar(std::string name, Type t);
};

class IntImmNode : public PrimExprNode {
public:
    static constexpr const std::string_view NAME = "IntImm";
    static constexpr const int32_t INDEX = TypeIndex::Dynamic;
    DEFINE_TYPEINDEX(IntImmNode, PrimExprNode);
    int64_t value;
};

class IntImm : public PrimExpr {
public:
    DEFINE_NODE_CLASS(IntImm, PrimExpr, IntImmNode);

    explicit IntImm(DataType datatype, int64_t value);
};

class FloatImmNode : public PrimExprNode {
public:
    static constexpr const std::string_view NAME = "FloatImm";
    static constexpr const int32_t INDEX = TypeIndex::Dynamic;
    DEFINE_TYPEINDEX(FloatImmNode, PrimExprNode);
    double value;
};

class FloatImm : public PrimExpr {
public:
    DEFINE_NODE_CLASS(FloatImm, PrimExpr, FloatImmNode);

    explicit FloatImm(DataType datatype, double value);
};

class NullImmNode : public PrimExprNode {
public:
    static constexpr const std::string_view NAME = "NullImm";
    static constexpr const int32_t INDEX = TypeIndex::Dynamic;
    DEFINE_TYPEINDEX(NullImmNode, PrimExprNode);
};

class NullImm : public PrimExpr {
public:
    DEFINE_NODE_CLASS(NullImm, PrimExpr, NullImmNode);
};

class Bool : public PrimExpr {
public:
    DEFINE_NODE_CLASS(Bool, PrimExpr, IntImmNode);

    explicit Bool(bool value);

    operator bool() const {
        if (const auto* node = dynamic_cast<const IntImmNode*>(get())) {
            return node->value != 0;
        }
        return false;
    }
};

template <typename T>
class PrimBinaryOpNode : public PrimExprNode {
public:
  static constexpr const int32_t INDEX = TypeIndex::Dynamic;
  DEFINE_TYPEINDEX(T, PrimExprNode);

  PrimExpr a;
  PrimExpr b;

  void VisitAttrs(AttrVisitor* v) override;
};

class PrimAddNode : public PrimBinaryOpNode<PrimAddNode> {
public:
    static constexpr const std::string_view NAME = "PrimAdd";
};

class PrimAdd : public PrimExpr {
public:
    DEFINE_NODE_CLASS(PrimAdd, PrimExpr, PrimAddNode);

    explicit PrimAdd(PrimExpr a, PrimExpr b);
};

class PrimMulNode : public PrimBinaryOpNode<PrimMulNode> {
public:
    static constexpr const std::string_view NAME = "PrimMul";
};

class PrimMul : public PrimExpr {
public:
    DEFINE_NODE_CLASS(PrimMul, PrimExpr, PrimMulNode);

    explicit PrimMul(PrimExpr a, PrimExpr b);
};

class PrimSubNode : public PrimBinaryOpNode<PrimSubNode> {
public:
    static constexpr const std::string_view NAME = "PrimSub";
};

class PrimSub : public PrimExpr {
public:
    DEFINE_NODE_CLASS(PrimSub, PrimExpr, PrimSubNode);

    explicit PrimSub(PrimExpr a, PrimExpr b);
};

class PrimDivNode : public PrimBinaryOpNode<PrimDivNode> {
public:
    static constexpr const std::string_view NAME = "PrimDiv";
};

class PrimDiv : public PrimExpr {
public:
    DEFINE_NODE_CLASS(PrimDiv, PrimExpr, PrimDivNode);

    explicit PrimDiv(PrimExpr a, PrimExpr b);
};

class PrimModNode : public PrimBinaryOpNode<PrimModNode> {
public:
    static constexpr const std::string_view NAME = "PrimMod";
};

class PrimMod : public PrimExpr {
public:
    DEFINE_NODE_CLASS(PrimMod, PrimExpr, PrimModNode);

    explicit PrimMod(PrimExpr a, PrimExpr b);
};

class PrimEqNode : public PrimBinaryOpNode<PrimEqNode> {
public:
    static constexpr const std::string_view NAME = "PrimEq";
};

class PrimEq : public PrimExpr {
public:
    DEFINE_NODE_CLASS(PrimEq, PrimExpr, PrimEqNode);

    explicit PrimEq(PrimExpr a, PrimExpr b);
};

class PrimNeNode : public PrimBinaryOpNode<PrimNeNode> {
public:
    static constexpr const std::string_view NAME = "PrimNe";
};

class PrimNe : public PrimExpr {
public:
    DEFINE_NODE_CLASS(PrimNe, PrimExpr, PrimNeNode);

    explicit PrimNe(PrimExpr a, PrimExpr b);
};

class PrimLtNode : public PrimBinaryOpNode<PrimLtNode> {
public:
    static constexpr const std::string_view NAME = "PrimLt";
};

class PrimLt : public PrimExpr {
public:
    DEFINE_NODE_CLASS(PrimLt, PrimExpr, PrimLtNode);

    explicit PrimLt(PrimExpr a, PrimExpr b);
};

class PrimLeNode : public PrimBinaryOpNode<PrimLeNode> {
public:
    static constexpr const std::string_view NAME = "PrimLe";
};

class PrimLe : public PrimExpr {
public:
    DEFINE_NODE_CLASS(PrimLe, PrimExpr, PrimLeNode);

    explicit PrimLe(PrimExpr a, PrimExpr b);
};

class PrimGtNode : public PrimBinaryOpNode<PrimGtNode> {
public:
    static constexpr const std::string_view NAME = "PrimGt";
};

class PrimGt : public PrimExpr {
public:
    DEFINE_NODE_CLASS(PrimGt, PrimExpr, PrimGtNode);

    explicit PrimGt(PrimExpr a, PrimExpr b);
};

class PrimGeNode : public PrimBinaryOpNode<PrimGeNode> {
public:
    static constexpr const std::string_view NAME = "PrimGe";
};

class PrimGe : public PrimExpr {
public:
    DEFINE_NODE_CLASS(PrimGe, PrimExpr, PrimGeNode);

    explicit PrimGe(PrimExpr a, PrimExpr b);
};

class PrimAndNode : public PrimBinaryOpNode<PrimAndNode> {
public:
    static constexpr const std::string_view NAME = "PrimAnd";
};

class PrimAnd : public PrimExpr {
public:
    DEFINE_NODE_CLASS(PrimAnd, PrimExpr, PrimAndNode);

    explicit PrimAnd(PrimExpr a, PrimExpr b);
};

class PrimOrNode : public PrimBinaryOpNode<PrimOrNode> {
public:
    static constexpr const std::string_view NAME = "PrimOr";
};

class PrimOr : public PrimExpr {
public:
    DEFINE_NODE_CLASS(PrimOr, PrimExpr, PrimOrNode);

    explicit PrimOr(PrimExpr a, PrimExpr b);
};

class PrimNotNode : public PrimExprNode {
public:
    static constexpr const std::string_view NAME = "PrimNot";
    static constexpr const int32_t INDEX = TypeIndex::Dynamic;
    DEFINE_TYPEINDEX(PrimNotNode, PrimExprNode);

    PrimExpr a;
};

class PrimNot : public PrimExpr {
public:
    DEFINE_NODE_CLASS(PrimNot, PrimExpr, PrimNotNode);

    explicit PrimNot(PrimExpr a);
};

class PrimCallNode : public PrimExprNode {
public:
    static constexpr const std::string_view NAME = "PrimCall";
    static constexpr const int32_t INDEX = TypeIndex::Dynamic;
    DEFINE_TYPEINDEX(PrimCallNode, PrimExprNode);

    BaseExpr op;
    Array<PrimExpr> gs;
};

class PrimCall : public PrimExpr {
public:
    DEFINE_NODE_CLASS(PrimCall, PrimExpr, PrimCallNode);

    explicit PrimCall(DataType datatype, BaseExpr op, Array<PrimExpr> gs);
};

class GlobalVarNode : public BaseExprNode {
public:
    static constexpr const std::string_view NAME = "GlobalVar";
    static constexpr const int32_t INDEX = TypeIndex::Dynamic;
    DEFINE_TYPEINDEX(GlobalVarNode, BaseExprNode);

    std::string var_name; // TODO -> Str

};

class GlobalVar : public BaseExpr {
public:
    DEFINE_NODE_CLASS(GlobalVar, BaseExpr, GlobalVarNode);

    explicit GlobalVar(std::string name);

    std::string var_name() const {
        return static_cast<const GlobalVarNode*>(get())->var_name;
    }
};

class StrImmNode : public PrimExprNode {
public:
    static constexpr const std::string_view NAME = "StrImm";
    static constexpr const int32_t INDEX = TypeIndex::Dynamic;
    DEFINE_TYPEINDEX(StrImmNode, PrimExprNode);

    Str value;

    void VisitAttrs(AttrVisitor* v) override;
};

class StrImm : public PrimExpr {
public:
    DEFINE_NODE_CLASS(StrImm, PrimExpr, StrImmNode);

    explicit StrImm(Str value);
    explicit StrImm(std::string value);
    explicit StrImm(const char* value);
};

class ClassGetItemNode : public PrimExprNode {
public:
    static constexpr const std::string_view NAME = "ClassGetItem";
    static constexpr const int32_t INDEX = TypeIndex::Dynamic;
    DEFINE_TYPEINDEX(ClassGetItemNode, PrimExprNode);

    BaseExpr object;
    StrImm item;

    void VisitAttrs(AttrVisitor* v) override;
};

class ClassGetItem : public PrimExpr {
public:
    DEFINE_NODE_CLASS(ClassGetItem, PrimExpr, ClassGetItemNode);

    explicit ClassGetItem(BaseExpr object, StrImm item);
};

// Container literal nodes
class ListLiteralNode : public PrimExprNode {
public:
    static constexpr const std::string_view NAME = "ListLiteral";
    static constexpr const int32_t INDEX = TypeIndex::Dynamic;
    DEFINE_TYPEINDEX(ListLiteralNode, PrimExprNode);

    Array<PrimExpr> elements;

    void VisitAttrs(AttrVisitor* v) override;
};

class ListLiteral : public PrimExpr {
public:
    DEFINE_NODE_CLASS(ListLiteral, PrimExpr, ListLiteralNode);

    explicit ListLiteral(Array<PrimExpr> elements);
};

class DictLiteralNode : public PrimExprNode {
public:
    static constexpr const std::string_view NAME = "DictLiteral";
    static constexpr const int32_t INDEX = TypeIndex::Dynamic;
    DEFINE_TYPEINDEX(DictLiteralNode, PrimExprNode);

    Array<PrimExpr> keys;
    Array<PrimExpr> values;

    void VisitAttrs(AttrVisitor* v) override;
};

class DictLiteral : public PrimExpr {
public:
    DEFINE_NODE_CLASS(DictLiteral, PrimExpr, DictLiteralNode);

    explicit DictLiteral(Array<PrimExpr> keys, Array<PrimExpr> values);
};

class SetLiteralNode : public PrimExprNode {
public:
    static constexpr const std::string_view NAME = "SetLiteral";
    static constexpr const int32_t INDEX = TypeIndex::Dynamic;
    DEFINE_TYPEINDEX(SetLiteralNode, PrimExprNode);

    Array<PrimExpr> elements;

    void VisitAttrs(AttrVisitor* v) override;
};

class SetLiteral : public PrimExpr {
public:
    DEFINE_NODE_CLASS(SetLiteral, PrimExpr, SetLiteralNode);

    explicit SetLiteral(Array<PrimExpr> elements);
};

// Container operation nodes
class ContainerGetItemNode : public PrimExprNode {
public:
    static constexpr const std::string_view NAME = "ContainerGetItem";
    static constexpr const int32_t INDEX = TypeIndex::Dynamic;
    DEFINE_TYPEINDEX(ContainerGetItemNode, PrimExprNode);

    BaseExpr object;
    PrimExpr index;

    void VisitAttrs(AttrVisitor* v) override;
};

class ContainerGetItem : public PrimExpr {
public:
    DEFINE_NODE_CLASS(ContainerGetItem, PrimExpr, ContainerGetItemNode);

    explicit ContainerGetItem(BaseExpr object, PrimExpr index);
};

class ContainerSetItemNode : public PrimExprNode {
public:
    static constexpr const std::string_view NAME = "ContainerSetItem";
    static constexpr const int32_t INDEX = TypeIndex::Dynamic;
    DEFINE_TYPEINDEX(ContainerSetItemNode, PrimExprNode);

    BaseExpr object;
    PrimExpr index;
    PrimExpr value;

    void VisitAttrs(AttrVisitor* v) override;
};

class ContainerSetItem : public PrimExpr {
public:
    DEFINE_NODE_CLASS(ContainerSetItem, PrimExpr, ContainerSetItemNode);

    explicit ContainerSetItem(BaseExpr object, PrimExpr index, PrimExpr value);
};

class ContainerMethodCallNode : public PrimExprNode {
public:
    static constexpr const std::string_view NAME = "ContainerMethodCall";
    static constexpr const int32_t INDEX = TypeIndex::Dynamic;
    DEFINE_TYPEINDEX(ContainerMethodCallNode, PrimExprNode);

    BaseExpr object;
    StrImm method;
    Array<PrimExpr> args;

    void VisitAttrs(AttrVisitor* v) override;
};

class ContainerMethodCall : public PrimExpr {
public:
    DEFINE_NODE_CLASS(ContainerMethodCall, PrimExpr, ContainerMethodCallNode);

    explicit ContainerMethodCall(BaseExpr object, StrImm method, Array<PrimExpr> args);
};

} // namespace runtime
} // namespace mc
