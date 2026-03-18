#pragma once 

#include <vector>
#include <initializer_list>
#include <iterator>

#include "object.h"

namespace mc {
namespace runtime {

class ArrayNode : public object_t {
public:
    static constexpr const int32_t INDEX = TypeIndex::RuntimeArray;
    static constexpr const std::string_view NAME = "Array";
    DEFINE_TYPEINDEX(ArrayNode, object_t);

    using value_type = object_r;
    using container_type = std::vector<value_type>;
    using const_iterator = typename container_type::const_iterator;

    void push_back(object_r value) {
        data_.push_back(std::move(value));
    }

    size_t size() const {
        return data_.size();
    }

    size_t capacity() const {
        return data_.capacity();
    }

    const object_r& operator[](size_t i) const {
        return data_[i];
    }

    const_iterator begin() const { return data_.begin(); }
    const_iterator end() const { return data_.end(); }

    void clear() {
        for (size_t i = 0; i < data_.size(); ++i) {
            data_[i].object_r::~object_r();
        }
        data_.clear();
    }

    object_r* MutableBegin() {
        return data_.data();
    }

    static object_p<ArrayNode> Empty(int64_t size) {
        object_p<ArrayNode> p = MakeObject<ArrayNode>();
        if (size > 0) {
            p->data_.resize(size);  
        }
        return p;
    }

private:
    std::vector<object_r> data_;
};

template<typename T>
class Array : public object_r {
public:
    using value_type = T;
    using node_type = ArrayNode;

    explicit Array(object_p<object_t> obj) : object_r(std::move(obj)) {
        if (data_ && !data_->IsType<ArrayNode>()) {
            throw std::runtime_error("Type mismatch: expected Array");
        }
    }

    Array() {
        data_ = MakeObject<ArrayNode>();
    }

    Array(std::initializer_list<T> init) {
        Assign(init.begin(), init.end());
    }

    Array(const std::vector<T>& init) {
        Assign(init.begin(), init.end());
    }

    template <typename IterType>
    void Assign(IterType first, IterType last) {
        int64_t cap = std::distance(first, last);

        ArrayNode* p = Get();
        if (p != nullptr && data_.unique() && p->capacity() >= cap) {
            // do not have to make new space
            p->clear();
        } else {
            // create new space
            data_ = ArrayNode::Empty(cap);
            p = Get();
        }
        
        object_r* itr = p->MutableBegin();
        for (int64_t i = 0; i < cap; ++i, ++first, ++itr) {
            new (itr) object_r(*first);
        }
    }

    void push_back(const T& value) {
        Get()->push_back(value);
    }

    T operator[](size_t i) const {
        return Downcast<T>(Get()->operator[](i));
    }

    size_t size() const {
        return Get()->size();
    }

    class const_iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = const T*;
        using reference = const T;

        const_iterator(node_type::const_iterator it) : it_(it) {}

        value_type operator*() const { 
            return Downcast<T>(*it_);
        }

        const_iterator& operator++() {
            ++it_;
            return *this;
        }

        const_iterator operator++(int) {
            const_iterator tmp = *this;
            ++it_;
            return tmp;
        }

        bool operator==(const const_iterator& other) const {
            return it_ == other.it_;
        }

        bool operator!=(const const_iterator& other) const {
            return it_ != other.it_;
        }

    private:
        node_type::const_iterator it_;
    };

    const_iterator begin() const { return const_iterator(Get()->begin()); }
    const_iterator end() const { return const_iterator(Get()->end()); }

private:
    ArrayNode* Get() const {
        return static_cast<ArrayNode*>(const_cast<object_t*>(get()));
    }
};

} // namespace runtime
} // namespace mc