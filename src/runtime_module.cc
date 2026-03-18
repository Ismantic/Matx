#include "runtime_module.h"
#include "debug_log.h"

#include <dlfcn.h>
#include <unordered_map>
#include <unordered_set>

#include <iostream>
#include <stdio.h>

namespace mc {
namespace runtime {


Function ModuleNode::GetFunction(const std::string_view& name,
                                 bool use_imports) {
    ModuleNode* self = this;
    MC_DLOG_STREAM(std::cout << "GetFunction" << std::endl);
    MC_DLOG_STREAM(std::cout << static_cast<void*>(this) << "\n");
    Function pn = self->GetFunction(name, NTcast<object_t>(this));
    MC_DLOG_STREAM(std::cout << "Got Pn" << std::endl);
    if (pn != nullptr) {
        return pn;
    }
    if (use_imports) {
        // TODO: 
    }
    return pn;
}

class DefaultLibray final : public Library {
public:
    ~DefaultLibray() {
        if (pointer_)
            UnLoad();
    }

    void Init(const std::string& name) {
        MC_DLOG_STREAM(std::cout << "Name " << name << std::endl);
        Load(name);
    }

    void* GetSymbol(std::string_view name) final {
        return GetSymbol_(name.data());
    } 

private:
    void* pointer_{nullptr};

    void Load(const std::string& name) {
        MC_DLOG_PRINTF("Attempting to load library: %s\n", name.c_str());
        pointer_ = dlopen(name.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!pointer_) {
            MC_DLOG_STREAM(std::cout << "name " << name << "\n");
            MC_DLOG_PRINTF("Failed to load library: %s\n", dlerror());
            throw std::runtime_error(std::string("Failed to load library: ") + dlerror());
        }
        MC_DLOG_PRINTF("Successfully loaded library: %s at %p\n", name.c_str(), pointer_);
    }
    void UnLoad() {
        dlclose(pointer_);
        pointer_ = nullptr;
    }

    void* GetSymbol_(const char* name) {
        void* sym = dlsym(pointer_, name);
        MC_DLOG_PRINTF("Looking for symbol: %s, found: %p\n", name, sym);
        if (!sym) {
            MC_DLOG_PRINTF("dlsym error: %s\n", dlerror());
        }
        return sym;
    }
};

class SystemLibrary : public Library {
public:
    SystemLibrary() = default;

    static const object_p<SystemLibrary>& Global() {
        static auto inst = MakeObject<SystemLibrary>();
        return inst;
    }

    void* GetSymbol(std::string_view name) final {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = map_.find(name);
        if (it != map_.end()) {
            return it->second;
        } else {
            return nullptr;
        }
    }

    void RegisterSymbol(std::string_view name, void* p) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = map_.find(name);
        if (it != map_.end() && p != it->second) {
            throw std::runtime_error("SystemLib Symbol Overridden");
        }
        map_[name] = p;
        MC_DLOG_STREAM(std::cout << name << " " << map_.size() << std::endl);
    }

private:
    std::mutex mutex_;
    std::unordered_map<std::string_view, void*> map_;
};

static std::vector<std::string_view> ReadFuncRegistryNames(const char* names) {
  std::vector<std::string_view> result;
  if (!names) {
    return result;
  }
  
  uint8_t num_funcs = static_cast<uint8_t>(names[0]);
  
  const char* p = names + 1;
  while (*p && result.size() < num_funcs) {
    size_t len = strlen(p);
    result.push_back(std::string_view(p, len));
    
    p += len + 1;
  }

  return result;
}

class LibraryModuleNode final : public ModuleNode {
public:
    static constexpr const std::string_view NAME = "LibraryModule";
    DEFINE_TYPEINDEX(LibraryModuleNode, ModuleNode); 

    explicit LibraryModuleNode(object_p<Library> p) : p_(std::move(p)) {
        LoadFunctions();
        MC_DLOG_STREAM(std::cout << "New LibraryModuleNode" << std::endl);
        MC_DLOG_STREAM(std::cout << static_cast<void*>(this) << std::endl);
    }

