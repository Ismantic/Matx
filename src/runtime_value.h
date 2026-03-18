#pragma once 

#include <string>
#include <memory>
#include <iostream>

#include <stdint.h>
#include <string.h>

#include "object.h"
#include "str.h"

#include "c_api.h"

namespace mc {
namespace runtime {

class McView;
class McValue;

class Any {
protected:
    Value value_;

    friend class McView;
    friend class McValue;
    
    constexpr Any(Value value) noexcept : value_(value) {}
    constexpr Any(const Value* value) noexcept : value_(*value) {}

public:
    constexpr const Value& value() const noexcept { return value_; }

    constexpr Any() noexcept : value_({0,0,TypeIndex::Null}) {}

    bool IsNull() const noexcept {
        return value_.t == TypeIndex::Null;
    }
    bool IsInt() const noexcept {
        return value_.t == TypeIndex::Int;
    }
    bool IsFloat() const noexcept {
        return value_.t == TypeIndex::Float;
    }
    bool IsStr() const noexcept {
        return value_.t == TypeIndex::Str;
    }
    bool IsDataType() const noexcept {
        return value_.t == TypeIndex::DataType || value_.t == TypeIndex::Str;
    }
    bool IsObject() const noexcept {
        return value_.t >= TypeIndex::Object;
    }

    template<typename T>
    bool Is() const noexcept {
        using RawType = std::remove_cv_t<std::remove_reference_t<T>>;
        return Is<RawType>(std::is_base_of<object_r, RawType>{});
    }

    template<typename T>
    bool Is(std::false_type) const noexcept {
        if constexpr(std::is_same_v<T, int>) {
            return IsInt();
        } else if constexpr(std::is_same_v<T, float>) {
            return IsFloat();
        } else if constexpr(std::is_same_v<T, const char*>) {
            return IsStr();
        } else if constexpr(std::is_same_v<T, char*>) {
            return IsStr();
        } else if constexpr(std::is_same_v<T, std::string>) {
            return IsStr();
        } else if constexpr(std::is_same_v<T, Dt>) {
            return value_.t == TypeIndex::DataType;
        }
        return false;
    }

    template<typename T>
    bool Is(std::true_type) const noexcept {
        if (!IsObject()) return false;
        auto* object = static_cast<object_t*>(value_.u.v_pointer);
        if (!object) return false;
        
        if constexpr (std::is_base_of_v<object_r, T>) {
            if constexpr (std::is_same_v<T, object_r>) {
                return true;
            } else {
                using NodeType = typename std::remove_pointer_t<
                    decltype(std::declval<T>().operator->())>;
                return object->template IsType<NodeType>();
            }
        }
        
        return false;
    }

    template<typename T>
    T As() const {
        if (!Is<T>()) {
            throw std::runtime_error("Type Mismatch in Conversion");
        }
        return As_<T>();
    }

    template<typename T>
    T As_() const noexcept {
        using RawType = std::remove_cv_t<std::remove_reference_t<T>>;
        return As_<RawType>(std::is_base_of<object_r, RawType>{});
    }

    template<typename T>
    T As_(std::false_type) const noexcept {
        if constexpr(std::is_same_v<T, int>) {
            return value_.u.v_int;
        } else if constexpr(std::is_same_v<T, float>) {
            return value_.u.v_float;
        } else if constexpr(std::is_same_v<T, const char*>) {
            return value_.u.v_str;
        } else if constexpr(std::is_same_v<T, char*>) {
            return value_.u.v_str;
        } else if constexpr(std::is_same_v<T, std::string>) {
            return std::string(value_.u.v_str);
        } else if constexpr(std::is_same_v<T, Dt>) {
            return value_.u.v_datatype;
        }
        return T();
    }

    template<typename T>
    T As_(std::true_type) const noexcept {
        auto* ptr = static_cast<object_t*>(value_.u.v_pointer);
        if (!ptr) return T();
        return T(object_p<object_t>(ptr));
    }

