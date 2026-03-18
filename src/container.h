#pragma once

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <initializer_list>
#include <stdint.h>

#include "object.h"
#include "runtime_value.h"
#include "iterator.h"

namespace mc {
namespace runtime {

class Dict;
class DictNode;
class DictIterator;

template <class I>
struct item_iterator_ : public I {
  using value_type = typename I::value_type;
  using pointer = value_type*;
  using reference = value_type&;

  item_iterator_() = default;
  item_iterator_(const I& i) : I(i) {}

  value_type operator*() const {
    return I::operator*();
  }
};

template <class I>
struct key_iterator_ : public I {
  using value_type = typename I::value_type::first_type;
  using pointer = value_type*;
  using reference = value_type&;

  key_iterator_() = default;
  key_iterator_(const I& i) : I(i) {}

  value_type operator*() const {
    return (*this)->first;
  }
};

template <class I>
struct value_iterator_ : public I {
  using value_type = typename I::value_type::second_type;
  using pointer = value_type*;
  using reference = value_type&;

  value_iterator_() = default;
  value_iterator_(const I& i) : I(i) {}

  value_type operator*() const {
    return (*this)->second;
  }
};

template <typename D>
struct DictItems {
  using iterator = typename D::item_const_iterator;
  using value_type = typename iterator::value_type;
  D data;

  DictItems(const D& data) : data(data) {}

  iterator begin() const { return data.item_begin(); }
  iterator end() const { return data.item_end(); }
  int64_t  size() const { return data.size(); }
};

template <typename D>
struct DictKeys {
  using iterator = typename D::key_const_iterator;
  using value_type = typename iterator::value_type;
  D data;

  DictKeys(const D& data) : data(data) {}

  iterator begin() const { return data.key_begin(); }
  iterator end() const { return data.key_end(); }
  int64_t  size() const { return data.size(); }
};

template <typename D>
struct DictValues {
  using iterator = typename D::value_const_iterator;
  using value_type = typename iterator::value_type;
  D data;

  DictValues(const D& data) : data(data) {}

  iterator begin() const { return data.value_begin(); }
  iterator end() const { return data.value_end(); }
  int64_t  size() const { return data.size(); } 
};

class DictNode : public object_t {
public:
  static constexpr const int32_t INDEX = TypeIndex::RuntimeDict;
  static constexpr const std::string_view NAME = "Dict";
  DEFINE_TYPEINDEX(DictNode, object_t);

  using container_type = std::unordered_map<McValue, McValue>;
  using iterator = typename container_type::iterator;
  using const_iterator = typename container_type::const_iterator;
  using value_type = typename container_type::value_type;
  using key_type = typename container_type::key_type;
  using mapped_type = typename container_type::mapped_type;

  DictNode() = default;


  template <typename Iterator>
  DictNode(Iterator s, Iterator e) {
    for (; s != e; ++s) {
        data_.emplace(s->first, s->second);
    }
  }

  mapped_type& operator[](const key_type& key) {
    return data_[key];
  }

  const_iterator find(const key_type& key) const {
    return data_.find(key);
  }

  iterator find(const key_type& key) {
    return data_.find(key);
  }

  bool contains(const key_type& key) const {
    return find(key) != data_.end();
  }

  size_t size() const {
    return data_.size();
  }

  bool empty() const {
    return data_.empty();
  }

  void clear() {
    data_.clear();
  }

  iterator begin() { return data_.begin(); }
  const_iterator begin() const { return data_.begin(); }
  iterator end() { return data_.end(); }
  const_iterator end() const { return data_.end(); }

private:
  container_type data_;
  friend class Dict;
};

class Dict : public object_r {
public:
  using container_type = DictNode::container_type;
  using value_type = typename container_type::value_type;
  using iterator = typename container_type::iterator;
  using const_iterator = typename container_type::const_iterator;
  using item_iterator = item_iterator_<iterator>;
  using item_const_iterator = item_iterator_<const_iterator>;
  using key_iterator = key_iterator_<iterator>;
  using key_const_iterator = key_iterator_<const_iterator>;
  using value_iterator = value_iterator_<iterator>;
  using value_const_iterator = value_iterator_<const_iterator>;

