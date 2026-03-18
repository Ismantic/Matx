#pragma once

#include <string>
#include <vector>
#include <cstddef>
#include <unordered_map>

#include "datatype.h"
#include "function.h"
#include "str.h"

namespace mc {
namespace runtime {

struct ParseOptions {
    std::string entry_func_name;
};

struct ParseOutput {
    Array<PrimFunc> functions;
    PrimFunc entry_func;
    bool has_entry = false;
};

struct LoweredExpr {
    Array<Stmt> stmts;
    PrimExpr expr;
};

struct Line {
    int indent = 0;
    std::string text;
};

struct MethodDef {
    std::string name;
    std::vector<std::string> param_names;
    std::vector<DataType> param_types;
    DataType ret_type = DataType::Handle();
    std::vector<Line> body_lines;
};

struct ClassDefInfo {
    std::string name;
    std::unordered_map<std::string, MethodDef> methods;
    std::unordered_map<std::string, DataType> attr_types;
};

class Parser {
 public:
    Parser() = default;

    PrimExpr ParseExpr(const std::string& text);
    LoweredExpr ParseExprLowered(const std::string& text);
    Stmt ParseStmtBlock(const std::vector<Line>& lines, size_t& index, int indent);
    void Reset();
    PrimVar LookupVar(const std::string& name) const;
    void DefineVar(const std::string& name, PrimVar var, DataType dtype);
    DataType InferExprType(const PrimExpr& expr) const;
    DataType MergeNumeric(const DataType& left, const DataType& right) const;
    DataType MergeValueType(const DataType& left, const DataType& right) const;
    PrimExpr DefaultInitForType(const DataType& dtype) const;
    std::string NewTempName(const std::string& prefix);
    PrimExpr LowerClassCalls(PrimExpr expr, Array<Stmt>* prelude);

    ParseOutput ParseModule(const std::string& source, const ParseOptions& options = {});

 private:
    MethodDef ParseMethodDef(const std::vector<Line>& lines, size_t& index, int indent);
    void ParseClassDef(const std::vector<Line>& lines, size_t& index, int indent);
    PrimFunc ParseFunctionDef(const std::vector<Line>& lines, size_t& index, int indent);
    void ParseFuncSignature(const std::string& header,
                            std::string* name,
                            std::vector<std::string>* param_names,
                            std::vector<DataType>* param_types,
                            DataType* ret_type) const;
    DataType ParseTypeName(const std::string& name) const;
    void InferClassAttrTypes(ClassDefInfo* cls);
    Stmt RewriteMethodReturns(const Stmt& stmt,
                              PrimVar ret_var,
                              PrimVar has_ret_var,
                              const DataType& ret_type,
                              const std::string& method_qual_name) const;
    PrimExpr ExpandClassMethod(const std::string& class_name,
                               const std::string& method_name,
                               PrimVar obj_var,
                               const Array<PrimExpr>& args,
                               Array<Stmt>* prelude);

    std::unordered_map<std::string, PrimVar> symbols_;
    std::unordered_map<std::string, DataType> symbol_types_;
    std::unordered_map<std::string, ClassDefInfo> class_defs_;
    std::unordered_map<std::string, std::string> current_var_class_types_;
    std::string current_function_name_;
    int tmp_id_ = 0;
};

}  // namespace runtime
}  // namespace mc
