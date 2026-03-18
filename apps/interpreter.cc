#include <cctype>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

constexpr int kIndentSize = 4;

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

struct Expr {
  enum class Kind { kInt, kVar, kUnary, kBinary, kCompareChain };
  Kind kind = Kind::kInt;
  int64_t int_value = 0;
  std::string name;
  std::string op;
  std::unique_ptr<Expr> left;
  std::unique_ptr<Expr> right;
  std::vector<std::string> cmp_ops;
  std::vector<Expr> cmp_rhs;

  static Expr Int(int64_t value) {
    Expr e;
    e.kind = Kind::kInt;
    e.int_value = value;
    return e;
  }

  static Expr Var(std::string name) {
    Expr e;
    e.kind = Kind::kVar;
    e.name = std::move(name);
    return e;
  }

  static Expr Unary(std::string op, Expr value) {
    Expr e;
    e.kind = Kind::kUnary;
    e.op = std::move(op);
    e.left = std::make_unique<Expr>(std::move(value));
    return e;
  }

  static Expr Binary(std::string op, Expr lhs, Expr rhs) {
    Expr e;
    e.kind = Kind::kBinary;
    e.op = std::move(op);
    e.left = std::make_unique<Expr>(std::move(lhs));
    e.right = std::make_unique<Expr>(std::move(rhs));
    return e;
  }

  static Expr CompareChain(Expr left, std::vector<std::string> ops, std::vector<Expr> rhs) {
    Expr e;
    e.kind = Kind::kCompareChain;
    e.left = std::make_unique<Expr>(std::move(left));
    e.cmp_ops = std::move(ops);
    e.cmp_rhs = std::move(rhs);
    return e;
  }
};

struct Stmt {
  enum class Kind { kAssign, kExpr, kIf, kWhile, kFor, kReturn, kBlock };
  Kind kind = Kind::kExpr;
  std::string name;
  Expr expr;
  Expr expr2;
  Expr expr3;
  std::vector<Stmt> body;
  std::vector<Stmt> else_body;

  static Stmt ExprStmt(Expr expr) {
    Stmt s;
    s.kind = Kind::kExpr;
    s.expr = std::move(expr);
    return s;
  }

  static Stmt Assign(std::string name, Expr expr) {
    Stmt s;
    s.kind = Kind::kAssign;
    s.name = std::move(name);
    s.expr = std::move(expr);
    return s;
  }

  static Stmt If(Expr cond, std::vector<Stmt> then_body, std::vector<Stmt> else_body) {
    Stmt s;
    s.kind = Kind::kIf;
    s.expr = std::move(cond);
    s.body = std::move(then_body);
    s.else_body = std::move(else_body);
    return s;
  }

  static Stmt While(Expr cond, std::vector<Stmt> body) {
    Stmt s;
    s.kind = Kind::kWhile;
    s.expr = std::move(cond);
    s.body = std::move(body);
    return s;
  }

  static Stmt For(std::string name, Expr start, Expr end, Expr step, std::vector<Stmt> body) {
    Stmt s;
    s.kind = Kind::kFor;
    s.name = std::move(name);
    s.expr = std::move(start);
    s.expr2 = std::move(end);
    s.expr3 = std::move(step);
    s.body = std::move(body);
    return s;
  }

  static Stmt Return(Expr expr) {
    Stmt s;
    s.kind = Kind::kReturn;
    s.expr = std::move(expr);
    return s;
  }
};

enum class TokenType {
  kIdentifier,
  kInteger,
  kLParen,
  kRParen,
  kComma,
  kOperator,
  kEnd,
};

struct Token {
  TokenType type = TokenType::kEnd;
  std::string text;
  int64_t int_value = 0;
};

class Lexer {
 public:
  explicit Lexer(std::string input) : input_(std::move(input)) {}