  Dict() : object_r(MakeObject<DictNode>()) {}

  explicit Dict(object_p<object_t> n) : object_r(std::move(n)) {}


  Dict(std::initializer_list<value_type> init)
    : object_r(MakeObject<DictNode>(init.begin(), init.end())) {}

  template<typename Iterator>
  Dict(Iterator s, Iterator e)
    : object_r(MakeObject<DictNode>(s, e)) {}

  Dict(const std::vector<value_type>& init) 
    : object_r(MakeObject<DictNode>(init.begin(), init.end())) {}

  iterator begin() const { return Get()->data_.begin(); }
  iterator end() const { return Get()->data_.end(); }
  iterator item_begin() const { return begin(); }
  iterator item_end() const { return end(); }
  
  void insert(const McValue& key, const McValue& value) {
    Get()->data_[key] = value;
  }

  void clear() const {
    if (auto node = Get()) {
      node->clear();
    }
  }

  bool contains(const McValue& key) const {
    return Get()->data_.find(key) != Get()->data_.end();
  }

  McValue& operator[](const McValue& key) {
    return Get()->data_[key];
  }

//  Iterator iter() const;

private:
  DictNode* Get() const {
    return static_cast<DictNode*>(data_.get());
  }

};

// class DictIteratorNode : public IteratorNode {
// public:
//   DictIteratorNode(Dict container, Dict::iterator s, Dict::iterator e)
//     : container_(std::move(container)), s_(s), e_(e) {}

//   bool HasNext() const override {
//     return s_ != e_;
//   }

//   McValue Next() override {
//     return *(s_++);
//   }

//   McValue Next(bool* has_next) override {
//     auto value = *(s_++);
//     *has_next = (s_ != e_);
//     return value;
//   }

//   McView NextView(bool* has_next, McValue* holder_or_null) override {
//     *holder_or_null = *(s_++);
//     *has_next = (s_ != e_);
//     return *holder_or_null;
//   }

//   int64_t Distance() const override {
//     return std::distance(s_, e_);
//   }  

// private:
//   Dict container_;
//   Dict::iterator s_;
//   Dict::iterator e_;
// };


//template<typename... Gs>
//Dict MakeDict(Gs&&... gs) {
//    return Dict{std::forward<Gs>(gs)...};
//}

// Iterator Dict::iter() const {
//     auto node = MakeObject<DictIteratorNode>(
//         *this,
//         this->begin(),
//         this->end());
//     return Iterator(std::move(node));
// }

class SetNode : public object_t {
public:
  static const int32_t INDEX = TypeIndex::RuntimeSet;
  static constexpr const std::string_view NAME = "Set";
  DEFINE_TYPEINDEX(SetNode, object_t);

  using value_type = McValue;
  using container_type = std::unordered_set<value_type>;
  using iterator = typename container_type::const_iterator;
  using const_iterator = iterator;

  SetNode() = default;

  template <typename Iterator>
  SetNode(Iterator s, Iterator e) {
    for (auto it = s; it != e; ++it) {
      data_.insert(*it);
    }
  }

  SetNode(std::initializer_list<value_type> init)
    : data_(init) { }

    iterator begin() { 
        return data_.begin(); 
    }
    
    const_iterator begin() const { 
        return data_.begin(); 
    }
    
    iterator end() { 
        return data_.end(); 
    }
    
    const_iterator end() const { 
        return data_.end(); 
    }

    bool empty() const { 
        return data_.empty(); 
    }
    
    size_t size() const { 
        return data_.size(); 
    }

    void clear() { 
        data_.clear(); 
    }

    bool insert(const value_type& value) { 
        return data_.insert(value).second; 
    }
    
    bool insert(value_type&& value) { 
        return data_.insert(std::move(value)).second; 
    }

    size_t erase(const value_type& value) { 
        return data_.erase(value); 
    }

