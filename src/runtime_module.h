#pragma once

#include <string_view>

#include "object.h"
#include "registry.h"
#include "c_api.h"

namespace mc {
namespace runtime {

namespace Symbol {
constexpr const char* ModuleCtx = "__mc_module_ctx";
constexpr const char* FuncArray = "__mc_func_array__";
constexpr const char* FuncRegistry = "__mc_func_registry__";
constexpr const char* ClosuresNames = "__mc_closures_names__";
} // namespace Symbol

class ModuleNode : public object_t {
public:
    static constexpr const std::string_view NAME = "runtime.Module";
    static constexpr const uint32_t INDEX = TypeIndex::Module;
    DEFINE_TYPEINDEX(ModuleNode, object_t);

    virtual ~ModuleNode() = default;

    virtual Function GetFunction(const std::string_view& name,
                                 const object_p<object_t>& ss) = 0;
    
    Function GetFunction(const std::string_view& name,
                         bool use_imports = false);

};

class Module : public object_r {
public:
    Module() = default;

    explicit Module(object_p<object_t> n) : object_r(std::move(n)) {}

    ModuleNode* operator->() {
        return static_cast<ModuleNode*>(get_mutable());
    }

    const ModuleNode* operator->() const {
        return static_cast<const ModuleNode*>(get());
    }

    Function GetFunction(const std::string_view& name,
                         const object_p<object_t>& ss) {
        return operator->()->GetFunction(name, ss);
    }

    static Module Load(const std::string& filename);
    
    using ContainerType = ModuleNode;

};


class Library : public object_t {
public:
    virtual ~Library() {}

    virtual void* GetSymbol(std::string_view name) = 0; 
};

Module CreateModuleFromLibrary(object_p<Library> b);


inline Function WrapFunction(BackendFunc func,
                           const object_p<object_t>& sptr_to_self,
                           bool capture_resource = false) {
  if (capture_resource) {
    return Function([func, sptr_to_self](Parameters args) -> McValue {
      if (args.size() <= 0) {
        throw std::runtime_error("Resource handle required but not provided");
      }

      void* handle = args[args.size() - 1].As<void*>();
      
      std::vector<Value> c_args;
      c_args.reserve(args.size() - 1);
      
      for (int i = 0; i < args.size() - 1; ++i) {
        c_args.push_back(args[i].value());
      }

      Value ret_val;
      if (int ret = (*func)(c_args.data(), c_args.size(), &ret_val, handle); ret != 0) {
        throw std::runtime_error(GetError());
      }
      
      return McValue(McView(&ret_val));
    });
  } else {
    return Function([func, sptr_to_self](Parameters args) -> McValue {
      std::vector<Value> c_args;
      c_args.reserve(args.size());
      for (int i = 0; i < args.size(); ++i) {
        c_args.push_back(args[i].value());
      }

      Value ret_val;
      if (int ret = (*func)(c_args.data(), c_args.size(), &ret_val, nullptr); ret != 0) {
        throw std::runtime_error(GetError());
      }
      
      return McValue(McView(&ret_val));
    });
  }
}

template<typename R = McValue>
class TypeFunction {
 public:
  template<typename F>
  explicit TypeFunction(F&& func) : func_(std::forward<F>(func)) {}
  
  Function Packed() const {
    return Function([f = func_](Parameters args) -> McValue {
      if constexpr (std::is_same_v<R, void*>) {
        void* ptr = f();
        Value v;
        v.t = TypeIndex::Func;
        v.u.v_pointer = ptr;
        return McValue(McView(&v));
      } else if constexpr (std::is_same_v<R, McValue>) {
        return f();
      } else {
        return McValue(f());
      }
    });
  }

 private:
  std::function<R()> func_;
};

} // namespace runtime
} // namespace mc