  std::vector<Token> Tokenize() {
    std::vector<Token> tokens;
    while (pos_ < input_.size()) {
      char c = input_[pos_];
      if (std::isspace(static_cast<unsigned char>(c))) {
        ++pos_;
        continue;
      }
      if (std::isdigit(static_cast<unsigned char>(c))) {
        tokens.push_back(ReadNumber());
        continue;
      }
      if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
        tokens.push_back(ReadIdentifier());
        continue;
      }
      if (c == '(') {
        ++pos_;
        tokens.push_back({TokenType::kLParen, "(", 0});
        continue;
      }
      if (c == ')') {
        ++pos_;
        tokens.push_back({TokenType::kRParen, ")", 0});
        continue;
      }
      if (c == ',') {
        ++pos_;
        tokens.push_back({TokenType::kComma, ",", 0});
        continue;
      }
      tokens.push_back(ReadOperator());
    }
    tokens.push_back({TokenType::kEnd, "", 0});
    return tokens;
  }

 private:
  Token ReadNumber() {
    size_t start = pos_;
    while (pos_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[pos_]))) {
      ++pos_;
    }
    std::string text = input_.substr(start, pos_ - start);
    return {TokenType::kInteger, text, std::stoll(text)};
  }

  Token ReadIdentifier() {
    size_t start = pos_;
    while (pos_ < input_.size() &&
           (std::isalnum(static_cast<unsigned char>(input_[pos_])) || input_[pos_] == '_')) {
      ++pos_;
    }
    std::string text = input_.substr(start, pos_ - start);
    if (text == "and" || text == "or" || text == "not") {
      return {TokenType::kOperator, text, 0};
    }
    return {TokenType::kIdentifier, text, 0};
  }

  Token ReadOperator() {
    static const std::vector<std::string> ops = {"==", "!=", "<=", ">=", "<", ">", "+", "-", "*", "/", "%"};
    for (const auto& op : ops) {
      if (input_.compare(pos_, op.size(), op) == 0) {
        pos_ += op.size();
        return {TokenType::kOperator, op, 0};
      }
    }
    std::ostringstream oss;
    oss << "Unexpected character: '" << input_[pos_] << "'";
    throw std::runtime_error(oss.str());
  }

  std::string input_;
  size_t pos_ = 0;
};

class ExprParser {
 public:
  explicit ExprParser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

  Expr Parse() { return ParseOr(); }

 private:
  const Token& Peek() const { return tokens_[pos_]; }

  const Token& Consume() { return tokens_[pos_++]; }

  bool MatchOperator(const std::string& op) {
    if (Peek().type == TokenType::kOperator && Peek().text == op) {
      ++pos_;
      return true;
    }
    return false;
  }

  Expr ParseOr() {
    Expr left = ParseAnd();
    while (MatchOperator("or")) {
      Expr right = ParseAnd();
      left = Expr::Binary("or", std::move(left), std::move(right));
    }
    return left;
  }

  Expr ParseAnd() {
    Expr left = ParseCompare();
    while (MatchOperator("and")) {
      Expr right = ParseCompare();
      left = Expr::Binary("and", std::move(left), std::move(right));
    }
    return left;
  }

  Expr ParseCompare() {
    Expr left = ParseAdd();
    std::vector<std::string> ops;
    std::vector<Expr> rights;
    while (Peek().type == TokenType::kOperator &&
           (Peek().text == "==" || Peek().text == "!=" || Peek().text == "<" ||
            Peek().text == "<=" || Peek().text == ">" || Peek().text == ">=")) {
      std::string op = Consume().text;
      Expr right = ParseAdd();
      ops.push_back(op);
      rights.push_back(std::move(right));
    }
    if (ops.empty()) {
      return left;
    }
    return Expr::CompareChain(std::move(left), std::move(ops), std::move(rights));
  }

  Expr ParseAdd() {
    Expr left = ParseMul();
    while (Peek().type == TokenType::kOperator &&
           (Peek().text == "+" || Peek().text == "-")) {
      std::string op = Consume().text;
      Expr right = ParseMul();
      left = Expr::Binary(op, std::move(left), std::move(right));
    }
    return left;
  }

  Expr ParseMul() {
    Expr left = ParseUnary();
    while (Peek().type == TokenType::kOperator &&
           (Peek().text == "*" || Peek().text == "/" || Peek().text == "%")) {
      std::string op = Consume().text;
      Expr right = ParseUnary();
      left = Expr::Binary(op, std::move(left), std::move(right));
    }
    return left;
  }

