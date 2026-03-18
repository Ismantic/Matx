#include <cctype>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <cmath>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "array.h"
#include "container.h"
#include "datatype.h"
#include "expression.h"
#include "parser.h"
#include "runtime_value.h"
#include "statement.h"
#include "str.h"

using mc::runtime::AllocaVarStmt;
using mc::runtime::AllocaVarStmtNode;
using mc::runtime::Array;
using mc::runtime::AssignStmt;
using mc::runtime::AssignStmtNode;
using mc::runtime::BaseExpr;
using mc::runtime::DataType;
using mc::runtime::Dict;
using mc::runtime::DictLiteral;
using mc::runtime::DictLiteralNode;
using mc::runtime::DictNode;
using mc::runtime::Downcast;
using mc::runtime::ExprStmt;
using mc::runtime::ExprStmtNode;
using mc::runtime::IfStmt;
using mc::runtime::IfStmtNode;
using mc::runtime::IntImmNode;
using mc::runtime::FloatImm;
using mc::runtime::FloatImmNode;
using mc::runtime::NullImm;
using mc::runtime::NullImmNode;
using mc::runtime::List;
using mc::runtime::ListLiteral;
using mc::runtime::ListLiteralNode;
using mc::runtime::ListNode;
using mc::runtime::McValue;
using mc::runtime::PrimAdd;
using mc::runtime::PrimAddNode;
using mc::runtime::PrimAnd;
using mc::runtime::PrimAndNode;
using mc::runtime::PrimDiv;
using mc::runtime::PrimDivNode;
using mc::runtime::PrimEq;
using mc::runtime::PrimEqNode;
using mc::runtime::PrimExpr;
using mc::runtime::PrimExprNode;
using mc::runtime::PrimGe;
using mc::runtime::PrimGeNode;
using mc::runtime::PrimGt;
using mc::runtime::PrimGtNode;
using mc::runtime::PrimLe;
using mc::runtime::PrimLeNode;
using mc::runtime::PrimLt;
using mc::runtime::PrimLtNode;
using mc::runtime::PrimMod;
using mc::runtime::PrimModNode;
using mc::runtime::PrimMul;
using mc::runtime::PrimMulNode;
using mc::runtime::PrimNe;
using mc::runtime::PrimNeNode;
using mc::runtime::PrimNot;
using mc::runtime::PrimNotNode;
using mc::runtime::PrimOr;
using mc::runtime::PrimOrNode;
using mc::runtime::PrimSub;
using mc::runtime::PrimSubNode;
using mc::runtime::PrimVar;
using mc::runtime::PrimVarNode;
using mc::runtime::Set;
using mc::runtime::SetLiteral;
using mc::runtime::SetLiteralNode;
using mc::runtime::SetNode;
using mc::runtime::ReturnStmt;
using mc::runtime::ReturnStmtNode;
using mc::runtime::SeqStmt;
using mc::runtime::SeqStmtNode;
using mc::runtime::Stmt;
using mc::runtime::Str;
using mc::runtime::StrImm;
using mc::runtime::StrImmNode;
using mc::runtime::WhileStmt;
using mc::runtime::WhileStmtNode;
using mc::runtime::ContainerGetItem;
using mc::runtime::ContainerGetItemNode;
using mc::runtime::ContainerMethodCall;
using mc::runtime::ContainerMethodCallNode;
using mc::runtime::ContainerSetItem;
using mc::runtime::ContainerSetItemNode;

namespace {

constexpr int kIndentSize = 4;

NullImm MakeNullImm() {
  auto node = mc::runtime::MakeObject<NullImmNode>();
  node->datatype = DataType::Handle();
  return NullImm(std::move(node));
}

std::string Trim(const std::string& s) {
  size_t start = 0;
  while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
    ++start;
  }
  size_t end = s.size();
  while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
    --end;
  }
  return s.substr(start, end - start);
}

