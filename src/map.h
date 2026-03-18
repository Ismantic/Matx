#pragma once 

#include <unordered_map>
#include <initializer_list>
#include <iterator>

#include "object.h"

namespace mc {
namespace runtime {

class MapNode : public object_t {
public:
    static constexpr const int32_t INDEX = TypeIndex::RuntimeMap;
    static constexpr const std::string_view NAME = "Map";
    DEFINE_TYPEINDEX(MapNode, object_t);

    using key_type = object_r;
    using mapped_type = object_r;
    using value_type = std::pair<key_type, mapped_type>;
    using container_type = std::unordered_map<key_type, mapped_type, object_s, object_e>;
    using const_iterator = typename container_type::const_iterator;

    size_t size() const {
        return data_.size();
    }

    size_t count(const key_type& key) const {
        return data_.count(key);
    }

    const mapped_type& at(const key_type& key) const {
        auto it = data_.find(key);
        if (it == data_.end()) {
            throw std::out_of_range("Key not found in Map");
        }
        return it->second;
    }

    mapped_type& at(const key_type& key) {
        auto it = data_.find(key);
        if (it == data_.end()) {
            throw std::out_of_range("Key not found in Map");
        }
        return it->second;
    }

    void set(const key_type& key, const mapped_type& value) {
        data_[key] = value;
    }

    void set(key_type&& key, mapped_type&& value) {
        data_[std::move(key)] = std::move(value);
    }

    mapped_type& operator[](const key_type& key) {
        return data_[key];
    }


    void erase(const key_type& key) {
        data_.erase(key);
    }

    const_iterator begin() const { return data_.begin(); }
    const_iterator end() const { return data_.end(); }
    const_iterator find(const key_type& key) const { return data_.find(key); }

private:
    container_type data_;
};

template<typename K, typename V>
class Map : public object_r {
public:
    using key_type = K;
    using mapped_type = V;
    using node_type = MapNode;

    explicit Map(object_p<object_t> obj) : object_r(std::move(obj)) {
        if (data_ && !data_->IsType<MapNode>()) {
            throw std::runtime_error("Type mismatch: expected Map");
        }
    }

    Map() {
        data_ = MakeObject<MapNode>();
    }

    Map(std::initializer_list<std::pair<K, V>> init) {
        data_ = MakeObject<MapNode>();
        for (const auto& [key, value] : init) {
            set(key, value);
        }
    }

    template <typename Hash = object_s, typename Equal = object_e>
    explicit Map(const std::unordered_map<K, V, Hash, Equal>& init) {
        data_ = MakeObject<MapNode>();
        for (const auto& [key, value] : init) {
            set(key, value);
        }
    }

    template <typename Hash = object_s, typename Equal = object_e>
    explicit Map(std::unordered_map<K, V, Hash, Equal>&& init) {
        data_ = MakeObject<MapNode>();
        for (auto&& [key, value] : init) {
            set(std::move(key), std::move(value));
        }
    }

    size_t size() const {
        return Get()->size();
    }

    bool empty() const {
        return size() == 0;
    }

    size_t count(const K& key) const {
        return Get()->count(key);
    }

    V at(const K& key) const {
        return Downcast<V>(Get()->at(key));
    }

    void set(const K& key, const V& value) {
        Get()->set(key, value);
    }

    void set(K&& key, V&& value) {  
        Get()->set(std::move(key), std::move(value));
    }

    V operator[](const K& key) const {
        auto* node = Get();
        auto it = node->find(key);
        if (it == node->end()) {
            return V();  
        }
        return Downcast<V>(it->second);
    }

    void erase(const K& key) {
        Get()->erase(key);
    }

    class const_iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = std::pair<K, V>;
        using difference_type = std::ptrdiff_t;
        using pointer = const value_type*;
        using reference = const value_type;

        const_iterator(node_type::const_iterator it) : it_(it) {}

        value_type operator*() const {
            return std::make_pair(
                Downcast<K>(it_->first),
                Downcast<V>(it_->second)
            );
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
    const_iterator find(const K& key) const { return const_iterator(Get()->find(key)); }

private:
    MapNode* Get() const {
        return static_cast<MapNode*>(const_cast<object_t*>(get()));
    }
};

} // namespace runtime
} // namespace mc