  Expr ParseUnary() {
    if (MatchOperator("not")) {
      Expr value = ParseUnary();
      return Expr::Unary("not", std::move(value));
    }
    if (MatchOperator("-")) {
      Expr value = ParseUnary();
      return Expr::Unary("-", std::move(value));
    }
    if (MatchOperator("+")) {
      Expr value = ParseUnary();
      return value;
    }
    return ParsePrimary();
  }

  Expr ParsePrimary() {
    if (Peek().type == TokenType::kInteger) {
      int64_t value = Consume().int_value;
      return Expr::Int(value);
    }
    if (Peek().type == TokenType::kIdentifier) {
      std::string name = Consume().text;
      if (name == "True") {
        return Expr::Int(1);
      }
      if (name == "False") {
        return Expr::Int(0);
      }
      return Expr::Var(std::move(name));
    }
    if (Peek().type == TokenType::kLParen) {
      Consume();
      Expr value = ParseOr();
      if (Peek().type != TokenType::kRParen) {
        throw std::runtime_error("Expected ')'");
      }
      Consume();
      return value;
    }
    throw std::runtime_error("Unexpected token in expression");
  }

  std::vector<Token> tokens_;
  size_t pos_ = 0;
};

Expr ParseExpr(const std::string& text) {
  Lexer lexer(text);
  ExprParser parser(lexer.Tokenize());
  return parser.Parse();
}

std::vector<std::string> SplitArgs(const std::string& text) {
  std::vector<std::string> args;
  int depth = 0;
  size_t start = 0;
  for (size_t i = 0; i < text.size(); ++i) {
    char c = text[i];
    if (c == '(') {
      ++depth;
    } else if (c == ')') {
      --depth;
    } else if (c == ',' && depth == 0) {
      args.push_back(Trim(text.substr(start, i - start)));
      start = i + 1;
    }
  }
  if (start < text.size()) {
    args.push_back(Trim(text.substr(start)));
  }
  return args;
}

size_t FindAssign(const std::string& line) {
  for (size_t i = 0; i < line.size(); ++i) {
    if (line[i] != '=') {
      continue;
    }
    if (i > 0 && (line[i - 1] == '=' || line[i - 1] == '!' || line[i - 1] == '<' ||
                  line[i - 1] == '>')) {
      continue;
    }
    if (i + 1 < line.size() && line[i + 1] == '=') {
      continue;
    }
    return i;
  }
  return std::string::npos;
}

std::vector<Stmt> ParseBlock(const std::vector<Line>& lines, size_t& index, int indent) {
  std::vector<Stmt> stmts;
  while (index < lines.size() && lines[index].indent == indent) {
    std::string line = Trim(lines[index].text);
    if (line.empty()) {
      ++index;
      continue;
    }
    if (StartsWith(line, "if ") && line.back() == ':') {
      std::string cond = Trim(line.substr(3, line.size() - 4));
      ++index;
      auto then_body = ParseBlock(lines, index, indent + kIndentSize);
      std::vector<Stmt> else_body;
      if (index < lines.size() && lines[index].indent == indent &&
          Trim(lines[index].text) == "else:") {
        ++index;
        else_body = ParseBlock(lines, index, indent + kIndentSize);
      }
      stmts.push_back(Stmt::If(ParseExpr(cond), std::move(then_body), std::move(else_body)));
      continue;
    }
    if (StartsWith(line, "while ") && line.back() == ':') {
      std::string cond = Trim(line.substr(6, line.size() - 7));
      ++index;
      auto body = ParseBlock(lines, index, indent + kIndentSize);
      stmts.push_back(Stmt::While(ParseExpr(cond), std::move(body)));
      continue;
    }
    if (StartsWith(line, "for ") && line.back() == ':') {
      std::string header = Trim(line.substr(4, line.size() - 5));
      size_t in_pos = header.find(" in ");
      if (in_pos == std::string::npos) {
        throw std::runtime_error("for requires 'in range(...)'");
      }
      std::string name = Trim(header.substr(0, in_pos));
      std::string rhs = Trim(header.substr(in_pos + 4));
      if (!StartsWith(rhs, "range(") || rhs.back() != ')') {
        throw std::runtime_error("for requires range(...)");
      }
      std::string args_text = rhs.substr(6, rhs.size() - 7);
      auto args = SplitArgs(args_text);
      Expr start = Expr::Int(0);
      Expr end = Expr::Int(0);
      Expr step = Expr::Int(1);
      if (args.size() == 1) {
        end = ParseExpr(args[0]);
      } else if (args.size() == 2) {
        start = ParseExpr(args[0]);
        end = ParseExpr(args[1]);
      } else if (args.size() == 3) {
        start = ParseExpr(args[0]);
        end = ParseExpr(args[1]);
        step = ParseExpr(args[2]);
      } else {
        throw std::runtime_error("range expects 1-3 arguments");
      }
      ++index;
      auto body = ParseBlock(lines, index, indent + kIndentSize);
      stmts.push_back(Stmt::For(name, std::move(start), std::move(end), std::move(step),
                                std::move(body)));
      continue;
    }
    if (StartsWith(line, "return ")) {
      std::string expr = Trim(line.substr(7));
      stmts.push_back(Stmt::Return(ParseExpr(expr)));
      ++index;
      continue;
    }
    size_t assign_pos = FindAssign(line);
    if (assign_pos != std::string::npos) {
      std::string name = Trim(line.substr(0, assign_pos));
      std::string expr = Trim(line.substr(assign_pos + 1));
      stmts.push_back(Stmt::Assign(name, ParseExpr(expr)));
      ++index;
      continue;
    }
    stmts.push_back(Stmt::ExprStmt(ParseExpr(line)));
    ++index;
  }
  return stmts;
}