    int32_t T() const noexcept { return value_.t; }
};

template<> inline bool Any::Is<int64_t>() const noexcept { 
    return value_.t == TypeIndex::Int; 
}

template<> inline bool Any::Is<double>() const noexcept { 
    return value_.t == TypeIndex::Float || value_.t == TypeIndex::Int; 
}

template<> inline bool Any::Is<const char*>() const noexcept { 
    return value_.t == TypeIndex::Str; 
}

template<> inline bool Any::Is<Dt>() const noexcept {
  return value_.t == TypeIndex::DataType;
}

template<> inline bool Any::Is<void*>() const noexcept { 
    return value_.t >= TypeIndex::Object || value_.t == TypeIndex::Pointer; 
}

template<> inline int Any::As_<int>() const noexcept {
    return static_cast<int>(value_.u.v_int);
}

template<> inline int64_t Any::As_<int64_t>() const noexcept {
    return value_.u.v_int;
}

template<> inline double Any::As_<double>() const noexcept {
    if (value_.t == TypeIndex::Int) {
        return static_cast<double>(value_.u.v_int);
    }
    return value_.u.v_float;
}

template<> inline const char* Any::As_<const char*>() const noexcept {
    return value_.u.v_str;
}

template<> inline Dt Any::As_<Dt>() const noexcept {
    return value_.u.v_datatype;
}

template<> inline void* Any::As_<void*>() const noexcept {
    // 支持所有对象类型和指针类型
    if (value_.t >= TypeIndex::Object || value_.t == TypeIndex::Pointer) {
        return value_.u.v_pointer;
    }
    return nullptr;
}


class McView : public Any {
public:
  using Any::Any;

  ~McView() noexcept = default;

  constexpr McView() noexcept : Any() {}

  constexpr McView(Value value) noexcept: Any(value) {}

  constexpr McView(const Value* value) noexcept : Any(*value) {}

  template <typename R,
              typename = typename std::enable_if<std::is_base_of<object_r, R>::value>::type>
  McView(const R& value) noexcept {
      *this = McView(McValue(value));
  }

  McView(const McView&) noexcept = default;
  McView& operator=(const McView&) noexcept = default;
  McView(McView&& other) noexcept = default;
  McView& operator=(McView&& other) noexcept = default;

  McView(const McValue& value) noexcept;

};

class McValue : public Any {
public:
  McValue() noexcept = default;
  ~McValue() { Clean(); }

  explicit McValue(const Any& o);


  template<typename T,
           typename = typename std::enable_if<std::is_base_of<object_t, T>::value>::type>
  McValue(object_p<T> obj) noexcept {
    if (obj.get()) {
        value_.t = obj->Index();
        value_.u.v_pointer = obj.get();
        obj.get()->IncCounter();
    } else {
        value_.t = TypeIndex::Null;
        value_.u.v_pointer = nullptr;
    }
  }

  template <typename R,
            typename = typename std::enable_if<std::is_base_of<object_r, R>::value>::type>
  McValue(R value) noexcept {
    if (value.data_.data_) {
      value_.t = value.data_.data_->Index();
      value_.u.v_pointer = value.data_.data_;
      value.data_.data_ = nullptr;
    } else {
      value_.t = TypeIndex::Null;
      value_.u.v_pointer = nullptr;
    }
  } 

  McValue(int32_t value) noexcept {
    value_.u.v_int = value;
    value_.t = TypeIndex::Int;
  }

  McValue(uint32_t value) noexcept {
    value_.u.v_int = value;
    value_.t = TypeIndex::Int;
  }

  McValue(int64_t value) noexcept {
    value_.u.v_int = value;
    value_.t = TypeIndex::Int;
  }

  McValue(uint64_t value) noexcept {
    value_.u.v_int = value;
    value_.t = TypeIndex::Int;
  }

  McValue(float value) noexcept {
    value_.u.v_float = value;
    value_.t = TypeIndex::Float;
  }

  McValue(double value) noexcept {
    value_.u.v_float = value;
    value_.t = TypeIndex::Float;
  }

  McValue(const char* str) {
    if (str) {
        size_t len = strlen(str);
        value_.u.v_str = new char[len+1];
        strcpy(value_.u.v_str, str);
        value_.t = TypeIndex::Str;
    } else {
        value_.t = TypeIndex::Null;
        value_.u.v_str = nullptr;
    }
  }

  McValue(void* ptr) noexcept {
    value_.u.v_pointer = ptr;
    value_.t = TypeIndex::Pointer;
  }

