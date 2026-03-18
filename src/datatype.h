#pragma once

#include <string_view>
#include <stdint.h>

namespace mc {
namespace runtime {

struct Dt {
    uint8_t c_; // Code, e.g, Int, Float
    uint8_t b_; // e.g. 8, 16, 32
    uint16_t a_; // e.g. 1
};

class DataType {
public:
    enum TypeCode {
        kInt = 0,
        kUInt = 1,
        kFloat = 2,
        kHandle = 3
    };

    DataType() {}

    explicit DataType(Dt t) : data_(t) {}

    DataType(int c, int b, int a) {
        data_.c_ = static_cast<uint8_t>(c);
        data_.b_ = static_cast<uint8_t>(b);
        data_.a_ = static_cast<uint16_t>(a);
    }

    static DataType Int(int b, int a = 1) {
        return DataType(kInt, b, a);
    }

    static DataType Float(int b, int a = 1) {
        return DataType(kFloat, b, a);
    }

    static DataType Bool(int a = 1) {
        return DataType(kUInt, 1, a);
    }

    static DataType Handle(int b = 64, int a = 1) {
        return DataType(kHandle, b, a);
    }

    static DataType Void() {
        return DataType(kHandle, 0, 0);
    }

    bool operator==(const DataType& o) const {
        return data_.c_ == o.data_.c_ && 
               data_.b_ == o.data_.b_ &&
               data_.a_ == o.data_.a_;
    }

    bool operator!=(const DataType& o) const {
        return !operator==(o);
    }

    int c() const {
        return static_cast<int>(data_.c_);
    }

    int b() const {
        return static_cast<int>(data_.b_);
    }

    int a() const {
        return static_cast<int>(data_.a_);
    }

    bool IsScalar() const {
        return a() == 1;
    }

    bool IsBool() const {
        return c() == DataType::kUInt && b() == 1;
    }

    bool IsFloat() const {
        return c() == DataType::kFloat;
    }

    bool IsInt() const {
        return c() == DataType::kInt;
    }

    bool IsUint() const {
        return c() == DataType::kUInt;
    }

    bool IsVoid() const {
        return c() == DataType::kHandle && b() == 0 && a() == 0;
    }

    bool IsHandle() const {
        return c() == DataType::kHandle && ! IsVoid();
    }

private:
    Dt data_{};

    friend class McValue;
};

Dt StrToDt(std::string_view s);
std::string DtToStr(const Dt& t);
std::ostream& operator<<(std::ostream& os, const Dt& t);

} // namespace runtime
} // namespace mc

namespace mc {
namespace runtime {
struct TypeIndex {
  enum : int32_t {
    Null = -1,
    Int = -2,
    Float = -3,
    Str = -4,
    Pointer = -5,
    Func = -6,
    DataType = -7,
    Object = 0,
    Module = 1,

    RuntimeStr = 3,

    RuntimeArray = 5,
    RuntimeMap = 6,
    RuntimeList = 7,
    RuntimeDict = 8,
    RuntimeSet = 9,
    RuntimeIterator = 10,

    RuntimeTuple = 22,

    Dynamic = 256
  
  };
};

} // namespace runtime  
} // namespace mc