class Interpreter {
 public:
  std::optional<int64_t> Execute(const std::vector<Stmt>& stmts, bool top_level) {
    std::optional<int64_t> last_expr;
    ExecResult result = ExecBlock(stmts, top_level ? &last_expr : nullptr);
    if (result.has_return) {
      return result.value;
    }
    return last_expr;
  }

 private:
  struct ExecResult {
    bool has_return = false;
    int64_t value = 0;
  };

  int64_t EvalExpr(const Expr& expr) {
    switch (expr.kind) {
      case Expr::Kind::kInt:
        return expr.int_value;
      case Expr::Kind::kVar: {
        auto it = env_.find(expr.name);
        if (it == env_.end()) {
          throw std::runtime_error("Unknown variable: " + expr.name);
        }
        return it->second;
      }
      case Expr::Kind::kUnary: {
        int64_t value = EvalExpr(*expr.left);
        if (expr.op == "not") {
          return value == 0 ? 1 : 0;
        }
        if (expr.op == "-") {
          return -value;
        }
        throw std::runtime_error("Unsupported unary op: " + expr.op);
      }
      case Expr::Kind::kBinary: {
        if (expr.op == "and") {
          int64_t left = EvalExpr(*expr.left);
          if (left == 0) {
            return 0;
          }
          int64_t right = EvalExpr(*expr.right);
          return right != 0 ? 1 : 0;
        }
        if (expr.op == "or") {
          int64_t left = EvalExpr(*expr.left);
          if (left != 0) {
            return 1;
          }
          int64_t right = EvalExpr(*expr.right);
          return right != 0 ? 1 : 0;
        }
        int64_t lhs = EvalExpr(*expr.left);
        int64_t rhs = EvalExpr(*expr.right);
        if (expr.op == "+") return lhs + rhs;
        if (expr.op == "-") return lhs - rhs;
        if (expr.op == "*") return lhs * rhs;
        if (expr.op == "/") return lhs / rhs;
        if (expr.op == "%") return lhs % rhs;
        if (expr.op == "==") return lhs == rhs ? 1 : 0;
        if (expr.op == "!=") return lhs != rhs ? 1 : 0;
        if (expr.op == "<") return lhs < rhs ? 1 : 0;
        if (expr.op == "<=") return lhs <= rhs ? 1 : 0;
        if (expr.op == ">") return lhs > rhs ? 1 : 0;
        if (expr.op == ">=") return lhs >= rhs ? 1 : 0;
        throw std::runtime_error("Unsupported binary op: " + expr.op);
      }
      case Expr::Kind::kCompareChain: {
        int64_t lhs = EvalExpr(*expr.left);
        for (size_t i = 0; i < expr.cmp_ops.size(); ++i) {
          int64_t rhs = EvalExpr(expr.cmp_rhs[i]);
          const std::string& op = expr.cmp_ops[i];
          bool ok = false;
          if (op == "==") ok = lhs == rhs;
          else if (op == "!=") ok = lhs != rhs;
          else if (op == "<") ok = lhs < rhs;
          else if (op == "<=") ok = lhs <= rhs;
          else if (op == ">") ok = lhs > rhs;
          else if (op == ">=") ok = lhs >= rhs;
          else throw std::runtime_error("Unsupported compare op: " + op);
          if (!ok) {
            return 0;
          }
          lhs = rhs;
        }
        return 1;
      }
    }
    throw std::runtime_error("Invalid expression");
  }

