#include "c_api.h"

#include <vector>
#include <string>
#include <stdexcept>
#include <iostream>
#include <sstream>

#include "registry.h"
#include "runtime_value.h"
#include "runtime_module.h"
#include "debug_log.h"

int HandleError(const std::runtime_error& e);

template <typename T>
class LocalStore {
public:
    static T* Get() {
        static thread_local T inst;
        return &inst;
    }

    LocalStore(const LocalStore&) = delete;
    LocalStore& operator=(const LocalStore&) = delete;
private:
    LocalStore() = default;
};

#define API_BEGIN() try {

#define API_END()                                  \
  }                                                \
  catch (std::runtime_error & _except_) {          \
    return HandleError(_except_); \
  }                                                \
  return 0;


std::string NormalizeError(std::string err_msg) {
    if (err_msg.find("Traceback") == 0) {
        return err_msg;
    }

    std::istringstream is(err_msg);
    std::ostringstream os;
    std::string line, file_name, error_type;
    int line_number = 0;

    if (!getline(is, line)) {
        return err_msg;
    }

    size_t pos = line.find(':');
    if (pos != std::string::npos) {
        file_name = line.substr(0, pos);
        try {
            line_number = std::stoi(line.substr(pos + 1));
        } catch (...) {
        }
    }

    error_type = "Error";  
    if (line.find("RuntimeError") != std::string::npos) {
        error_type = "RuntimeError";
    } else if (line.find("ValueError") != std::string::npos) {
        error_type = "ValueError";
    }

    os << error_type << ": ";
    if (!file_name.empty()) {
        os << "File \"" << file_name << "\", line " << line_number << "\n";
    }
    os << line << "\n";

    bool in_stack_trace = false;
    while (getline(is, line)) {
        if (line.find("Stack trace") != std::string::npos) {
            in_stack_trace = true;
            os << "Traceback (most recent call last):\n";
        } else if (in_stack_trace && !line.empty()) {
            if (line.find("[bt]") == std::string::npos) {
                os << "  " << line << "\n";
            }
        } else {
            os << line << "\n";
        }
    }

    return os.str();
}

struct RuntimeEntry {
    std::string error;
};

struct LocalEntry {
    std::vector<const char*> ret_vec_charp;
};

void SetError(const char* s) {
    LocalStore<RuntimeEntry>::Get()->error = s;
}

const char* GetError() {
    return LocalStore<RuntimeEntry>::Get()->error.c_str();
}

int HandleError(const std::runtime_error& e) {
    SetError(NormalizeError(e.what()).c_str());
    return -1;
}

int FuncFree(FunctionHandle fn) {
    API_BEGIN();
    using Func = mc::runtime::Function;
    delete static_cast<Func*>(fn);
    API_END();
}

static void StrAsValue(const std::string& s, Value* value) {
    int32_t len = s.length();
    char* new_str = new char[len+1];
    memcpy(new_str, s.c_str(), len);
    new_str[len] = '\0';

    using mc::runtime::TypeIndex;
    value->t = TypeIndex::Str;
    value->u.v_str = new_str;
    value->p = len;
}

int FuncCall_PYTHON_C_API(FunctionHandle func, Value* vs, int n, Value* r) {
    API_BEGIN();
    using mc::runtime::McView;
    using mc::runtime::McValue;
    using Func = mc::runtime::Function;
    using mc::runtime::Parameters;
    using mc::runtime::DataType;
    using mc::runtime::TypeIndex;

    std::vector<McView> gs;
    gs.reserve(n);
    for (int i = 0; i < n; ++i) {
        gs.push_back(McView(vs[i]));
    }

    McValue rv = (*static_cast<const Func*>(func))(Parameters(gs.data(), gs.size()));
    if (rv.T() == TypeIndex::DataType) {
        std::string s = DtToStr(rv.As<Dt>());
        StrAsValue(s, r);
    } else {
        rv.AsValue(r);
    }

    API_END();
}

int DataTypeToStr(Dt t, char* str, int* size) {
    API_BEGIN();
    auto s = mc::runtime::DtToStr(t);
    memcpy(str, s.data(), s.size()+1);
    *size = s.size();
    API_END();
}

int RegisterGlobal(const char* name, FunctionHandle fn) {
    API_BEGIN();
    using Func = mc::runtime::Function;
    auto* func_ptr = static_cast<Func*>(fn);
    mc::runtime::FunctionRegistry::Register(name)
        .set(*func_ptr);
    API_END();
}

int GetGlobal(const char* name, FunctionHandle* fn){
    API_BEGIN();
    using Func = mc::runtime::Function;
    const Func* p = mc::runtime::FunctionRegistry::Get(name);
    if (p != nullptr) {
        *fn = new Func(*p);
    } else {
        *fn = nullptr;
    }
    API_END();
}

int ListGlobalNames(int* out_size, const char*** out_array) {
    API_BEGIN();
    LocalEntry* ret = LocalStore<LocalEntry>::Get();
    auto ret_vec_str = mc::runtime::FunctionRegistry::ListNames();
    ret->ret_vec_charp.clear();
    for (size_t i = 0; i < ret_vec_str.size(); ++i) {
        ret->ret_vec_charp.push_back(ret_vec_str[i].data());
    }
    *out_array = Pointer(ret->ret_vec_charp);
    *out_size = static_cast<int>(ret_vec_str.size());
    API_END();
}

int ObjectRetain(ObjectHandle o) {
    API_BEGIN();
    using mc::runtime::object_t;
    if (o != nullptr) {
        static_cast<object_t*>(o)->IncCounter();
    }
    API_END();
}

int ObjectFree(ObjectHandle o) {
    API_BEGIN();
    using mc::runtime::object_t;
    if (o != nullptr) {
        static_cast<object_t*>(o)->DecCounter();
    }
    API_END();
}

int GetIndex(const char* name, unsigned* index) {
    API_BEGIN();
    index[0] = mc::runtime::GetIndex(name);
    API_END();
}


int GetBackendFunction(ModuleHandle m, 
                       const char* func_name, 
                       int use_imports,
                       FunctionHandle* out) {
    API_BEGIN();
    MC_DLOG_STREAM(std::cout << "GetBackendFunction" << "\n");
    using mc::runtime::ModuleNode;
    using mc::runtime::object_t;
    using mc::runtime::Function;
    auto me = static_cast<ModuleNode*>(static_cast<object_t*>(m));
    auto pn = me->GetFunction(func_name, use_imports != 0);
    MC_DLOG_STREAM(std::cout << "pn=");
    if (pn != nullptr) {
        MC_DLOG_STREAM(std::cout << "OK" << "\n");
        *out = new Function(pn);
    } else {
        MC_DLOG_STREAM(std::cout << "None" << "\n");
        *out = nullptr;
    }
    API_END();
}
