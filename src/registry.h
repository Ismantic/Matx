#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <memory>
#include <vector>
#include <tuple>

#include "runtime_value.h"
#include "parameters.h"

namespace mc {
namespace runtime {

using Function = std::function<McValue(Parameters)>;

class FunctionRegistry;

class FunctionRegistry {
public:

  struct FunctionData {
    Function func_;
    std::string_view name_;
    std::string doc_;

    template <typename F>
    FunctionData& SetBody(F&& func);

    FunctionData& set(Function func) {
      func_ = std::move(func);
      return *this;
    }
  };

public:
  static FunctionData& Register(std::string_view name) {
    auto& instance = Instance();
    std::lock_guard<std::mutex> lock(instance.mutex_);

    auto it = instance.functions_.find(name);
    if (it != instance.functions_.end()) {
        throw std::runtime_error(
                std::string("Function already registered: ")+std::string(name));
        return it->second;
    }

    return instance.functions_.emplace(
                name, FunctionData{nullptr, name}).first->second;
  }

  template<typename F>
  static FunctionData& SetFunction(std::string_view name, 
                                   F&& func) {
        auto& data = Register(name);
        data.func_ = std::forward<F>(func);
        return data;
  }

  static Function* Get(std::string_view name) {
    auto& instance = Instance();
    std::lock_guard<std::mutex> lock(instance.mutex_);

    auto it = instance.functions_.find(name);
    return it != instance.functions_.end() ?
                    &it->second.func_ : nullptr;
  }

  static bool Remove(std::string_view name) {
    auto& instance = Instance();
    std::lock_guard<std::mutex> lock(instance.mutex_);
    return instance.functions_.erase(name) > 0;
  }

  static std::vector<std::string_view> ListNames() {
    auto& instance = Instance();
    std::lock_guard<std::mutex> lock(instance.mutex_);

    std::vector<std::string_view> names;
    names.reserve(instance.functions_.size());
    for (const auto& [name, _] : instance.functions_) {
        names.push_back(name);
    }
    return names;
  }

private:
  static FunctionRegistry& Instance() {
    //static std::unique_ptr<FunctionRegistry> instance = 
    //               std::make_unique<FunctionRegistry>();
    static FunctionRegistry* instance = new FunctionRegistry();
    return *instance;
  }

  FunctionRegistry() = default;
  FunctionRegistry(const FunctionRegistry&) = delete;
  FunctionRegistry& operator=(const FunctionRegistry&) = delete;

  std::mutex mutex_;
  std::unordered_map<std::string_view, FunctionData> functions_;
};

// Function traits implementation
template<typename F>
struct FunctionTraits;

// Specialization for regular functions
template<typename R, typename... Args>
struct FunctionTraits<R(Args...)> {
    static constexpr size_t arity = sizeof...(Args);
    using result_type = R;
    template<size_t I>
    struct arg {
        using type = typename std::tuple_element<I, std::tuple<Args...>>::type;
    };
};

// Specialization for lambda expressions
template<typename F>
struct FunctionTraits : public FunctionTraits<decltype(&F::operator())> {};

template<typename C, typename R, typename... Args>
struct FunctionTraits<R(C::*)(Args...) const> {
    static constexpr size_t arity = sizeof...(Args);
    using result_type = R;
    template<size_t I>
    struct arg {
        using type = typename std::remove_cv_t<
            typename std::remove_reference_t<
                typename std::tuple_element<I, std::tuple<Args...>>::type
            >
        >;
    };
};

class FunctionWrapper {
public:
    using FuncType = std::function<McValue(Parameters)>;
    
    template<typename R, typename... Args>
    static FuncType Create(R (*func)(Args...)) {
        return [func](Parameters ps) -> McValue {
            if (ps.size() < sizeof...(Args)) {
                throw std::runtime_error("Not enough parameters");
            }
            return CallImpl(func, ps, std::make_index_sequence<sizeof...(Args)>{});
        };
    }

    template<typename F, typename = std::enable_if_t<std::is_class_v<std::remove_reference_t<F>>>>
    static FuncType Create(F&& f) {
        return [f = std::forward<F>(f)](Parameters ps) -> McValue {
            return CallLambda(f, ps);
        };
    }

private:
    template<typename R, typename... Args, size_t... Is>
    static McValue CallImpl(R (*func)(Args...), 
                          const Parameters& ps, 
                          std::index_sequence<Is...>) {
        return McValue(func(ConvertArg<Args>(ps[Is])...));
    }

template<typename F>
static McValue CallLambda(F& f, const Parameters& ps) {
    using traits = FunctionTraits<F>;
    if (ps.size() < traits::arity) {
        throw std::runtime_error("Not enough parameters");
    }
    return CallLambdaImpl(f, ps, std::make_index_sequence<traits::arity>{});
}

template<typename F, size_t... Is>
static McValue CallLambdaImpl(F& f, const Parameters& ps, std::index_sequence<Is...>) {
    using traits = FunctionTraits<F>;
    return f(ConvertArg<typename traits::template arg<Is>::type>(ps[Is])...);
}

    template<typename T>
    static auto ConvertArg(const Any& value) {
        using clean_type = std::remove_cv_t<std::remove_reference_t<T>>;
        
        if constexpr (std::is_same_v<clean_type, McValue>) {
            return McValue(value);
        }
        else if constexpr (std::is_same_v<clean_type, Object>) {
            if (!value.IsObject()) {
                throw std::runtime_error("Expected object type");
            }
            return AsObject(value);
        } 
        else if constexpr (std::is_base_of_v<object_r, clean_type>) {
            if (!value.IsObject()) {
                throw std::runtime_error("Expected object type");
            }
            return value.As<clean_type>();
        } 
        else if constexpr (std::is_integral_v<clean_type>) {
            if (!value.IsInt()) {
                throw std::runtime_error("Expected integer type");
            }
            return static_cast<clean_type>(value.As<int64_t>());
        }
        else if constexpr (std::is_floating_point_v<clean_type>) {
            if (!value.IsFloat() && !value.IsInt()) {
                throw std::runtime_error("Expected floating point type");
            }
            return static_cast<clean_type>(value.As<double>());
        }
        else if constexpr (std::is_same_v<clean_type, std::string>) {
            if (!value.IsStr()) {
                throw std::runtime_error("Expected string type");
            }
            return std::string(value.As<const char*>());
        }
        else if constexpr (std::is_same_v<clean_type, DataType>) {
            if (!value.IsDataType()) {
                throw std::runtime_error("Expected DataType");
            }
            return value.As<DataType>();
        }
        else {
            throw std::runtime_error("Unsupported parameter type");
        }
    }
};



template<typename F>
FunctionRegistry::FunctionData& FunctionRegistry::FunctionData::SetBody(F&& func) {
    return set(FunctionWrapper::Create(std::forward<F>(func)));

}

#define STR_CONCAT_IMPL(x, y) x##y
#define STR_CONCAT(x, y) STR_CONCAT_IMPL(x, y)

#define REGISTER_GLOBAL(Name) \
    static auto& STR_CONCAT(__reg_global_, __COUNTER__) = \
        mc::runtime::FunctionRegistry::Register(Name)

#define REGISTER_FUNCTION(Name, Func) \
    static auto& STR_CONCAT(__reg_global_, __COUNTER__) = \
        mc::runtime::FunctionRegistry::SetFunction(Name, Func)

} // namespace runtime
} // namespace mc