  ExecResult ExecBlock(const std::vector<Stmt>& stmts, std::optional<int64_t>* last_expr) {
    for (const auto& stmt : stmts) {
      ExecResult result = ExecStmt(stmt, last_expr);
      if (result.has_return) {
        return result;
      }
    }
    return {};
  }

  ExecResult ExecStmt(const Stmt& stmt, std::optional<int64_t>* last_expr) {
    switch (stmt.kind) {
      case Stmt::Kind::kAssign: {
        int64_t value = EvalExpr(stmt.expr);
        env_[stmt.name] = value;
        return {};
      }
      case Stmt::Kind::kExpr: {
        int64_t value = EvalExpr(stmt.expr);
        if (last_expr) {
          *last_expr = value;
        }
        return {};
      }
      case Stmt::Kind::kReturn: {
        int64_t value = EvalExpr(stmt.expr);
        return {true, value};
      }
      case Stmt::Kind::kIf: {
        int64_t cond = EvalExpr(stmt.expr);
        if (cond != 0) {
          return ExecBlock(stmt.body, last_expr);
        }
        return ExecBlock(stmt.else_body, last_expr);
      }
      case Stmt::Kind::kWhile: {
        while (EvalExpr(stmt.expr) != 0) {
          ExecResult result = ExecBlock(stmt.body, last_expr);
          if (result.has_return) {
            return result;
          }
        }
        return {};
      }
      case Stmt::Kind::kFor: {
        int64_t start = EvalExpr(stmt.expr);
        int64_t end = EvalExpr(stmt.expr2);
        int64_t step = EvalExpr(stmt.expr3);
        if (step == 0) {
          throw std::runtime_error("range step cannot be 0");
        }
        if (step > 0) {
          for (int64_t i = start; i < end; i += step) {
            env_[stmt.name] = i;
            ExecResult result = ExecBlock(stmt.body, last_expr);
            if (result.has_return) {
              return result;
            }
          }
        } else {
          for (int64_t i = start; i > end; i += step) {
            env_[stmt.name] = i;
            ExecResult result = ExecBlock(stmt.body, last_expr);
            if (result.has_return) {
              return result;
            }
          }
        }
        return {};
      }
      case Stmt::Kind::kBlock:
        return ExecBlock(stmt.body, last_expr);
    }
    return {};
  }

  std::unordered_map<std::string, int64_t> env_;
};

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

}  // namespace

int main(int argc, char** argv) {
  try {
    Interpreter interpreter;
    if (argc > 1) {
      std::ifstream file(argv[1]);
      if (!file.is_open()) {
        std::cerr << "Unable to open file: " << argv[1] << "\n";
        return 1;
      }
      std::ostringstream ss;
      ss << file.rdbuf();
      auto lines = CollectLinesFromString(ss.str());
      size_t index = 0;
      auto stmts = ParseBlock(lines, index, 0);
      auto result = interpreter.Execute(stmts, false);
      if (result.has_value()) {
        std::cout << result.value() << "\n";
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
      size_t index = 0;
      auto stmts = ParseBlock(lines, index, 0);
      auto result = interpreter.Execute(stmts, true);
      if (result.has_value()) {
        std::cout << result.value() << "\n";
      }
    }
  } catch (const std::exception& ex) {
    std::cerr << "Interpreter error: " << ex.what() << "\n";
    return 1;
  }
  return 0;
}
