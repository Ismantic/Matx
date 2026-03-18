#include "datatype.h"

#include <sstream>
#include <stdexcept>


namespace mc {
namespace runtime {

Dt StrToDt(std::string_view s) {
    Dt t;
    t.b_ = 32;  // default bits
    t.a_ = 1;   // default lanes

    // 先处理 bool 类型的特殊情况
    if (s == "bool") {
        t.c_ = DataType::kUInt;
        t.b_ = 1;      // bool 用1位表示
        t.a_ = 1;
        return t;
    }

    const char* scan;
    if (s.substr(0, 3) == "int") {
        t.c_ = DataType::kInt;
        scan = s.data() + 3;
    } else if (s.substr(0, 4) == "uint") {
        t.c_ = DataType::kUInt;
        scan = s.data() + 4;
    } else if (s.substr(0, 5) == "float") {
        t.c_ = DataType::kFloat;
        scan = s.data() + 5;
    } else if (s == "handle") {
        t.c_ = DataType::kHandle;
        t.b_ = 64;  // default bits for handle
        t.a_ = 1;
        return t;
    } else {
        throw std::runtime_error("unknown type " + std::string(s));
    }

    char* x;
    uint8_t b = static_cast<uint8_t>(strtoul(scan, &x, 10));
    if (b != 0) {
        t.b_ = b;
    }

    char* e = x;
    if (*x == 'x') {
        t.a_ = static_cast<uint16_t>(strtoul(x + 1, &e, 10));
    }

    // Check if we consumed the entire string
    if (e != s.data() + s.length()) {
        throw std::runtime_error("invalid type format: " + std::string(s));
    }

    return t;
}

std::string _ToStr(int c) {
    switch (c) {
        case DataType::kInt: return "int";
        case DataType::kUInt: return "uint";
        case DataType::kFloat: return "float";
        case DataType::kHandle: return "handle";
        default: throw std::runtime_error("unknown type code");
    }
}

std::ostream& operator<<(std::ostream& os, const Dt& t) {
    if (t.b_ == 1 && t.a_ == 1 && t.c_ == DataType::kUInt) {
        os << "bool";
        return os;
    }

    if (t.c_ == DataType::kHandle) {
        os << "handle";
        return os;
    }

    os << _ToStr(t.c_);
    
    os << static_cast<int>(t.b_);
    
    if (t.a_ != 1) {
        os << 'x' << static_cast<int>(t.a_);
    }
    
    return os;
}

std::string DtToStr(const Dt& t) {
    if (t.b_ == 0) {
        return {};  
    }
    std::ostringstream os;
    os << t;
    return os.str();
}


} // namespace runtime
} // namespace mc