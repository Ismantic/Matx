#pragma once

#include <string>
#include <string_view>
#include <atomic>

#include <stdint.h>

#include "datatype.h"

namespace mc {
namespace runtime {

class AttrVisitor;

template <typename T>
class object_p; 

class object_t {
public:
  virtual void VisitAttrs(AttrVisitor* visitor) {}

  virtual ~object_t();

  object_t() {};
  object_t(const object_t& o) {};
  object_t& operator=(const object_t& o) { return *this; };
  object_t& operator=(object_t&& o) { return *this; }

  virtual int32_t Index() const {
    return t_;
  }
  virtual std::string Name() const;

  static constexpr const int32_t INDEX = TypeIndex::Dynamic;
  static constexpr const std::string_view NAME = "Object";

  static int32_t _GetOrAllocRuntimeTypeIndex() { return TypeIndex::Object; }
  static int32_t RuntimeTypeIndex() { return TypeIndex::Object; }

  bool IsFrom(int32_t p) const;

  template <typename T>
  bool IsType() const noexcept {
    return IsFrom(T::RuntimeTypeIndex());
  }

  void IncCounter() noexcept { ++count_; }
  void DecCounter() noexcept {
    if (--count_ == 0) {
      delete this;
    }
  }
  int32_t UseCount() const noexcept {
    return count_;
  }


protected:
  static int32_t GetOrAllocRuntimeTypeIndex(const std::string_view& name, 
                                            int32_t st,
                                            int32_t pt);
  int32_t t_{0};
  std::atomic<int32_t> count_{0};

private:
  template <typename T, typename... Gs>
  friend object_p<T> MakeObject(Gs&&... args); 

  friend class McValue;
};

template <typename T>
class object_p {
public:
  object_p() noexcept = default;
  explicit object_p(T* p) noexcept : data_{p} {
    if (data_) data_->IncCounter();
  }
  ~object_p() {
    if (data_) data_->DecCounter();
  }

  object_p(const object_p& other) noexcept : data_(other.data_) {
    if (data_) data_->IncCounter();
  }

  template<typename U>
  object_p(const object_p<U>& other) noexcept : data_(other.get()) {
      static_assert(std::is_base_of<T, U>::value,
                     "can only assign of child class object_p to parent");
      if (data_) data_->IncCounter();
  }

  object_p& operator=(const object_p& other) noexcept {
    if (data_ != other.data_) {
      T* o = data_;
      data_ = other.data_;
      if (data_) data_->IncCounter();
      if (o) o->DecCounter();
    }
    return *this;
  }

  object_p(object_p&& other) noexcept : data_(other.data_) {
    other.data_ = nullptr;
  }

  object_p& operator=(object_p&& other) noexcept {
    if (this != &other) {
      T* o = data_;
      data_ = other.data_;
      other.data_ = nullptr;
      if (o) o->DecCounter();
    }
    return *this;
  }

  void swap(object_p<T>& other) noexcept {
    std::swap(data_, other.data_);
  }

  T* get() const noexcept { return data_; }
  T* operator->() const noexcept { return data_; }
  T& operator*() const noexcept { return *data_; }

  bool operator==(const object_p<T>& other) const noexcept {
    return data_ == other.data_;
  }
  bool operator!=(const object_p<T>& other) const noexcept {
    return data_ != other.data_;
  }
  bool operator==(std::nullptr_t n) const noexcept {
    return data_ == nullptr;
  }
  bool operator!=(std::nullptr_t n) const noexcept {
    return data_ != nullptr;
  }

  void reset() noexcept {
    if (data_) {
      data_->DecCounter();
      data_ = nullptr;
    }
  }

  bool unique() const noexcept {
    return data_ && data_->UseCount() == 1;
  }

  explicit operator bool() const noexcept {
    return data_ != nullptr;
  }

private:
  T* data_{nullptr};
  template<typename U> friend class object_p;

