#include "ops.h"
#include "registry.h"

#include <stdexcept>

#include <iostream>

namespace mc {
namespace runtime {

OpRegEntry& OpRegEntry::RegisterOrGet(const std::string& name) {
    return OpRegistry::Global()->RegisterOrGet(name);
}

const Op& Op::Get(const std::string& name) {
    return OpRegEntry::RegisterOrGet(name).op();
}

#define REGISTER_OP(OpName) \
  ::mc::runtime::OpRegEntry::RegisterOrGet(OpName)

#define DEFINE_OP(OpName) \
  const Op& OpName() { \
    static const Op& op = Op::Get("ast."#OpName); \
    return op; \
  } \
  auto _unused_##OpName = REGISTER_OP("ast." #OpName)

#define REGISTER_MAKE_UNARY_OP(Node, Func)               \
  REGISTER_GLOBAL("ast." #Node).SetBody([](PrimExpr a) { \
    return (Func(a));                                    \
  })

#define REGISTER_MAKE_BINARY_OP(Node, Func)                          \
  REGISTER_GLOBAL("ast." #Node).SetBody([](PrimExpr a, PrimExpr b) { \
    return (Func(a, b));                                       \
  })

PrimExpr op_add(PrimExpr a, PrimExpr b) {
    return PrimAdd(a, b);
}
DEFINE_OP(add).set_num(2);

PrimExpr op_mul(PrimExpr a, PrimExpr b) {
    return PrimMul(a, b);
}
DEFINE_OP(mul).set_num(2);

PrimExpr op_sub(PrimExpr a, PrimExpr b) {
    return PrimSub(a, b);
}

PrimExpr op_div(PrimExpr a, PrimExpr b) {
    return PrimDiv(a, b);
}

PrimExpr op_mod(PrimExpr a, PrimExpr b) {
    return PrimMod(a, b);
}

PrimExpr op_logic_not(PrimExpr a) {
    return PrimNot(a);
}
DEFINE_OP(logic_not).set_num(1);

PrimExpr op_if_then_else(PrimExpr c, PrimExpr t, PrimExpr f) {
    if (c->datatype != DataType::Bool()) {
        throw std::runtime_error("if_then_else only accepts boolean condition");
    }

    if (c.get()->template IsType<IntImmNode>()) {
        auto* imm = static_cast<const IntImmNode*>(c.get());
        return imm->value != 0 ? t : f;
    }

    return PrimCall(t->datatype, 
                    if_then_else(),
                    {c,t,f});
}
DEFINE_OP(if_then_else).set_num(3);

PrimExpr op_eq(PrimExpr a, PrimExpr b) {
    return PrimEq(a, b);
}

PrimExpr op_ne(PrimExpr a, PrimExpr b) {
    return PrimNe(a, b);
}

PrimExpr op_lt(PrimExpr a, PrimExpr b) {
    return PrimLt(a, b);
}

PrimExpr op_le(PrimExpr a, PrimExpr b) {
    return PrimLe(a, b);
}

PrimExpr op_gt(PrimExpr a, PrimExpr b) {
    return PrimGt(a, b);
}

PrimExpr op_ge(PrimExpr a, PrimExpr b) {
    return PrimGe(a, b);
}

PrimExpr op_and(PrimExpr a, PrimExpr b) {
    return PrimAnd(a, b);
}

PrimExpr op_or(PrimExpr a, PrimExpr b) {
    return PrimOr(a, b);
}

REGISTER_MAKE_BINARY_OP(_OpAdd, op_add);
REGISTER_MAKE_BINARY_OP(_OpMul, op_mul);
REGISTER_MAKE_BINARY_OP(_OpSub, op_sub);
REGISTER_MAKE_BINARY_OP(_OpDiv, op_div);
REGISTER_MAKE_BINARY_OP(_OpMod, op_mod);
REGISTER_MAKE_BINARY_OP(_OpEq, op_eq);
REGISTER_MAKE_BINARY_OP(_OpNe, op_ne);
REGISTER_MAKE_BINARY_OP(_OpLt, op_lt);
REGISTER_MAKE_BINARY_OP(_OpLe, op_le);
REGISTER_MAKE_BINARY_OP(_OpGt, op_gt);
REGISTER_MAKE_BINARY_OP(_OpGe, op_ge);
REGISTER_MAKE_BINARY_OP(_OpAnd, op_and);
REGISTER_MAKE_BINARY_OP(_OpOr, op_or);

REGISTER_MAKE_UNARY_OP(_OpNot, op_logic_not);

REGISTER_GLOBAL("ast._OpIfThenElse")
    .SetBody([](PrimExpr c, PrimExpr t, PrimExpr f){
        return op_if_then_else(c, t, f);
});

} // namespace runtime

} // namespace mc
