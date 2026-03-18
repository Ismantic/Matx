#include "object.h"
#include "debug_log.h"

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <atomic>
#include <mutex>

#include <assert.h>


namespace mc {
namespace runtime {

object_t::~object_t() = default;

struct TypeData {
    int32_t t{0};

    int32_t p{0};

    std::string name;
};

class TypeContext {
public:
    bool IsFrom(int32_t t, int32_t p) {
        if (t < p) 
            return false;
        if (t == p)
            return true;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            assert(t < vec_.size());
            while ( t > p) {
                t = vec_[t].p;
            }
            return t == p;
        }
    }
    int32_t GetOrAllocRuntimeTypeIndex(const std::string_view& n, int32_t t, int32_t p) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = names_.find(n);
        if (it != names_.end()) {
            return it->second;
        }

        assert(p < vec_.size());
        TypeData& pa = vec_[p];
        assert(pa.t == p);

        int32_t i;

        if (t != TypeIndex::Dynamic) {
            MC_DLOG_STREAM(std::cout << "TypeIndex[" << t << "]: static: " << n
                                     << ", parent " << vec_[p].name << std::endl);
            i = t;
            assert(t < vec_.size());
        } else {
            MC_DLOG_STREAM(std::cout << "TypeIndex[" << counter_ << "]: dynamic: " << n
                                     << ", parent " << vec_[p].name << std::endl);
            i = counter_++;
            assert(vec_.size() <= counter_);
            vec_.resize(counter_, TypeData());
        }

        vec_[i].t = i;
        vec_[i].p = p;
        vec_[i].name = n;

        names_[n] = i;
        return i;
    }

    std::string Name(int32_t i) {
        std::lock_guard<std::mutex> lock(mutex_);
        assert(i < vec_.size());
        return vec_[i].name;
    }

    int32_t Index(const std::string_view& name) {
        auto it = names_.find(name);
        if (it != names_.end()) {
            return it->second;
        }
        throw std::runtime_error("not exits");
    }

    static TypeContext* Global() {
        static TypeContext inst;
        return &inst;
    }

private:
    TypeContext() {
        vec_.resize(TypeIndex::Dynamic, TypeData());
        vec_[0].name = "Object";
    }

    std::mutex mutex_;

    std::atomic<uint32_t> counter_{TypeIndex::Dynamic};

    std::vector<TypeData> vec_;

    std::unordered_map<std::string_view, int32_t> names_;
};

int32_t object_t::GetOrAllocRuntimeTypeIndex(const std::string_view& n, int32_t t, int32_t p) {
  return TypeContext::Global()->GetOrAllocRuntimeTypeIndex(n, t, p);
}

bool object_t::IsFrom(int32_t p) const {
    return TypeContext::Global()->IsFrom(t_, p);
}


std::string object_t::Name() const {
    return TypeContext::Global()->Name(t_);
}

int32_t GetIndex(const std::string_view& name) {
    return TypeContext::Global()->Index(name);
}

} // namespace runtime
} // namespace mc
