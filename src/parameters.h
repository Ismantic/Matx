#pragma once 

#include <initializer_list>

namespace mc {
namespace runtime {

class Any;
class McView;

class Parameters {
public:
  constexpr Parameters() : item_(nullptr), size_(0) { }

  constexpr explicit Parameters(const Any* begin, size_t len)
    : item_(const_cast<Any*>(begin)), size_(len) {}

  constexpr Parameters(std::initializer_list<McView> gs)
    : item_(const_cast<Any*>(static_cast<const Any*>(gs.begin())))
    , size_(gs.size()) {}


  Parameters(const Parameters& other) = default;
  Parameters(Parameters&& other) = default;
  Parameters& operator=(const Parameters& other) = default;
  Parameters& operator=(Parameters&& other) = default;

  constexpr int size() const {
    return size_;
  }
  constexpr const Any* begin() const {
    return item_;
  }
  constexpr const Any* end() const {
    return item_ + size_;
  }
  inline const Any& operator[](int64_t i) const {
    return *(item_ + i);
  }

  inline Any& operator[](int64_t i) {
    return *(item_ + i);
  }

  bool empty() {
    return size_ == 0;
  }

private:
  Any* item_;
  size_t size_;
};

} // namespace runtime
} // namespace mc