    bool contains(const value_type& value) const { 
        return data_.find(value) != data_.end(); 
    }

private:
  container_type data_;
};

class Set : public object_r {
public:
  using Node = SetNode;
  using value_type = Node::value_type;
  using const_iterator = Node::const_iterator;

  Set() = default;

  explicit Set(object_p<object_t> n) : object_r(std::move(n)) {}

  template<typename Iterator>
  Set(Iterator s, Iterator e) {
    data_ = MakeObject<Node>(s, e);
  }

  Set(std::initializer_list<value_type> init) {
    data_ = MakeObject<Node>(init);
  }

    // iterators
    const_iterator begin() const { 
        return Get()->begin();
    }
    
    const_iterator end() const { 
        return Get()->end();
    }

    // capacity
    bool empty() const { 
        if (!get()) return true;
        return Get()->empty();
    }
    
    size_t size() const { 
        if (!get()) return 0;
        return Get()->size();
    }

    // modifiers
    void clear() const { 
        if (Node* node = Get()) {
            node->clear();
        }
    }

    size_t erase(const value_type& value) const {
        if (Node* node = Get()) {
            return node->erase(value);
        }
        return 0;
    }

    bool insert(const value_type& value) const { 
        if (Node* node = Get()) {
            return node->insert(value);
        }
        return false;
    }

    template<typename T>
    bool insert(const T& value) const {
        return insert(value_type(value));
    }

    bool contains(const value_type& value) const {
        if (const Node* node = Get()) {
            return node->contains(value);
        }
        return false;
    }

    template<typename T>
    bool contains(const T& value) const {
        return contains(value_type(value));
    }

    Set set_union(const Set& other) const {
        auto result = Set();
        if (empty()) {
            result = other;
        } else if (!other.empty()) {
            result = *this;
            for (const auto& item : other) {
                result.insert(item);
            }
        }
        return result;
    }

    Set set_minus(const Set& other) const {
        auto result = Set();
        if (!empty()) {
            for (const auto& item : *this) {
                if (!other.contains(item)) {
                    result.insert(item);
                }
            }
        }
        return result;
    }

private:
  Node* Get() const {
      return static_cast<Node*>(const_cast<object_t*>(get()));
  }
};

class ListNode : public object_t {
public:
  static constexpr uint32_t INDEX = TypeIndex::RuntimeList;
  static constexpr const std::string_view NAME = "List";
  DEFINE_TYPEINDEX(ListNode, object_t);

  using value_type = McValue;
  using container_type = std::vector<value_type>;
  using iterator = typename container_type::iterator;
  using const_iterator = typename container_type::const_iterator;

  ListNode() = default;

  ListNode(std::initializer_list<value_type> init)
    : data_(init) { }
  
  template <typename Iterator>
  ListNode(Iterator s, Iterator e)
    : data_(s, e) { }
  
  ListNode(size_t n, const value_type& value) 
    : data_(n, value) { }
  
    // iterators
    iterator begin() { 
        return data_.begin(); 
    }
    
    const_iterator begin() const { 
        return data_.begin(); 
    }
    
    iterator end() { 
        return data_.end(); 
    }
    
    const_iterator end() const { 
        return data_.end(); 
    }

    // element access
    value_type& operator[](int64_t i) {
        return data_[i];
    }
    
    const value_type& operator[](int64_t i) const {
        return data_[i];
    }

    // capacity
    size_t size() const { 
        return data_.size(); 
    }
    
    bool empty() const { 
        return data_.empty(); 
    }

    void reserve(size_t new_size) {
        data_.reserve(new_size);
    }

    void push_back(const value_type& value) {
        data_.push_back(value);
    }
    
    void push_back(value_type&& value) {
        data_.push_back(std::move(value));
    }

    template<typename T>
    void append(T&& value) {
        push_back(value_type(std::forward<T>(value)));
    }

    void pop_back() {
        data_.pop_back();
    }

    void clear() {
        data_.clear();
    }