  McValue(const std::string& str) 
    : McValue(str.c_str()) { }
  
  McValue(Dt datatype) noexcept {
    value_.u.v_datatype = datatype;
    value_.t = TypeIndex::DataType;
  }

  McValue(DataType datatype) 
    : McValue(datatype.data_) {}

  McValue(const McValue& other) {
    CopyFrom(other);
  }

  McValue& operator=(const McValue& other) {
    if (this != &other) {
        Clean();
        CopyFrom(other);
    }
    return *this;
  }

  McValue(McValue&& other) noexcept {
    value_ = other.value_;
    other.value_.t = TypeIndex::Null;
    other.value_.u.v_pointer = nullptr;
  }

  McValue& operator=(McValue&& other) noexcept {
    if (this != &other) {
        Clean();
        value_ = other.value_;
        other.value_.t = TypeIndex::Null;
        other.value_.u.v_pointer = nullptr;
    }
    return *this;
  }

  McValue(const McView& view) {
    CopyFrom(view);
  }

  template<typename T>
  T MoveTo() {
    T r = As<T>();
    value_.t = TypeIndex::Null;
    value_.u.v_pointer = nullptr;
    return r;
  }

  void AsValue(Value* value) noexcept;

private:
  void Clean() noexcept {
    if (value_.t == TypeIndex::Str) {
        delete[] value_.u.v_str;
    } else if (value_.t >= TypeIndex::Object) {
        if (value_.u.v_pointer) {
            static_cast<object_t*>(value_.u.v_pointer)->DecCounter();
        }
    }
    value_.t = TypeIndex::Null;
    value_.u.v_pointer = nullptr;
  }

  void CopyFrom(const Any& other) {
    value_.t = other.T();

    switch (value_.t)
    {
    case TypeIndex::Null:
        value_.u.v_pointer = nullptr;
        break;
    case TypeIndex::Int:
        value_.u.v_int = other.As_<int64_t>(); 
        break;
    case TypeIndex::Float:
        value_.u.v_float = other.As_<double>();
        break;
    case TypeIndex::Str: {
        const char* str = other.As_<const char*>();
        if (str) {
            size_t len = strlen(str);
            value_.u.v_str = new char[len+1];
            strcpy(value_.u.v_str, str);
        } else {
            value_.u.v_str = nullptr;
        }
        break;
    }
    case TypeIndex::DataType: {
      value_.u.v_datatype = other.As_<Dt>();
      break;
    }
    case TypeIndex::Pointer: {
      value_.u.v_pointer = other.As_<void*>();
      break;
    }
    default:
        if (other.IsObject()) {
            object_t* obj = static_cast<object_t*>(other.value_.u.v_pointer);
            value_.u.v_pointer = obj;
            if (obj) obj->IncCounter();
        }
        break;
    }
  }

  friend bool operator==(const McValue& u, const McValue& v);

};

inline McView::McView(const McValue& value) noexcept {
    value_ = value.value_;
}

extern std::ostream& operator<<(std::ostream& out, const Any& input);

Object AsObject(Any value);


} // namespace runtime

} // namespace mc

namespace std {
template<>
struct hash<mc::runtime::McValue> {
    size_t operator()(const mc::runtime::McValue& value) const {
        switch(value.T()) {
            case mc::runtime::TypeIndex::Null:
                return 0;
            case mc::runtime::TypeIndex::Int:
                return std::hash<int64_t>{}(value.As_<int64_t>());
            case mc::runtime::TypeIndex::Float:
                return std::hash<double>{}(value.As_<double>());
            case mc::runtime::TypeIndex::Str:
                return std::hash<std::string>{}(
                    value.As_<const char*>() ? value.As_<const char*>() : ""
                );
            default:
                if (value.IsObject()) {
                  auto* obj = static_cast<mc::runtime::object_t*>(value.As_<void*>());
                  if (obj && obj->IsType<mc::runtime::StrNode>()) {
                      mc::runtime::Str str{mc::runtime::object_p<mc::runtime::object_t>(obj)};
                      return std::hash<std::string_view>{}(std::string_view(str));
                  }
                  return obj ? std::hash<void*>{}(obj) : 0;
                }
                return 0;
        }
    }
};
} // namespace std
