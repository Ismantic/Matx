#include "runtime_value.h"
#include "datatype.h"
#include "expression.h"
#include "str.h"

#include <iostream>

namespace mc {
namespace runtime {

template<>
bool Any::Is<DataType>() const noexcept {
    if (value_.t != TypeIndex::DataType && value_.t != TypeIndex::Str) {
        return false;
    }
    return true;
}

template<>
DataType Any::As_<DataType>() const noexcept {
    switch (value_.t) {
        case TypeIndex::Str: {
            return DataType(StrToDt(As_<const char*>()));
        }
        default:
            return DataType(value_.u.v_datatype);
    }
}

/*
template<> 
bool Any::Is<BaseExpr>() const noexcept { 
    return IsObjectType<BaseExprNode>();
}

template<> 
BaseExpr Any::As_<BaseExpr>() const noexcept {
    auto* ptr = static_cast<BaseExprNode*>(value_.u.v_pointer);
    return BaseExpr(object_p<object_t>(ptr));
}
*/

McValue::McValue(const Any& o) {
    CopyFrom(o);
}

void McValue::AsValue(Value* value) noexcept {
    if (value_.t == TypeIndex::Str && value_.u.v_str != nullptr) {
        // 1. 先保存值，因为后面会修改 value_
        const char* old_str = value_.u.v_str;
        int32_t len = strlen(old_str);

        // 2. 创建新的字符串副本，使用 new[] 以配对 delete[]
        char* new_str = new char[len + 1];
        memcpy(new_str, old_str, len);
        new_str[len] = '\0';
        
        // 3. 设置新的 Value
        value->t = TypeIndex::Str;
        value->p = len;
        value->u.v_str = new_str;

    } else {
        *value = value_;
    }
    
    value_.t = TypeIndex::Null;
    value_.u.v_pointer = nullptr;  // 保持与 Clean() 一致
}

bool operator==(const McValue& u, const McValue& v) {
    if (u.T() != v.T()) {
        return false;
    }

    switch (u.T()) {
        case TypeIndex::Null:
            return true;
        case TypeIndex::Int:
            return u.As<int64_t>() == v.As<int64_t>();
        case TypeIndex::Float:
            return u.As<double>() == v.As<double>();
        case TypeIndex::Str:
            return strcmp(u.As<const char*>(), v.As<const char*>()) == 0;
        default:
            if (u.T() >= TypeIndex::Object) {
                auto* u_obj = static_cast<object_t*>(u.value_.u.v_pointer);
                auto* v_obj = static_cast<object_t*>(v.value_.u.v_pointer);
                if (u_obj == nullptr || v_obj == nullptr) {
                    return u_obj == v_obj;
                }
                if (u_obj->IsType<StrNode>() && v_obj->IsType<StrNode>()) {
                    Str u_str{object_p<object_t>(u_obj)};
                    Str v_str{object_p<object_t>(v_obj)};
                    return u_str == v_str;
                }
                return u_obj == v_obj;
            }
            return false;
    }
}

std::ostream& operator<<(std::ostream& out, const Any& input) {
    switch (input.T()) {
        case TypeIndex::Null: {
            out << "nullptr";
        } break;
        case TypeIndex::Int: {
            out << input.As<int64_t>();
        } break;
        case TypeIndex::Float: {
            out << input.As<double>(); // TODO
        } break;
        case TypeIndex::Str: {
            out << input.As<const char*>();
        } break;
        default : {
            out << "Object(" << input.As<void*>() << ")";
        } break;
    }
    return out;
}

Object AsObject(Any value) {
    if (value.IsObject()) {
      Object node(object_p<object_t>(
                static_cast<object_t*>(value.As_<void*>())));
      return node;
    }
    throw std::runtime_error("NotObject");
}

} // namespace runtime

} // namespace mc
