#pragma once

#include <functional>
#include <stdint.h>
#include <ostream>

#include "object.h"
#include "runtime_value.h"

namespace mc {
namespace runtime {

class IteratorNode : public object_t {
public:
    static constexpr const int32_t INDEX = TypeIndex::RuntimeIterator;
    static constexpr const std::string_view NAME = "Iterator";
    DEFINE_TYPEINDEX(IteratorNode, object_t);

    virtual bool HasNext() const = 0;
    virtual McValue Next() = 0;
    virtual McValue Next(bool* has_next) = 0;
    virtual McView NextView(bool* has_next, McValue*holder_or_null) = 0;
    virtual int64_t Distance() const = 0;
    virtual uint64_t HashCode() const = 0;
};

class GenericIteratorNode : public IteratorNode {
public:
  GenericIteratorNode(McValue container, 
                      std::function<bool()> has_next_fn,
                      std::function<McValue()> next_fn,
                      std::function<McValue(bool*)> next_check_fn)
    : container_(container)
    , has_next_fn_(std::move(has_next_fn))
    , next_fn_(std::move(next_fn))
    , next_check_fn_(std::move(next_check_fn)) {}

    bool HasNext() const override {
        return has_next_fn_();
    }

    McValue Next() override {
        return next_fn_();
    }

    McValue Next(bool* has_next) override {
        return next_check_fn_(has_next);
    }

    McView NextView(bool* has_next, McValue* holder_or_null) override {
        *holder_or_null = next_check_fn_(has_next);
        return *holder_or_null;
    }

    int64_t Distance() const override {
        return -1; 
    }

    uint64_t HashCode() const override {
        return reinterpret_cast<uint64_t>(this);
    }

private:
  McValue container_;
  std::function<bool()> has_next_fn_;
  std::function<McValue()> next_fn_;
  std::function<McValue(bool*)> next_check_fn_;
};

class Iterator : public object_r {
public:
  using ContainerType = IteratorNode;
  static constexpr bool _type_is_nullable = false;

  Iterator() noexcept = default;
  Iterator(const Iterator& other) noexcept = default;
  Iterator(Iterator&& other) noexcept = default;

  template<typename T, typename = typename std::enable_if<std::is_base_of<object_t, T>::value>::type>
  explicit Iterator(object_p<T> n) noexcept 
    : object_r(std::move(n)) {}

  bool HasNext() const {
    auto* node = GetMutableNode();
    return node && node->HasNext();
  }

  McValue Next() const {
    auto* node = GetMutableNode();
    if (!node) {
        throw std::runtime_error("Iterator is null");
    }
    return node->Next();
  }

  McValue Next(bool* has_next) const {
    auto* node = GetMutableNode();
    if (!node) {
        *has_next = false;
        throw std::runtime_error("Iterator is null");
    }
    return node->Next(has_next);
  }

  McView NextView(bool* has_next, McValue* holder_or_null) const {
    auto* node = GetMutableNode();
    if (!node) {
        *has_next = false;
        throw std::runtime_error("Iterator is null");
    }
    return node->NextView(has_next, holder_or_null);
  }

  int64_t Distance() const {
    auto* node = GetMutableNode();
    return node ? node->Distance() : 0;
  }

  static Iterator MakeGenericIterator(McValue container, 
                                      std::function<bool()> has_next,
                                      std::function<McValue()> next,
                                      std::function<McValue(bool*)> next_and_check) {
    auto node = MakeObject<GenericIteratorNode>(
        std::move(container),
        std::move(has_next),
        std::move(next),
        std::move(next_and_check));
    //return Iterator(object_p<object_t>(node.get()));
    return Iterator(std::move(node));
  }

  static Iterator MakeGenericIterator(const Any& container) {
    if (container.IsNull()) {
        throw std::runtime_error("Cannot iterator over null");
    }
    // TODO
    throw std::runtime_error("Unsupported container type");
  }

  static Iterator MakeItemsIterator(const Any& container) {
    if (container.IsNull()) {
        throw std::runtime_error("Cannot iterate over null");
    }
    // TODO
    throw std::runtime_error("Unsupported container type");
  }

  static bool IsEqual(const Iterator& u, const Iterator& v) {
    while (true) {
        bool u_has_next = false;
        bool v_has_next = false;

        McValue u_value;
        McValue v_value;

        try {
            u_value = u.Next(&u_has_next);
            v_value = v.Next(&v_has_next);
        } catch (...) {
            return false;
        }

        if (!u_has_next && !v_has_next) {
            return true;
        }

        if (u_has_next != v_has_next) {
            return false;
        }

        if (!(u_value == v_value)) {
            return false;
        }
    }
  }

  IteratorNode* GetMutableNode() const {
    return static_cast<IteratorNode*>(data_.get());
  }

  friend std::ostream& operator<<(std::ostream& os, const Iterator& iter);
};

template<typename T>
struct TypeTraits {
    static constexpr int32_t index = TypeIndex::Null;
};

template<> 
struct TypeTraits<Iterator> {
    static constexpr int32_t index = TypeIndex::RuntimeIterator;
};

template<>
inline bool Any::Is<Iterator>() const noexcept {
    return T() == TypeIndex::RuntimeIterator;
}

template<>
inline Iterator Any::As<Iterator>() const {
    if (!Is<Iterator>()) {
        throw std::runtime_error("Type mismatch in conversion to Iterator");
    }
    return Iterator(object_p<object_t>(static_cast<object_t*>(value_.u.v_pointer)));
}

inline std::ostream& operator<<(std::ostream& os, const Iterator& iter) {
    os << "Iterator@" << iter.GetMutableNode();
    return os;
}

bool operator==(const McValue& lhs, const McValue& rhs);


} // namespace runtime

} // namespace mc