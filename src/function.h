#pragma once 

#include <unordered_map>
#include <iostream>

#include "object.h"
#include "datatype.h"
#include "expression.h"
#include "statement.h"
#include "array.h"
#include "map.h"
#include "str.h"

// Type, Function, Module

namespace mc {
namespace runtime {

class DictAttrsNode : public object_t {
public:
    static constexpr const std::string_view NAME = "DictAttrs";
    static constexpr const int32_t INDEX = TypeIndex::Dynamic;
    DEFINE_TYPEINDEX(DictAttrsNode, object_t);

    Map<Str, Object> dict;

    DictAttrsNode() : dict(Map<Str,Object>{}) {}
};

class DictAttrs : public object_r {
public:
    DEFINE_NODE_CLASS(DictAttrs, object_r, DictAttrsNode);
    DEFINE_COW_METHOD(DictAttrsNode);

    explicit DictAttrs(Map<Str, Object> dict);
};

class TypeNode : public object_t {
public:
    static constexpr const std::string_view NAME = "Type";
    static constexpr const int32_t INDEX = TypeIndex::Dynamic;
    DEFINE_TYPEINDEX(TypeNode, object_t);
};

class Type : public object_r {
public:
    DEFINE_NODE_CLASS(Type, object_r, TypeNode);
};


class PrimTypeNode : public TypeNode {
public:
    static constexpr const std::string_view NAME = "PrimType";
    static constexpr const int32_t INDEX = TypeIndex::Dynamic;
    DEFINE_TYPEINDEX(PrimTypeNode, TypeNode);

    DataType datatype;
};

class PrimType : public Type {
public:
    DEFINE_NODE_CLASS(PrimType, Type, PrimTypeNode);

    explicit PrimType(DataType datatype);
};

class ClassTypeNode : public TypeNode {
public:
    static constexpr const std::string_view NAME = "ClassType";
    static constexpr const int32_t INDEX = TypeIndex::Dynamic;
    DEFINE_TYPEINDEX(ClassTypeNode, TypeNode);

};

class ClassType : public Type {
public:
    DEFINE_NODE_CLASS(ClassType, Type, ClassTypeNode);

};

class TypeVarNode : public TypeNode {
public:
    static constexpr const std::string_view NAME = "TypeVar";
    static constexpr const int32_t INDEX = TypeIndex::Dynamic;
    DEFINE_TYPEINDEX(TypeVarNode, TypeNode);

    std::string name;
};

class TypeVar : public Type {
public:
    DEFINE_NODE_CLASS(TypeVar, Type, TypeVarNode);

    explicit TypeVar(std::string name);
};

class GlobalTypeVarNode : public TypeNode {
public:
    static constexpr const std::string_view NAME = "GlobalTypeVar";
    static constexpr const int32_t INDEX = TypeIndex::Dynamic;
    DEFINE_TYPEINDEX(GlobalTypeVarNode, TypeNode);

    std::string var_name;
};

class GlobalTypeVar : public Type {
public:
    DEFINE_NODE_CLASS(GlobalTypeVar, Type, GlobalTypeVarNode);

    explicit GlobalTypeVar(std::string name);
};

class BaseFuncNode : public AstExprNode {
public:
    static constexpr const std::string_view NAME = "BaseFunc";
    static constexpr const int32_t INDEX = TypeIndex::Dynamic;
    DEFINE_TYPEINDEX(BaseFuncNode, AstExprNode);

    Str name;
    DictAttrs attrs;

    template <typename T>
    T GetAttr(const Str& attr_key,
              const T& default_value = T()) const {
        static_assert(std::is_base_of<object_r, T>::value,
                     "Can only call GetAttr with object_r types.");
        
        if (attrs.get() == nullptr) {
            return default_value;
        }
        
        auto it = attrs->dict.find(attr_key);
        if (it != attrs->dict.end()) {
            auto pair = *it;  
            if (auto* value = Downcast<T>(pair.second)) {
                return *value;
            }   
        } 
        
        return default_value;
    }
};

class BaseFunc : public AstExpr {
public:
    DEFINE_NODE_CLASS(BaseFunc, AstExpr, BaseFuncNode);
};

template <typename TFunc,
          typename = typename std::enable_if<std::is_base_of<BaseFunc, TFunc>::value>::type>
inline TFunc WithAttr(TFunc func, Str attr_key, object_r attr_value) {
    using TNode = typename TFunc::ContainerType;
    
    TNode* node = func.CopyOnWrite();
    
    if (node->attrs.none()) {
        Map<Str, Object> dict;
        dict.set(attr_key, attr_value);
        node->attrs = DictAttrs(std::move(dict));
    } else {
        node->attrs->dict.set(attr_key, attr_value);
    }

    return func;
}


class PrimFuncNode : public BaseFuncNode {
public:
    static constexpr const std::string_view NAME = "PrimFunc";
    static constexpr const int32_t INDEX = TypeIndex::Dynamic;
    DEFINE_TYPEINDEX(PrimFuncNode, BaseFuncNode);