  friend class McValue;
};

class object_r {
public:
  object_r() noexcept = default;
  explicit object_r(object_p<object_t> p) noexcept
    : data_(std::move(p)) {}
  
  const object_t* get() const noexcept { return data_.get(); }
  object_t* get_mutable() const noexcept { return data_.get(); }
  const object_t* operator->() const noexcept { return get(); }

  template<typename T>
  const T* As() const noexcept {
    if (data_ && data_->IsType<T>()) {
      return static_cast<const T*>(data_.get());
    }
    return nullptr;
  }

  bool operator==(const object_r& other) const noexcept {
    return data_ == other.data_;
  }

  bool operator!=(const object_r& other) const noexcept {
    return data_ != other.data_;
  }

  bool none() const noexcept {
    return data_ == nullptr;
  }

protected:
  object_p<object_t> data_;

  friend class McValue;
};

struct object_s {
    size_t operator()(const object_r& obj) const {
        return std::hash<const void*>()(obj.get());
    }

    template <typename T>
    size_t operator()(const T& e) const {
        return std::hash<const void*>()(e.get());
    }
};

struct object_e {
    bool operator()(const object_r& a, const object_r& b) const {
        return a.get() == b.get();
    }

    template <typename T>
    bool operator()(const T& a, const T& b) const {
        return a.get() == b.get();
    } 
};

#define DEFINE_TYPEINDEX(TypeName, ParentType)  \
  static int32_t RuntimeTypeIndex() {                                                         \
    if (TypeName::INDEX != ::mc::runtime::TypeIndex::Dynamic) {                        \
      _GetOrAllocRuntimeTypeIndex();                                          \
      return TypeName::INDEX;                                                            \
    }                                                                                          \
    return _GetOrAllocRuntimeTypeIndex();                                                      \
  }                                                                                            \
  static int32_t _GetOrAllocRuntimeTypeIndex() {                                              \
    static int32_t t = object_t::GetOrAllocRuntimeTypeIndex(                               \
        TypeName::NAME, TypeName::INDEX, ParentType::_GetOrAllocRuntimeTypeIndex()); \
    return t;                                                                             \
  }

#define REGISTER_TYPEINDEX(TypeName)  \
  STR_CONCAT(REG_VAR, __COUNTER__) =  \
    TypeName::_GetOrAllocRuntimeTypeIndex()

template <typename T, typename... Gs>
object_p<T> MakeObject(Gs&&... gs) {
    static_assert(std::is_base_of<object_t, T>::value, "MakeObject can only be used to create object_t");
    //return object_p<T>(new T(std::forward<Gs>(gs)...));
    T* p = new T(std::forward<Gs>(gs)...);
    p->t_ = T::RuntimeTypeIndex();
    return object_p<T>(p);
}

#define DEFINE_COW_METHOD(TypeName)                                 \
  TypeName* CopyOnWrite() {                                         \
    if (!data_.unique()) {                                          \
      auto n = MakeObject<TypeName>(*(operator->()));               \
      object_p<object_t>(std::move(n)).swap(data_);                 \
    }                                                              \
    return static_cast<TypeName*>(data_.get());                    \
  }

using Object = object_r;

template <typename R>
inline R Downcast(const Object& from) {
    return R(object_p<object_t>(const_cast<object_t*>(from.get())));
}

template <typename R, typename T>
inline R RTcast(const T* p) {
    return R(object_p<object_t>(
               const_cast<object_t*>(
                static_cast<const object_t*>(p))));
}

template <typename BaseType, typename ObjType>
inline object_p<BaseType> NTcast(ObjType* ptr) noexcept {
  static_assert(std::is_base_of<BaseType, ObjType>::value,
                "Can only cast to the ref of same container type");
  return object_p<BaseType>(static_cast<object_t*>(ptr));
}

int32_t GetIndex(const std::string_view& name);


} // namespace runtime
} // namespace mc