#pragma once 

#include <string>
#include <sstream>

#include "object.h"

namespace mc {
namespace runtime {

class StrNode;
class Str;

class StrNode : public object_t {
public:
    static constexpr const int32_t INDEX = TypeIndex::RuntimeStr;
    static constexpr const std::string_view NAME = "RuntimeStr";
    DEFINE_TYPEINDEX(StrNode, object_t);

    using container_type = std::string;
    using value_type = char;
    using size_type = size_t;

    StrNode() = default;
    explicit StrNode(container_type data) : data_(std::move(data)) {}
    StrNode(const StrNode& other) : data_(other.data_) {}

    size_type size() const noexcept { return data_.size(); }
    bool empty() const noexcept { return data_.empty(); }
    const char* data() const noexcept { return data_.data(); }
    const char* c_str() const noexcept { return data_.c_str(); }    

    StrNode& operator=(const StrNode& other) {
        if (this != &other) {
            data_ = other.data_;
        }
        return *this;
    }

    void VisitAttrs(AttrVisitor* v) override;

private:
    container_type data_;

    friend class Str;
};

class Str : public object_r {
public:
    using node_type = StrNode;
    using value_type = char;
    using size_type = size_t;

    Str() : object_r(MakeObject<StrNode>()) {}

    Str(const char* str) 
        : object_r(MakeObject<StrNode>(StrNode::container_type(str))) {}
    
    Str(const std::string& str) 
        : object_r(MakeObject<StrNode>(str)) {}
    
    explicit Str(object_p<object_t> obj) 
        : object_r(std::move(obj)) {}

    Str(const Str&) = default;
    Str(Str&&) = default;
    Str& operator=(const Str&) = default;
    Str& operator=(Str&&) = default;

    const StrNode* operator->() const {
        return Get(); 
    }

    size_type size() const noexcept { return Get()->size(); }
    bool empty() const noexcept { return Get()->empty(); }
    const char* data() const noexcept { return Get()->data(); }
    const char* c_str() const noexcept { return Get()->c_str(); }

    // Comparison operators
    bool operator==(const Str& other) const noexcept {
        return std::string_view(data(), size()) == 
               std::string_view(other.data(), other.size());
    }
    
    bool operator!=(const Str& other) const noexcept {
        return !(*this == other);
    }

    // COW support
    DEFINE_COW_METHOD(StrNode)

    // Conversion operators
    operator std::string_view() const noexcept {
        return std::string_view(data(), size());
    }

private:
    const StrNode* Get() const {
        return static_cast<const StrNode*>(get());
    }

    StrNode* GetMutable() {
        return static_cast<StrNode*>(get_mutable());
    }
};

inline Str operator+(const Str& lhs, const Str& rhs) {
    std::string result;
    result.reserve(lhs.size() + rhs.size());
    result.append(lhs.data(), lhs.size());
    result.append(rhs.data(), rhs.size());
    return Str(result);
}

inline std::ostream& operator<<(std::ostream& os, const Str& str) {
    return os.write(str.data(), str.size());
}

} // namespace runtime

} // namespace mc

template<>
struct std::hash<mc::runtime::Str> {
    size_t operator()(const mc::runtime::Str& str) const noexcept {
        return std::hash<std::string_view>{}(std::string_view(str));
    }
};