    Array<PrimVar> gs; // Arguments gs
    Array<PrimExpr> fs; // Default Arguments fs
    Stmt body;
    Type rt;
};

class PrimFunc : public BaseFunc {
public:
    DEFINE_NODE_CLASS(PrimFunc, BaseFunc, PrimFuncNode);
    DEFINE_COW_METHOD(PrimFuncNode);

    explicit PrimFunc(Str name,
                     Array<PrimVar> gs,
                     Array<PrimExpr> fs,
                     Stmt body,
                     Type rt);

    explicit PrimFunc(std::string name,
                     Array<PrimVar> gs,
                     Array<PrimExpr> fs,
                     Stmt body,
                     Type rt);

    explicit PrimFunc(const char* name,
                     Array<PrimVar> gs,
                     Array<PrimExpr> fs,
                     Stmt body,
                     Type rt);

    explicit PrimFunc(Array<PrimVar> gs,
                     Array<PrimExpr> fs,
                     Stmt body,
                     Type rt);

    Str name() const { return static_cast<const BaseFuncNode*>(get())->name; }
};

class AstFuncNode : public BaseFuncNode {
public:
    static constexpr const std::string_view NAME = "AstFunc";
    static constexpr const int32_t INDEX = TypeIndex::Dynamic;
    DEFINE_TYPEINDEX(AstFuncNode, BaseFuncNode);

    Array<BaseExpr> gs;
    Array<BaseExpr> fs;
    Stmt body;
    Type rt;
    // Corresponds to Template Parameters in C++'s Terminology
    Array<TypeVar> ts;
};

class AstFunc : public BaseFunc {
public:
    DEFINE_NODE_CLASS(AstFunc, BaseFunc, AstFuncNode);
    DEFINE_COW_METHOD(AstFuncNode);

    explicit AstFunc(Str name,
                    Array<BaseExpr> gs, Array<BaseExpr> fs,
                    Stmt body, Type rt, Array<TypeVar> ts);

    explicit AstFunc(std::string name,
                    Array<BaseExpr> gs, Array<BaseExpr> fs,
                    Stmt body, Type rt, Array<TypeVar> ts);

    explicit AstFunc(const char* name,
                    Array<BaseExpr> gs, Array<BaseExpr> fs,
                    Stmt body, Type rt, Array<TypeVar> ts);

    explicit AstFunc(Array<BaseExpr> gs, Array<BaseExpr> fs,
                    Stmt body, Type rt, Array<TypeVar> ts);

    Str name() const { return static_cast<const BaseFuncNode*>(get())->name; }
};

class IRModuleNode : public object_t {
public:
    static constexpr const std::string_view NAME = "IRModule";
    static constexpr const int32_t INDEX = TypeIndex::Module;
    DEFINE_TYPEINDEX(IRModuleNode, object_t);

    Map<GlobalVar, BaseFunc> func_;

    Map<GlobalTypeVar, ClassType> class_;

    void VisitAttrs(AttrVisitor* v) override;

    void Insert(const GlobalVar& var, const BaseFunc& func) {
        func_[var] = func;
    }
private:
    std::unordered_map<std::string, GlobalVar> vars_;
    std::unordered_map<std::string, GlobalTypeVar> class_vars_;
    friend class IRModule;
};

class IRModule : public object_r {
public:
    DEFINE_COW_METHOD(IRModuleNode);

    explicit IRModule(object_p<object_t> n) noexcept 
        : object_r(std::move(n)) {} 
    
    IRModuleNode* operator->() const noexcept {
        auto* p = get_mutable();
        return static_cast<IRModuleNode*>(p);
    }

    explicit IRModule(Map<GlobalVar,BaseFunc> funcions,
                      Map<GlobalTypeVar, ClassType> ts = {});

    IRModule() : IRModule(Map<GlobalVar,BaseFunc>({})) {}

    static IRModule From(const AstExpr& e,
                         const Map<GlobalVar, BaseFunc>& funcs = {},
                         const Map<GlobalTypeVar, ClassType>& types = {});

    using ContainerType = IRModuleNode;
};

DataType GetRuntimeDataType(const Type& t);
bool IsRuntimeDataType(const Type& t);



} // namespace runtime
} // namespace mc