bool StartsWith(const std::string& s, const std::string& prefix) {
  return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

struct Line {
  int indent = 0;
  std::string text;
};

struct ExecResult {
  bool has_return = false;
  McValue value;
};

class Evaluator {
 public:
  ExecResult Execute(const Stmt& stmt, std::optional<McValue>* last_expr) {
    return ExecStmt(stmt, last_expr);
  }

 private:
  ExecResult ExecStmt(const Stmt& stmt, std::optional<McValue>* last_expr) {
    if (auto* node = stmt.As<SeqStmtNode>()) {
      for (const auto& it : node->s) {
        ExecResult result = ExecStmt(it, last_expr);
        if (result.has_return) {
          return result;
        }
      }
      return {};
    }
    if (auto* node = stmt.As<ExprStmtNode>()) {
      McValue value = EvalExpr(Downcast<PrimExpr>(node->expr));
      if (last_expr) {
        *last_expr = value;
      }
      return {};
    }
    if (auto* node = stmt.As<AllocaVarStmtNode>()) {
      McValue value = McValue(int64_t(0));
      if (!node->init_value.none()) {
        value = EvalExpr(Downcast<PrimExpr>(node->init_value));
      }
      env_[node->var->var_name] = value;
      return {};
    }
    if (auto* node = stmt.As<AssignStmtNode>()) {
      PrimVar var = Downcast<PrimVar>(node->u);
      if (var.none()) {
        throw std::runtime_error("Assignment target is not a variable");
      }
      McValue value = EvalExpr(Downcast<PrimExpr>(node->v));
      env_[var->var_name] = value;
      return {};
    }
    if (auto* node = stmt.As<ReturnStmtNode>()) {
      McValue value = EvalExpr(Downcast<PrimExpr>(node->value));
      return {true, value};
    }
    if (auto* node = stmt.As<IfStmtNode>()) {
      if (IsTrue(EvalExpr(node->cond))) {
        return ExecStmt(node->then_case, last_expr);
      }
      return ExecStmt(node->else_case, last_expr);
    }
    if (auto* node = stmt.As<WhileStmtNode>()) {
      while (IsTrue(EvalExpr(node->cond))) {
        ExecResult result = ExecStmt(node->body, last_expr);
        if (result.has_return) {
          return result;
        }
      }
      return {};
    }
    throw std::runtime_error("Unsupported statement");
  }

  McValue EvalExpr(const PrimExpr& expr) {
    const auto* base = expr.get();
    if (auto* node = dynamic_cast<const IntImmNode*>(base)) {
      return McValue(node->value);
    }
    if (auto* node = dynamic_cast<const FloatImmNode*>(base)) {
      return McValue(node->value);
    }
    if (auto* node = dynamic_cast<const NullImmNode*>(base)) {
      return McValue();
    }
    if (auto* node = dynamic_cast<const StrImmNode*>(base)) {
      return McValue(node->value.c_str());
    }
    if (auto* node = dynamic_cast<const PrimVarNode*>(base)) {
      auto it = env_.find(node->var_name);
      if (it == env_.end()) {
        throw std::runtime_error("Unknown variable: " + node->var_name);
      }
      return it->second;
    }
    if (auto* node = dynamic_cast<const ListLiteralNode*>(base)) {
      std::vector<McValue> values;
      values.reserve(node->elements.size());
      for (const auto& elem : node->elements) {
        values.push_back(EvalExpr(elem));
      }
      List list(values.begin(), values.end());
      return McValue(list);
    }
    if (auto* node = dynamic_cast<const DictLiteralNode*>(base)) {
      Dict dict;
      for (size_t i = 0; i < node->keys.size(); ++i) {
        dict.insert(EvalExpr(node->keys[i]), EvalExpr(node->values[i]));
      }
      return McValue(dict);
    }
    if (auto* node = dynamic_cast<const SetLiteralNode*>(base)) {
      std::vector<McValue> values;
      values.reserve(node->elements.size());
      for (const auto& elem : node->elements) {
        values.push_back(EvalExpr(elem));
      }
      Set set(values.begin(), values.end());
      return McValue(set);
    }
    if (auto* node = dynamic_cast<const ContainerGetItemNode*>(base)) {
      McValue obj = EvalExpr(Downcast<PrimExpr>(node->object));
      McValue index = EvalExpr(node->index);
      if (IsListValue(obj)) {
        List list = AsList(obj);
        int64_t idx = ToInt(index);
        return list[idx];
      }
      if (IsDictValue(obj)) {
        Dict dict = AsDict(obj);
        if (!dict.contains(index)) {
          throw std::runtime_error("Dict key not found");
        }
        return dict[index];
      }
      throw std::runtime_error("Indexing unsupported object");
    }
    if (auto* node = dynamic_cast<const ContainerSetItemNode*>(base)) {
      McValue obj = EvalExpr(Downcast<PrimExpr>(node->object));
      McValue index = EvalExpr(node->index);
      McValue value = EvalExpr(node->value);
      if (IsListValue(obj)) {
        List list = AsList(obj);
        int64_t idx = ToInt(index);
        list[idx] = value;
        return value;
      }
      if (IsDictValue(obj)) {
        Dict dict = AsDict(obj);
        dict[index] = value;
        return value;
      }
      throw std::runtime_error("Indexed assignment unsupported object");
    }
    if (auto* node = dynamic_cast<const ContainerMethodCallNode*>(base)) {
      McValue obj = EvalExpr(Downcast<PrimExpr>(node->object));
      std::string method = node->method->value.c_str();
      std::vector<McValue> args;
      args.reserve(node->args.size());
      for (const auto& arg : node->args) {
        args.push_back(EvalExpr(arg));
      }
      if (IsListValue(obj)) {
        List list = AsList(obj);
        if (method == "append") {
          if (args.size() != 1) {
            throw std::runtime_error("list.append expects 1 argument");
          }
          list.append(args[0]);
          return McValue(int64_t(0));
        }
        if (method == "clear") {
          list.clear();
          return McValue(int64_t(0));
        }
      }
      if (IsSetValue(obj)) {
        Set set = AsSet(obj);
        if (method == "add") {
          if (args.size() != 1) {
            throw std::runtime_error("set.add expects 1 argument");
          }
          set.insert(args[0]);
          return McValue(int64_t(0));
        }
        if (method == "discard") {
          if (args.size() != 1) {
            throw std::runtime_error("set.discard expects 1 argument");
          }
          set.erase(args[0]);
          return McValue(int64_t(0));
        }
        if (method == "clear") {
          set.clear();
          return McValue(int64_t(0));
        }
      }
      if (IsDictValue(obj)) {
        Dict dict = AsDict(obj);
        if (method == "get") {
          if (args.empty() || args.size() > 2) {
            throw std::runtime_error("dict.get expects 1-2 arguments");
          }
          if (dict.contains(args[0])) {
            return dict[args[0]];
          }
          if (args.size() == 2) {
            return args[1];
          }
          return McValue();
        }
        if (method == "clear") {
          dict.clear();
          return McValue(int64_t(0));
        }
      }
      throw std::runtime_error("Unsupported container method: " + method);
    }
    if (auto* node = dynamic_cast<const PrimAddNode*>(base)) {
      return Add(EvalExpr(node->a), EvalExpr(node->b));
    }
    if (auto* node = dynamic_cast<const PrimSubNode*>(base)) {
      return Sub(EvalExpr(node->a), EvalExpr(node->b));
    }
    if (auto* node = dynamic_cast<const PrimMulNode*>(base)) {
      return Mul(EvalExpr(node->a), EvalExpr(node->b));
    }
    if (auto* node = dynamic_cast<const PrimDivNode*>(base)) {
      return Div(EvalExpr(node->a), EvalExpr(node->b));
    }
    if (auto* node = dynamic_cast<const PrimModNode*>(base)) {
      return Mod(EvalExpr(node->a), EvalExpr(node->b));
    }
    if (auto* node = dynamic_cast<const PrimEqNode*>(base)) {
      return McValue(Compare(EvalExpr(node->a), EvalExpr(node->b), "==") ? 1 : 0);
    }
    if (auto* node = dynamic_cast<const PrimNeNode*>(base)) {
      return McValue(Compare(EvalExpr(node->a), EvalExpr(node->b), "!=") ? 1 : 0);
    }
    if (auto* node = dynamic_cast<const PrimLtNode*>(base)) {
      return McValue(Compare(EvalExpr(node->a), EvalExpr(node->b), "<") ? 1 : 0);
    }
    if (auto* node = dynamic_cast<const PrimLeNode*>(base)) {
      return McValue(Compare(EvalExpr(node->a), EvalExpr(node->b), "<=") ? 1 : 0);
    }
    if (auto* node = dynamic_cast<const PrimGtNode*>(base)) {
      return McValue(Compare(EvalExpr(node->a), EvalExpr(node->b), ">") ? 1 : 0);
    }
    if (auto* node = dynamic_cast<const PrimGeNode*>(base)) {
      return McValue(Compare(EvalExpr(node->a), EvalExpr(node->b), ">=") ? 1 : 0);
    }
    if (auto* node = dynamic_cast<const PrimAndNode*>(base)) {
      McValue left = EvalExpr(node->a);
      if (!IsTrue(left)) {
        return McValue(0);
      }
      McValue right = EvalExpr(node->b);
      return McValue(IsTrue(right) ? 1 : 0);
    }
    if (auto* node = dynamic_cast<const PrimOrNode*>(base)) {
      McValue left = EvalExpr(node->a);
      if (IsTrue(left)) {
        return McValue(1);
      }
      McValue right = EvalExpr(node->b);
      return McValue(IsTrue(right) ? 1 : 0);
    }
    if (auto* node = dynamic_cast<const PrimNotNode*>(base)) {
      McValue value = EvalExpr(node->a);
      return McValue(IsTrue(value) ? 0 : 1);
    }
    throw std::runtime_error("Unsupported expression");
  }

  bool IsTrue(const McValue& value) {
    if (value.T() == mc::runtime::TypeIndex::Null) {
      return false;
    }
    if (value.T() == mc::runtime::TypeIndex::Int) {
      return value.As<int64_t>() != 0;
    }
    if (value.T() == mc::runtime::TypeIndex::Float) {
      return value.As<double>() != 0.0;
    }
    if (value.T() == mc::runtime::TypeIndex::Str) {
      const char* s = value.As<const char*>();
      return s && *s != '\0';
    }
    if (IsListValue(value)) {
      return AsList(value).size() != 0;
    }
    if (IsDictValue(value)) {
      return DictSize(AsDict(value)) != 0;
    }
    if (IsSetValue(value)) {
      return AsSet(value).size() != 0;
    }
    return true;
  }

  size_t DictSize(const Dict& dict) {
    return static_cast<size_t>(std::distance(dict.begin(), dict.end()));
  }

  bool IsListValue(const McValue& value) {
    return IsObjectType<ListNode>(value);
  }

  bool IsDictValue(const McValue& value) {
    return IsObjectType<DictNode>(value);
  }

  bool IsSetValue(const McValue& value) {
    return IsObjectType<SetNode>(value);
  }

  template <typename NodeT>
  bool IsObjectType(const McValue& value) {
    if (!value.IsObject()) {
      return false;
    }
    auto* obj = static_cast<mc::runtime::object_t*>(value.As_<void*>());
    return obj && obj->IsType<NodeT>();
  }

  List AsList(const McValue& value) {
    return List(mc::runtime::object_p<mc::runtime::object_t>(
        static_cast<mc::runtime::object_t*>(value.As_<void*>())));
  }

  Dict AsDict(const McValue& value) {
    return Dict(mc::runtime::object_p<mc::runtime::object_t>(
        static_cast<mc::runtime::object_t*>(value.As_<void*>())));
  }

  Set AsSet(const McValue& value) {
    return Set(mc::runtime::object_p<mc::runtime::object_t>(
        static_cast<mc::runtime::object_t*>(value.As_<void*>())));
  }

  int64_t ToInt(const McValue& value) {
    if (value.T() == mc::runtime::TypeIndex::Int) {
      return value.As<int64_t>();
    }
    throw std::runtime_error("Expected int value");
  }

  bool IsFloatValue(const McValue& value) {
    return value.T() == mc::runtime::TypeIndex::Float;
  }

  double ToDouble(const McValue& value) {
    if (value.T() == mc::runtime::TypeIndex::Float) {
      return value.As<double>();
    }
    if (value.T() == mc::runtime::TypeIndex::Int) {
      return static_cast<double>(value.As<int64_t>());
    }
    throw std::runtime_error("Expected numeric value");
  }

  McValue Add(const McValue& lhs, const McValue& rhs) {
    if (lhs.T() == mc::runtime::TypeIndex::Str &&
        rhs.T() == mc::runtime::TypeIndex::Str) {
      std::string left = lhs.As<const char*>() ? lhs.As<const char*>() : "";
      std::string right = rhs.As<const char*>() ? rhs.As<const char*>() : "";
      return McValue(left + right);
    }
    if (IsFloatValue(lhs) || IsFloatValue(rhs)) {
      return McValue(ToDouble(lhs) + ToDouble(rhs));
    }
    return McValue(ToInt(lhs) + ToInt(rhs));
  }

  McValue Sub(const McValue& lhs, const McValue& rhs) {
    if (IsFloatValue(lhs) || IsFloatValue(rhs)) {
      return McValue(ToDouble(lhs) - ToDouble(rhs));
    }
    return McValue(ToInt(lhs) - ToInt(rhs));
  }

  McValue Mul(const McValue& lhs, const McValue& rhs) {
    if (IsFloatValue(lhs) || IsFloatValue(rhs)) {
      return McValue(ToDouble(lhs) * ToDouble(rhs));
    }
    return McValue(ToInt(lhs) * ToInt(rhs));
  }

  McValue Div(const McValue& lhs, const McValue& rhs) {
    if (IsFloatValue(lhs) || IsFloatValue(rhs)) {
      return McValue(ToDouble(lhs) / ToDouble(rhs));
    }
    return McValue(ToInt(lhs) / ToInt(rhs));
  }

  McValue Mod(const McValue& lhs, const McValue& rhs) {
    if (IsFloatValue(lhs) || IsFloatValue(rhs)) {
      return McValue(std::fmod(ToDouble(lhs), ToDouble(rhs)));
    }
    return McValue(ToInt(lhs) % ToInt(rhs));
  }

  bool Compare(const McValue& lhs, const McValue& rhs, const std::string& op) {
    if (lhs.T() == mc::runtime::TypeIndex::Null ||
        rhs.T() == mc::runtime::TypeIndex::Null) {
      if (op == "==") {
        return lhs.T() == rhs.T();
      }
      if (op == "!=") {
        return lhs.T() != rhs.T();
      }
      throw std::runtime_error("Unsupported comparison for None");
    }
    if (lhs.T() == mc::runtime::TypeIndex::Str &&
        rhs.T() == mc::runtime::TypeIndex::Str) {
      std::string left = lhs.As<const char*>() ? lhs.As<const char*>() : "";
      std::string right = rhs.As<const char*>() ? rhs.As<const char*>() : "";
      if (op == "==") return left == right;
      if (op == "!=") return left != right;
      if (op == "<") return left < right;
      if (op == "<=") return left <= right;
      if (op == ">") return left > right;
      if (op == ">=") return left >= right;
    }
    if (IsFloatValue(lhs) || IsFloatValue(rhs)) {
      double left = ToDouble(lhs);
      double right = ToDouble(rhs);
      if (op == "==") return left == right;
      if (op == "!=") return left != right;
      if (op == "<") return left < right;
      if (op == "<=") return left <= right;
      if (op == ">") return left > right;
      if (op == ">=") return left >= right;
    } else {
      int64_t left = ToInt(lhs);
      int64_t right = ToInt(rhs);
      if (op == "==") return left == right;
      if (op == "!=") return left != right;
      if (op == "<") return left < right;
      if (op == "<=") return left <= right;
      if (op == ">") return left > right;
      if (op == ">=") return left >= right;
    }
    throw std::runtime_error("Unsupported comparison");
  }

  std::unordered_map<std::string, McValue> env_;
};

std::string FormatValue(const McValue& value) {
  if (value.T() == mc::runtime::TypeIndex::Null) {
    return "None";
  }
  if (value.T() == mc::runtime::TypeIndex::Int) {
    return std::to_string(value.As<int64_t>());
  }
  if (value.T() == mc::runtime::TypeIndex::Float) {
    return std::to_string(value.As<double>());
  }
  if (value.T() == mc::runtime::TypeIndex::Str) {
    const char* s = value.As<const char*>();
    return s ? std::string(s) : std::string();
  }
  auto is_object_type = [&](auto* dummy) {
    using NodeT = std::remove_pointer_t<decltype(dummy)>;
    if (!value.IsObject()) {
      return false;
    }
    auto* obj = static_cast<mc::runtime::object_t*>(value.As_<void*>());
    return obj && obj->IsType<NodeT>();
  };
  if (is_object_type(static_cast<ListNode*>(nullptr))) {
    List list(mc::runtime::object_p<mc::runtime::object_t>(
        static_cast<mc::runtime::object_t*>(value.As_<void*>())));
    std::string out = "[";
    bool first = true;
    for (const auto& item : list) {
      if (!first) {
        out += ", ";
      }
      first = false;
      out += FormatValue(item);
    }
    out += "]";
    return out;
  }
  if (is_object_type(static_cast<DictNode*>(nullptr))) {
    Dict dict(mc::runtime::object_p<mc::runtime::object_t>(
        static_cast<mc::runtime::object_t*>(value.As_<void*>())));
    std::string out = "{";
    bool first = true;
    for (const auto& kv : dict) {
      if (!first) {
        out += ", ";
      }
      first = false;
      out += FormatValue(kv.first);
      out += ": ";
      out += FormatValue(kv.second);
    }
    out += "}";
    return out;
  }
  if (is_object_type(static_cast<SetNode*>(nullptr))) {
    Set set(mc::runtime::object_p<mc::runtime::object_t>(
        static_cast<mc::runtime::object_t*>(value.As_<void*>())));
    std::string out = "{";
    bool first = true;
    for (const auto& item : set) {
      if (!first) {
        out += ", ";
      }
      first = false;
      out += FormatValue(item);
    }
    out += "}";
    return out;
  }
  return "Object";
}

std::vector<Line> CollectLinesFromString(const std::string& source) {
  std::vector<Line> lines;
  std::istringstream iss(source);
  std::string line;
  while (std::getline(iss, line)) {
    if (Trim(line).empty()) {
      continue;
    }
    int indent = 0;
    while (indent < static_cast<int>(line.size()) && line[indent] == ' ') {
      ++indent;
    }
    if (indent % kIndentSize != 0) {
      throw std::runtime_error("Indentation must be multiples of 4 spaces");
    }
    lines.push_back({indent, line.substr(indent)});
  }
  return lines;
}

std::vector<Line> ReadBlockFromStdin() {
  std::vector<Line> lines;
  std::string line;
  bool multiline = false;
  while (true) {
    std::cout << (lines.empty() ? "matx> " : "...> ");
    if (!std::getline(std::cin, line)) {
      return {};
    }
    if (Trim(line).empty() && lines.empty()) {
      continue;
    }
    if (Trim(line).empty() && multiline) {
      break;
    }
    int indent = 0;
    while (indent < static_cast<int>(line.size()) && line[indent] == ' ') {
      ++indent;
    }
    if (indent % kIndentSize != 0) {
      std::cerr << "Indentation must be multiples of 4 spaces\n";
      lines.clear();
      multiline = false;
      continue;
    }
    std::string trimmed = Trim(line);
    lines.push_back({indent, line.substr(indent)});
    if (!trimmed.empty() && trimmed.back() == ':') {
      multiline = true;
    }
    if (!multiline) {
      break;
    }
  }
  return lines;
}

std::vector<mc::runtime::Line> ToRuntimeLines(const std::vector<Line>& lines) {
  std::vector<mc::runtime::Line> out;
  out.reserve(lines.size());
  for (const auto& line : lines) {
    out.push_back({line.indent, line.text});
  }
  return out;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    mc::runtime::Parser parser;
    Evaluator evaluator;
    if (argc > 1) {
      std::ifstream file(argv[1]);
      if (!file.is_open()) {
        std::cerr << "Unable to open file: " << argv[1] << "\n";
        return 1;
      }
      std::ostringstream ss;
      ss << file.rdbuf();
      auto lines = CollectLinesFromString(ss.str());
      auto runtime_lines = ToRuntimeLines(lines);
      size_t index = 0;
      Stmt program = parser.ParseStmtBlock(runtime_lines, index, 0);
      std::optional<McValue> last_expr;
      ExecResult result = evaluator.Execute(program, &last_expr);
      if (result.has_return) {
        std::cout << FormatValue(result.value) << "\n";
      } else if (last_expr.has_value()) {
        std::cout << FormatValue(last_expr.value()) << "\n";
      }
      return 0;
    }

    while (true) {
      auto lines = ReadBlockFromStdin();
      if (lines.empty()) {
        break;
      }
      std::string first = Trim(lines.front().text);
      if (first == "quit" || first == "exit") {
        break;
      }
      auto runtime_lines = ToRuntimeLines(lines);
      size_t index = 0;
      Stmt block = parser.ParseStmtBlock(runtime_lines, index, 0);
      std::optional<McValue> last_expr;
      ExecResult result = evaluator.Execute(block, &last_expr);
      if (result.has_return) {
        std::cout << FormatValue(result.value) << "\n";
      } else if (last_expr.has_value()) {
        std::cout << FormatValue(last_expr.value()) << "\n";
      }
    }
  } catch (const std::exception& ex) {
    std::cerr << "Interpreter error: " << ex.what() << "\n";
    return 1;
  }
  return 0;
}