    value_type& at(int64_t i) {
        if (i < 0) {
            i += data_.size();
        }
        return data_.at(i);
    }

private:
  container_type data_;
};

class List : public object_r {
public:
  using Node = ListNode;
  using value_type = Node::value_type;
  using iterator = Node::iterator;
  using const_iterator = Node::const_iterator;

  List() = default;

  explicit List(object_p<object_t> n) : object_r(std::move(n)) {}

  List(std::initializer_list<value_type> init) {
    data_ = MakeObject<Node>(init);
  }

  template<typename Iterator>
  List(Iterator s, Iterator e) {
    data_ = MakeObject<Node>(s, e);
  }

  List(size_t n, const value_type& value) {
    data_ = MakeObject<Node>(n, value);
  }

  value_type& operator[](int64_t i) const {
    return Get()->at(i);
  }

  size_t size() const {
    auto node = Get();
    return node ? node->size() : 0;
  }

  bool empty() const {
    auto node = Get();
    return node ? node->empty() : true;
  }

    void push_back(const value_type& val) const {
        if (auto node = Get()) {
            node->push_back(val);
        }
    }

    template<typename T>
    void append(T&& val) const {
        if (auto node = Get()) {
            node->append(std::forward<T>(val));
        }
    }

    void pop_back() const {
        if (auto node = Get()) {
            node->pop_back();
        }
    }

    void clear() const {
        if (auto node = Get()) {
            node->clear();
        }
    }

    iterator begin() const { 
        return Get()->begin(); 
    }
    
    iterator end() const { 
        return Get()->end(); 
    }

private:
  Node* Get() const {
    return static_cast<Node*>(const_cast<object_t*>(get()));
  }
};

class TupleNode : public object_t {
public:
  static constexpr const int32_t INDEX = TypeIndex::RuntimeTuple;
  static constexpr const std::string_view NAME = "Tuple";
  DEFINE_TYPEINDEX(TupleNode, object_t);

  using value_type = McValue;
  using iterator = const value_type*;
  using const_iterator = const value_type*;

  TupleNode(const value_type* s, const value_type* e)
    : size_(e - s) {
      data_ = new value_type[size_];
      for (size_t i = 0; i < size_; ++i) {
        data_[i] = s[i];
      }
  }

  ~TupleNode() {
      delete[] data_;
  }

  size_t size() const noexcept { return size_; }
  const value_type* data() const noexcept { return data_; }
  value_type* data() noexcept { return data_; }

  const_iterator begin() const noexcept { return data_; }
  const_iterator end() const noexcept { return data_ + size_; }

private:
  size_t size_;
  value_type* data_;
};

class Tuple : public object_r {
public:
  using Node = TupleNode;
  using value_type = Node::value_type;
  using iterator = Node::const_iterator;
  using const_iterator = Node::const_iterator;

  Tuple() = default;

  explicit Tuple(object_p<object_t> n) : object_r(std::move(n)) {}

  template<typename Iterator>
  Tuple(Iterator s, Iterator e) {
    std::vector<value_type> t;
    std::copy(s, e, std::back_inserter(t));
    data_ = MakeObject<Node>(t.data(), t.data()+t.size());
  }

  Tuple(std::initializer_list<value_type> init) 
    : Tuple(init.begin(), init.end()) {}
  
  const value_type& operator[] (size_t i) const {
    auto node = static_cast<const Node*>(get());
    return node->data()[i];
  }

  size_t size() const noexcept {
    if (!get()) return 0;
    return static_cast<const Node*>(get())->size();
  }

  const_iterator begin() const noexcept {
    if (!get()) return nullptr;
      return static_cast<const Node*>(get())->begin();
  }

  const_iterator end() const noexcept {
    if (!get()) return nullptr;
    return static_cast<const Node*>(get())->end();
  }

  bool operator==(const Tuple& other) const noexcept {
      if (size() != other.size()) return false;
      return std::equal(begin(), end(), other.begin());
  }  

};

} // namespace runtime

} // namespace mc