    Function GetFunction(const std::string_view& name,
                         const object_p<object_t>& sptr_to_self) override {
        MC_DLOG_STREAM(std::cout << "LibraryModuleNode GetFunction: " << name << "\n");
        if (name == Symbol::FuncRegistry) {
            auto* func_reg = reinterpret_cast<FunctionRegistry*>(
                p_->GetSymbol(Symbol::FuncRegistry));
            return TypeFunction<void*>([func_reg]() -> void* { 
                        return func_reg; }).Packed();
        }

        // 检查是否为类方法调用格式: "ClassName.method_name"
        std::string fname(name);
        size_t dot_pos = fname.find('.');
        if (dot_pos != std::string::npos) {
            std::string class_name = fname.substr(0, dot_pos);
            std::string method_name = fname.substr(dot_pos + 1);
            
            auto class_it = class_methods_.find(class_name);
            if (class_it != class_methods_.end()) {
                auto method_it = class_it->second.find(method_name);
                if (method_it != class_it->second.end()) {
                    MC_DLOG_STREAM(std::cout << "Found class method: " << class_name << "." << method_name << "\n");
                    return WrapFunction(method_it->second, sptr_to_self, false);
                }
            }
        }

        bool is_contain = closure_names_.find(std::string(name)) != closure_names_.end();

        auto it = func_table_.find(std::string(name));
        if (it != func_table_.end()) {
            return WrapFunction(it->second, sptr_to_self, 
                         is_contain);
        }

        auto faddr = GetBackendFunction(name);
        if (!faddr) {
            return Function();
        }

        return WrapFunction(faddr, sptr_to_self,
                        is_contain);        
    }

private:
    void LoadFunctions() {
        MC_DLOG_PRINTF("Looking for symbol: %s\n", Symbol::FuncRegistry);
        auto* func_reg = reinterpret_cast<FuncRegistry*>(
            p_->GetSymbol(Symbol::FuncRegistry));
        if (!func_reg) {
            MC_DLOG_PRINTF("Failed to find function registry symbol!\n");
            throw std::runtime_error("Missing function registry");
        }
        MC_DLOG_PRINTF("Found function registry at: %p\n", func_reg);

        MC_DLOG_PRINTF("Loading functions from registry:\n");
        MC_DLOG_PRINTF("Registry names: %s\n", func_reg->names);

        auto func_names = ReadFuncRegistryNames(func_reg->names);
        MC_DLOG_PRINTF("Found %zu functions:\n", func_names.size());
        for (const auto& name : func_names) {
            MC_DLOG_PRINTF("  - %s\n", std::string(name).c_str());
        }

        for (size_t i = 0; i < func_names.size(); ++i) {
            std::string fname(func_names[i]);
            MC_DLOG_PRINTF("Registering function: %s at %p\n",
                           fname.c_str(),
                           func_reg->funcs[i]);
            
            // 解析函数名，判断是否为类方法
            auto parsed = ParseFunctionName(fname);
            if (parsed.class_name.empty()) {
                // 全局函数 -> 放入func_table_
                func_table_.emplace(parsed.method_name, func_reg->funcs[i]);
                MC_DLOG_PRINTF("  -> Global function: %s\n", parsed.method_name.c_str());
            } else {
                // 类方法 -> 放入class_methods_
                class_methods_[parsed.class_name][parsed.method_name] = func_reg->funcs[i];
                MC_DLOG_PRINTF("  -> Class method: %s.%s\n",
                               parsed.class_name.c_str(),
                               parsed.method_name.c_str());
            }
        }

        auto* closure_names = reinterpret_cast<const char*>(
            p_->GetSymbol(Symbol::ClosuresNames));
        if (!closure_names) {
            MC_DLOG_PRINTF("Warning: Missing closure names, using empty set\n");
        } else {
            MC_DLOG_PRINTF("Found closure names (binary data, length=%d)\n", (int)closure_names[0]);
            // 简化处理：如果是 "0\000"，则表示没有闭包
            if (closure_names[0] == '0') {
                MC_DLOG_PRINTF("No closures defined\n");
            } else {
                auto names = ReadFuncRegistryNames(closure_names);
                closure_names_.insert(names.begin(), names.end());
            }
        }
    }

    BackendFunc GetBackendFunction(const std::string_view& name) {
        return reinterpret_cast<BackendFunc>(
            p_->GetSymbol(std::string(name).c_str()));
    }

    // 简化的命名解析结构
    struct ParsedName {
        std::string class_name;  // 空字符串表示全局函数
        std::string method_name;
    };
    
    ParsedName ParseFunctionName(const std::string& func_name) {
        ParsedName result;
        
        // 查找第一个__的位置
        size_t pos = func_name.find("__");
        if (pos != std::string::npos) {
            if (pos == 0) {
                // 以__开头 -> 全局函数
                result.class_name = "";  // 空字符串
                result.method_name = func_name.substr(2);  // 去掉前缀__
            } else {
                // 中间有__ -> 类方法
                result.class_name = func_name.substr(0, pos);
                result.method_name = func_name.substr(pos + 2);
            }
        } else {
            // 没有__ -> 也当作全局函数处理
            result.class_name = "";
            result.method_name = func_name;
        }
        
        return result;
    }

    object_p<Library> p_;
    std::unordered_map<std::string, BackendFunc> func_table_;
    std::unordered_set<std::string> closure_names_;
    // 新增：按类分组的方法表
    std::unordered_map<std::string, std::unordered_map<std::string, BackendFunc>> class_methods_;
};

REGISTER_TYPEINDEX(ModuleNode);
REGISTER_TYPEINDEX(LibraryModuleNode);

Module CreateModuleFromLibrary(object_p<Library> p) {
    auto n = MakeObject<LibraryModuleNode>(p);
    Module root = Module(n);
    if (auto* ctx_addr = reinterpret_cast<void**>(
          p->GetSymbol(Symbol::ModuleCtx))) {
        *ctx_addr = root.operator->();
    }
    return root;
}

McValue ModuleLoader(Parameters gs) {
    MC_DLOG_PRINTF("ModuleLoader called with %zu parameters\n", gs.size());
    auto n = MakeObject<DefaultLibray>();
    auto name = gs[0].As<const char*>();
    MC_DLOG_PRINTF("ModuleLoader: loading library '%s'\n", name);
    n->Init(gs[0].As<const char*>());
    MC_DLOG_PRINTF("ModuleLoader: library loaded, creating module\n");
    auto m = CreateModuleFromLibrary(n);
    MC_DLOG_PRINTF("ModuleLoader: module created, type index = %d\n", m->RuntimeTypeIndex());
    return m;
}
REGISTER_FUNCTION("runtime.ModuleLoader", ModuleLoader);

REGISTER_GLOBAL("runtime.SystemLib")
    .SetBody([]() {
        static auto m = CreateModuleFromLibrary(SystemLibrary::Global());
        return m;
});


} // namespace runtime
} // namespace mc

int BackendRegisterSystemLibSymbol(const char* name, void* ptr) {
    mc::runtime::SystemLibrary::Global()->RegisterSymbol(name, ptr);
    return 0;
}
