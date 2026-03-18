#pragma once

#include "expression.h"

#include <string>
#include <unordered_map>
#include <mutex>

namespace mc {
namespace runtime {

class OpNode : public AstExprNode {
public:
    static constexpr const std::string_view NAME = "Op";
    static constexpr const int32_t INDEX = TypeIndex::Dynamic;
    DEFINE_TYPEINDEX(OpNode, AstExprNode);

    std::string op_name;
};

class Op : public AstExpr {
public:
    DEFINE_NODE_CLASS(Op, AstExpr, OpNode);

   static const Op& Get(const std::string& name);
};

class OpRegEntry {
public:
    static OpRegEntry& RegisterOrGet(const std::string& name);

    const Op& op() const {
        return op_;
    }

    OpRegEntry& set_num(int32_t n) {
        num_gs = n;
        return *this;
    }

private:
    std::string name;
    Op op_;
    int32_t num_gs{0};

    friend class OpRegistry;
};


class OpRegistry {
public:
    static OpRegistry* Global() {
        static OpRegistry instance;
        return &instance;
    }

    OpRegEntry& RegisterOrGet(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = registry_.find(name);
        if (it != registry_.end()) {
            return it->second;
        }
        OpRegEntry entry;
        entry.name = name;
        auto node = MakeObject<OpNode>();
        node->op_name = name;
        entry.op_ = Op(std::move(node));
        
        registry_[name] = entry;
        return registry_[name];
    }

private:
    OpRegistry() = default;
    std::mutex mutex_;
    std::unordered_map<std::string, OpRegEntry> registry_;
};

PrimExpr op_add(PrimExpr a, PrimExpr b);
const Op& add();

PrimExpr op_mul(PrimExpr a, PrimExpr b);
const Op& mul();

PrimExpr op_sub(PrimExpr a, PrimExpr b);
PrimExpr op_div(PrimExpr a, PrimExpr b);
PrimExpr op_mod(PrimExpr a, PrimExpr b);

PrimExpr op_logic_not(PrimExpr a);
const Op& logic_not();

PrimExpr op_if_then_else(PrimExpr c, PrimExpr t, PrimExpr f);
const Op& if_then_else();

PrimExpr op_eq(PrimExpr a, PrimExpr b);
PrimExpr op_ne(PrimExpr a, PrimExpr b);
PrimExpr op_lt(PrimExpr a, PrimExpr b);
PrimExpr op_le(PrimExpr a, PrimExpr b);
PrimExpr op_gt(PrimExpr a, PrimExpr b);
PrimExpr op_ge(PrimExpr a, PrimExpr b);
PrimExpr op_and(PrimExpr a, PrimExpr b);
PrimExpr op_or(PrimExpr a, PrimExpr b);

} // namespace runtime

} // namespace mc
