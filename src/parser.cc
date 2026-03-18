#include "parser.h"

#include <cctype>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "container.h"
#include "expression.h"
#include "statement.h"

namespace mc {
namespace runtime {

namespace {

constexpr int kIndentSize = 4;

IntImm MakeIntImm64(int64_t value) {
    return IntImm(DataType::Int(64), value);
}

NullImm MakeNullImm() {
    auto node = MakeObject<NullImmNode>();
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

std::string TypeToString(const DataType& t) {
    if (t.IsBool()) return "bool";
    if (t.IsFloat() && t.b() == 64) return "float64";
    if (t.IsInt() && t.b() == 64) return "int64";
    if (t.IsHandle()) return "handle";
    if (t.IsVoid()) return "void";
    std::ostringstream oss;
    oss << "dtype(" << t.c() << "," << t.b() << "," << t.a() << ")";
    return oss.str();
}

enum class TokenType {
    kIdentifier,
    kInteger,
    kFloat,
    kString,
    kLParen,
    kRParen,
    kLBracket,
    kRBracket,
    kLBrace,
    kRBrace,
    kComma,
    kDot,
    kColon,
    kOperator,
    kEnd,
};

struct Token {
    TokenType type = TokenType::kEnd;
    std::string text;
    int64_t int_value = 0;
    double float_value = 0.0;
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
            if (c == '"' || c == '\'') {
                tokens.push_back(ReadString(c));
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
            if (c == '[') {
                ++pos_;
                tokens.push_back({TokenType::kLBracket, "[", 0});
                continue;
            }
            if (c == ']') {
                ++pos_;
                tokens.push_back({TokenType::kRBracket, "]", 0});
                continue;
            }
            if (c == '{') {
                ++pos_;
                tokens.push_back({TokenType::kLBrace, "{", 0});
                continue;
            }
            if (c == '}') {
                ++pos_;
                tokens.push_back({TokenType::kRBrace, "}", 0});
                continue;
            }
            if (c == ',') {
                ++pos_;
                tokens.push_back({TokenType::kComma, ",", 0});
                continue;
            }
            if (c == '.') {
                ++pos_;
                tokens.push_back({TokenType::kDot, ".", 0});
                continue;
            }
            if (c == ':') {
                ++pos_;
                tokens.push_back({TokenType::kColon, ":", 0});
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
        bool is_float = false;
        while (pos_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[pos_]))) {
            ++pos_;
        }
        if (pos_ < input_.size() && input_[pos_] == '.') {
            if (pos_ + 1 < input_.size() &&
                std::isdigit(static_cast<unsigned char>(input_[pos_ + 1]))) {
                is_float = true;
                ++pos_;
                while (pos_ < input_.size() &&
                       std::isdigit(static_cast<unsigned char>(input_[pos_]))) {
                    ++pos_;
                }
            }
        }
        std::string text = input_.substr(start, pos_ - start);
        if (is_float) {
            return {TokenType::kFloat, text, 0, std::stod(text)};
        }
        return {TokenType::kInteger, text, std::stoll(text), 0.0};
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

    Token ReadString(char quote) {
        ++pos_;
        std::string value;
        while (pos_ < input_.size()) {
            char c = input_[pos_++];
            if (c == quote) {
                return {TokenType::kString, value, 0};
            }
            if (c == '\\' && pos_ < input_.size()) {
                char esc = input_[pos_++];
                switch (esc) {
                    case 'n':
                        value.push_back('\n');
                        break;
                    case 't':
                        value.push_back('\t');
                        break;
                    case 'r':
                        value.push_back('\r');
                        break;
                    case '\\':
                        value.push_back('\\');
                        break;
                    case '"':
                        value.push_back('"');
                        break;
                    case '\'':
                        value.push_back('\'');
                        break;
                    default:
                        value.push_back(esc);
                        break;
                }
            } else {
                value.push_back(c);
            }
        }
        throw std::runtime_error("Unterminated string literal");
    }

    Token ReadOperator() {
        static const std::vector<std::string> ops = {
            "==", "!=", "<=", ">=", "<", ">", "+", "-", "*", "/", "%"
        };
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

}  // namespace

class ExprParser {
 public:
    ExprParser(std::vector<Token> tokens, Parser& owner, Array<Stmt>* prelude)
        : tokens_(std::move(tokens)), owner_(owner), prelude_(prelude) {}

    PrimExpr Parse() {
        PrimExpr expr = ParseIfExpr();
        if (Peek().type != TokenType::kEnd) {
            throw std::runtime_error("Unexpected trailing tokens in expression");
        }
        return expr;
    }

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

    bool MatchIdentifier(const std::string& name) {
        if (Peek().type == TokenType::kIdentifier && Peek().text == name) {
            ++pos_;
            return true;
        }
        return false;
    }

    PrimExpr ParseIfExpr() {
        Array<Stmt> then_local;
        Array<Stmt>* saved_prelude = prelude_;
        prelude_ = &then_local;
        PrimExpr then_expr = ParseOr();
        then_expr = owner_.LowerClassCalls(then_expr, prelude_);
        prelude_ = saved_prelude;
        if (!MatchIdentifier("if")) {
            for (const auto& s : then_local) {
                saved_prelude->push_back(s);
            }
            return then_expr;
        }

        PrimExpr cond_expr = ParseIfExpr();
        cond_expr = owner_.LowerClassCalls(cond_expr, saved_prelude);
        if (!MatchIdentifier("else")) {
            throw std::runtime_error("Expected 'else' in conditional expression");
        }

        Array<Stmt> else_local;
        prelude_ = &else_local;
        PrimExpr else_expr = ParseIfExpr();
        else_expr = owner_.LowerClassCalls(else_expr, prelude_);
        prelude_ = saved_prelude;

        DataType then_ty = owner_.InferExprType(then_expr);
        DataType else_ty = owner_.InferExprType(else_expr);
        DataType out_ty = owner_.MergeValueType(then_ty, else_ty);

        std::string tmp_name = owner_.NewTempName("__ifexp");
        PrimExpr init_value = owner_.DefaultInitForType(out_ty);
        AllocaVarStmt tmp_init(tmp_name, out_ty, init_value);
        owner_.DefineVar(tmp_name, tmp_init->var, out_ty);

        Array<Stmt> then_stmts;
        for (const auto& s : then_local) {
            then_stmts.push_back(s);
        }
        then_stmts.push_back(AssignStmt(tmp_init->var, then_expr));
        Array<Stmt> else_stmts;
        for (const auto& s : else_local) {
            else_stmts.push_back(s);
        }
        else_stmts.push_back(AssignStmt(tmp_init->var, else_expr));

        saved_prelude->push_back(tmp_init);
        saved_prelude->push_back(IfStmt(cond_expr, SeqStmt(then_stmts), SeqStmt(else_stmts)));
        return tmp_init->var;
    }

    PrimExpr ParseOr() {
        PrimExpr left = ParseAnd();
        while (MatchOperator("or")) {
            PrimExpr right = ParseAnd();
            left = PrimOr(left, right);
        }
        return left;
    }

    PrimExpr ParseAnd() {
        PrimExpr left = ParseCompare();
        while (MatchOperator("and")) {
            PrimExpr right = ParseCompare();
            left = PrimAnd(left, right);
        }
        return left;
    }

    PrimExpr ParseCompare() {
        PrimExpr left = ParseAdd();
        std::vector<std::string> ops;
        std::vector<PrimExpr> rights;
        while (Peek().type == TokenType::kOperator &&
               (Peek().text == "==" || Peek().text == "!=" || Peek().text == "<" ||
                Peek().text == "<=" || Peek().text == ">" || Peek().text == ">=")) {
            std::string op = Consume().text;
            PrimExpr right = ParseAdd();
            ops.push_back(op);
            rights.push_back(right);
        }
        if (ops.empty()) {
            return left;
        }
        PrimExpr expr = MakeCompare(ops[0], left, rights[0]);
        for (size_t i = 1; i < ops.size(); ++i) {
            PrimExpr next = MakeCompare(ops[i], rights[i - 1], rights[i]);
            expr = PrimAnd(expr, next);
        }
        return expr;
    }

    PrimExpr ParseAdd() {
        PrimExpr left = ParseMul();
        while (Peek().type == TokenType::kOperator &&
               (Peek().text == "+" || Peek().text == "-")) {
            std::string op = Consume().text;
            PrimExpr right = ParseMul();
            if (op == "+") {
                left = PrimAdd(left, right);
            } else {
                left = PrimSub(left, right);
            }
        }
        return left;
    }

    PrimExpr ParseMul() {
        PrimExpr left = ParseUnary();
        while (Peek().type == TokenType::kOperator &&
               (Peek().text == "*" || Peek().text == "/" || Peek().text == "%")) {
            std::string op = Consume().text;
            PrimExpr right = ParseUnary();
            if (op == "*") {
                left = PrimMul(left, right);
            } else if (op == "/") {
                left = PrimDiv(left, right);
            } else {
                left = PrimMod(left, right);
            }
        }
        return left;
    }

    PrimExpr ParseUnary() {
        if (MatchOperator("not")) {
            return PrimNot(ParseUnary());
        }
        if (MatchOperator("-")) {
            return PrimSub(MakeIntImm64(0), ParseUnary());
        }
        if (MatchOperator("+")) {
            return ParseUnary();
        }
        return ParsePrimary();
    }

    PrimExpr ParsePrimary() {
        if (Peek().type == TokenType::kInteger) {
            int64_t value = Consume().int_value;
            return ParsePostfix(MakeIntImm64(value));
        }
        if (Peek().type == TokenType::kFloat) {
            double value = Consume().float_value;
            return ParsePostfix(FloatImm(DataType::Float(64), value));
        }
        if (Peek().type == TokenType::kString) {
            std::string value = Consume().text;
            return ParsePostfix(StrImm(value));
        }
        if (Peek().type == TokenType::kIdentifier) {
            std::string name = Consume().text;
            if (name == "True") {
                return Bool(true);
            }
            if (name == "False") {
                return Bool(false);
            }
            if (name == "None") {
                return MakeNullImm();
            }
            return ParsePostfix(owner_.LookupVar(name));
        }
        if (Peek().type == TokenType::kLParen) {
            Consume();
            PrimExpr value = ParseIfExpr();
            if (Peek().type != TokenType::kRParen) {
                throw std::runtime_error("Expected ')'");
            }
            Consume();
            return ParsePostfix(value);
        }
        if (Peek().type == TokenType::kLBracket) {
            return ParsePostfix(ParseListLiteral());
        }
        if (Peek().type == TokenType::kLBrace) {
            return ParsePostfix(ParseDictOrSetLiteral());
        }
        throw std::runtime_error("Unexpected token in expression");
    }

    PrimExpr ParsePostfix(PrimExpr base) {
        while (true) {
            if (Peek().type == TokenType::kLBracket) {
                Consume();
                PrimExpr index = ParseIfExpr();
                if (Peek().type != TokenType::kRBracket) {
                    throw std::runtime_error("Expected ']'");
                }
                Consume();
                base = ContainerGetItem(base, index);
                continue;
            }
            if (Peek().type == TokenType::kDot) {
                Consume();
                if (Peek().type != TokenType::kIdentifier) {
                    throw std::runtime_error("Expected method name");
                }
                std::string member = Consume().text;
                if (Peek().type == TokenType::kLParen) {
                    Consume();
                    std::vector<PrimExpr> args;
                    if (Peek().type != TokenType::kRParen) {
                        while (true) {
                            args.push_back(ParseIfExpr());
                            if (Peek().type == TokenType::kComma) {
                                Consume();
                                continue;
                            }
                            break;
                        }
                    }
                    if (Peek().type != TokenType::kRParen) {
                        throw std::runtime_error("Expected ')'");
                    }
                    Consume();
                    base = ContainerMethodCall(base, StrImm(member), Array<PrimExpr>(args));
                } else {
                    base = ContainerGetItem(base, StrImm(member));
                }
                continue;
            }
            break;
        }
        return base;
    }

    PrimExpr ParseListLiteral() {
        Consume();
        std::vector<PrimExpr> elements;
        if (Peek().type == TokenType::kRBracket) {
            Consume();
            return ListLiteral(Array<PrimExpr>(elements));
        }
        while (true) {
            elements.push_back(ParseIfExpr());
            if (Peek().type == TokenType::kComma) {
                Consume();
                continue;
            }
            if (Peek().type != TokenType::kRBracket) {
                throw std::runtime_error("Expected ']' in list literal");
            }
            Consume();
            break;
        }
        return ListLiteral(Array<PrimExpr>(elements));
    }

    PrimExpr ParseDictOrSetLiteral() {
        Consume();
        if (Peek().type == TokenType::kRBrace) {
            Consume();
            return DictLiteral(Array<PrimExpr>(), Array<PrimExpr>());
        }
        std::vector<PrimExpr> keys;
        std::vector<PrimExpr> values;
        std::vector<PrimExpr> elements;
        bool is_dict = false;
        while (true) {
            PrimExpr first = ParseIfExpr();
            if (Peek().type == TokenType::kColon) {
                is_dict = true;
                Consume();
                PrimExpr value = ParseIfExpr();
                keys.push_back(first);
                values.push_back(value);
            } else {
                elements.push_back(first);
            }
            if (Peek().type == TokenType::kComma) {
                Consume();
                if (Peek().type == TokenType::kRBrace) {
                    Consume();
                    break;
                }
                continue;
            }
            if (Peek().type != TokenType::kRBrace) {
                throw std::runtime_error("Expected '}' in literal");
            }
            Consume();
            break;
        }
        if (is_dict) {
            if (!elements.empty()) {
                throw std::runtime_error("Mixed dict and set literal");
            }
            return DictLiteral(Array<PrimExpr>(keys), Array<PrimExpr>(values));
        }
        return SetLiteral(Array<PrimExpr>(elements));
    }

    PrimExpr MakeCompare(const std::string& op, PrimExpr lhs, PrimExpr rhs) {
        if (op == "==") return PrimEq(lhs, rhs);
        if (op == "!=") return PrimNe(lhs, rhs);
        if (op == "<") return PrimLt(lhs, rhs);
        if (op == "<=") return PrimLe(lhs, rhs);
        if (op == ">") return PrimGt(lhs, rhs);
        if (op == ">=") return PrimGe(lhs, rhs);
        throw std::runtime_error("Unsupported compare operator: " + op);
    }

    std::vector<Token> tokens_;
    size_t pos_ = 0;
    Parser& owner_;
    Array<Stmt>* prelude_;
};

PrimExpr Parser::ParseExpr(const std::string& text) {
    auto lowered = ParseExprLowered(text);
    if (lowered.stmts.size() != 0) {
        throw std::runtime_error("Expression lowering requires statement context");
    }
    return lowered.expr;
}

LoweredExpr Parser::ParseExprLowered(const std::string& text) {
    Lexer lexer(text);
    LoweredExpr out;
    ExprParser parser(lexer.Tokenize(), *this, &out.stmts);
    out.expr = LowerClassCalls(parser.Parse(), &out.stmts);
    return out;
}

PrimVar Parser::LookupVar(const std::string& name) const {
    auto it = symbols_.find(name);
    if (it == symbols_.end()) {
        throw std::runtime_error("Unknown variable: " + name);
    }
    return it->second;
}

void Parser::DefineVar(const std::string& name, PrimVar var, DataType dtype) {
    symbols_[name] = std::move(var);
    symbol_types_[name] = dtype;
}

void Parser::Reset() {
    symbols_.clear();
    symbol_types_.clear();
    current_var_class_types_.clear();
    current_function_name_.clear();
    tmp_id_ = 0;
}

DataType Parser::InferExprType(const PrimExpr& expr) const {
    const auto* base = expr.get();
    if (auto* node = dynamic_cast<const IntImmNode*>(base)) {
        return node->datatype;
    }
    if (auto* node = dynamic_cast<const FloatImmNode*>(base)) {
        return node->datatype;
    }
    if (dynamic_cast<const NullImmNode*>(base)) {
        return DataType::Handle();
    }
    if (dynamic_cast<const StrImmNode*>(base)) {
        return DataType::Handle();
    }
    if (dynamic_cast<const ListLiteralNode*>(base)) {
        return DataType::Handle();
    }
    if (dynamic_cast<const DictLiteralNode*>(base)) {
        return DataType::Handle();
    }
    if (dynamic_cast<const SetLiteralNode*>(base)) {
        return DataType::Handle();
    }
    if (dynamic_cast<const ContainerGetItemNode*>(base)) {
        if (auto* node = dynamic_cast<const ContainerGetItemNode*>(base)) {
            if (auto* obj = node->object.As<PrimVarNode>()) {
                auto vit = current_var_class_types_.find(obj->var_name);
                if (vit != current_var_class_types_.end()) {
                    auto cit = class_defs_.find(vit->second);
                    if (cit != class_defs_.end()) {
                        if (auto* idx = node->index.As<StrImmNode>()) {
                            auto ait = cit->second.attr_types.find(idx->value.c_str());
                            if (ait != cit->second.attr_types.end()) {
                                return ait->second;
                            }
                        }
                    }
                }
            }
        }
        return DataType::Handle();
    }
    if (dynamic_cast<const ContainerSetItemNode*>(base)) {
        return DataType::Handle();
    }
    if (dynamic_cast<const ContainerMethodCallNode*>(base)) {
        return DataType::Handle();
    }
    if (auto* node = dynamic_cast<const PrimVarNode*>(base)) {
        auto it = symbol_types_.find(node->var_name);
        if (it != symbol_types_.end()) {
            return it->second;
        }
        return node->datatype;
    }
    if (dynamic_cast<const PrimAndNode*>(base) || dynamic_cast<const PrimOrNode*>(base) ||
        dynamic_cast<const PrimNotNode*>(base) || dynamic_cast<const PrimEqNode*>(base) ||
        dynamic_cast<const PrimNeNode*>(base) || dynamic_cast<const PrimLtNode*>(base) ||
        dynamic_cast<const PrimLeNode*>(base) || dynamic_cast<const PrimGtNode*>(base) ||
        dynamic_cast<const PrimGeNode*>(base)) {
        return DataType::Bool();
    }
    if (auto* node = dynamic_cast<const PrimAddNode*>(base)) {
        return MergeNumeric(InferExprType(node->a), InferExprType(node->b));
    }
    if (auto* node = dynamic_cast<const PrimSubNode*>(base)) {
        return MergeNumeric(InferExprType(node->a), InferExprType(node->b));
    }
    if (auto* node = dynamic_cast<const PrimMulNode*>(base)) {
        return MergeNumeric(InferExprType(node->a), InferExprType(node->b));
    }
    if (auto* node = dynamic_cast<const PrimDivNode*>(base)) {
        return MergeNumeric(InferExprType(node->a), InferExprType(node->b));
    }
    if (auto* node = dynamic_cast<const PrimModNode*>(base)) {
        return MergeNumeric(InferExprType(node->a), InferExprType(node->b));
    }
    return DataType::Int(64);
}

DataType Parser::MergeNumeric(const DataType& left, const DataType& right) const {
    if (left.IsFloat() || right.IsFloat()) {
        return DataType::Float(64);
    }
    if (left.IsBool() && right.IsBool()) {
        return DataType::Bool();
    }
    return DataType::Int(64);
}

DataType Parser::MergeValueType(const DataType& left, const DataType& right) const {
    if (left == right) {
        return left;
    }
    if (left.IsHandle() || right.IsHandle()) {
        return DataType::Handle();
    }
    return MergeNumeric(left, right);
}

PrimExpr Parser::DefaultInitForType(const DataType& dtype) const {
    if (dtype.IsBool()) {
        return Bool(false);
    }
    if (dtype.IsFloat()) {
        return FloatImm(dtype, 0.0);
    }
    if (dtype.IsHandle()) {
        return MakeNullImm();
    }
    return MakeIntImm64(0);
}

std::string Parser::NewTempName(const std::string& prefix) {
    while (true) {
        std::string name = prefix + "_" + std::to_string(tmp_id_++);
        if (symbols_.find(name) == symbols_.end()) {
            return name;
        }
    }
}

namespace {

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

}  // namespace

DataType Parser::ParseTypeName(const std::string& name) const {
    std::string t = Trim(name);
    if (t == "int") return DataType::Int(64);
    if (t == "float") return DataType::Float(64);
    if (t == "bool") return DataType::Bool();
    if (t == "None" || t == "str" || t == "handle") return DataType::Handle();
    return DataType::Handle();
}

void Parser::ParseFuncSignature(const std::string& header,
                                std::string* name,
                                std::vector<std::string>* param_names,
                                std::vector<DataType>* param_types,
                                DataType* ret_type) const {
    std::string h = Trim(header);
    if (!StartsWith(h, "def ") || h.back() != ':') {
        throw std::runtime_error("Invalid function definition header");
    }
    h = h.substr(4, h.size() - 5);
    size_t lp = h.find('(');
    size_t rp = h.rfind(')');
    if (lp == std::string::npos || rp == std::string::npos || rp < lp) {
        throw std::runtime_error("Invalid function signature");
    }
    *name = Trim(h.substr(0, lp));
    std::string args_text = h.substr(lp + 1, rp - lp - 1);
    std::string ret_text = "handle";
    size_t arrow = h.find("->", rp);
    if (arrow != std::string::npos) {
        ret_text = Trim(h.substr(arrow + 2));
    }
    if (ret_type != nullptr) {
        *ret_type = ParseTypeName(ret_text);
    }
    auto args = SplitArgs(args_text);
    for (const auto& a : args) {
        if (a.empty()) continue;
        size_t colon = a.find(':');
        if (colon == std::string::npos) {
            param_names->push_back(Trim(a));
            param_types->push_back(DataType::Handle());
            continue;
        }
        param_names->push_back(Trim(a.substr(0, colon)));
        param_types->push_back(ParseTypeName(a.substr(colon + 1)));
    }
}

MethodDef Parser::ParseMethodDef(const std::vector<Line>& lines, size_t& index, int indent) {
    MethodDef out;
    ParseFuncSignature(lines[index].text, &out.name, &out.param_names, &out.param_types, &out.ret_type);
    ++index;
    while (index < lines.size() && lines[index].indent >= indent + kIndentSize) {
        Line line = lines[index];
        line.indent -= (indent + kIndentSize);
        out.body_lines.push_back(std::move(line));
        ++index;
    }
    return out;
}

void Parser::InferClassAttrTypes(ClassDefInfo* cls) {
    auto it = cls->methods.find("__init__");
    if (it == cls->methods.end()) {
        return;
    }
    const MethodDef& init = it->second;
    std::unordered_map<std::string, DataType> param_map;
    for (size_t i = 0; i < init.param_names.size(); ++i) {
        param_map[init.param_names[i]] = init.param_types[i];
    }
    for (const auto& line : init.body_lines) {
        std::string t = Trim(line.text);
        size_t eq = FindAssign(t);
        if (eq == std::string::npos) {
            continue;
        }
        std::string lhs = Trim(t.substr(0, eq));
        if (!StartsWith(lhs, "self.")) {
            continue;
        }
        std::string attr = Trim(lhs.substr(std::strlen("self.")));
        std::string rhs = Trim(t.substr(eq + 1));
        auto pit = param_map.find(rhs);
        if (pit != param_map.end()) {
            cls->attr_types[attr] = pit->second;
            continue;
        }
        if (rhs == "True" || rhs == "False") {
            cls->attr_types[attr] = DataType::Bool();
            continue;
        }
        bool has_dot = rhs.find('.') != std::string::npos;
        bool all_digit = !rhs.empty();
        for (char c : rhs) {
            if (!(std::isdigit(static_cast<unsigned char>(c)) || c == '.')) {
                all_digit = false;
                break;
            }
        }
        if (all_digit && has_dot) {
            cls->attr_types[attr] = DataType::Float(64);
        } else if (all_digit) {
            cls->attr_types[attr] = DataType::Int(64);
        }
    }
}

void Parser::ParseClassDef(const std::vector<Line>& lines, size_t& index, int indent) {
    std::string line = Trim(lines[index].text);
    if (!StartsWith(line, "class ") || line.back() != ':') {
        throw std::runtime_error("Invalid class definition header");
    }
    std::string class_name = Trim(line.substr(6, line.size() - 7));
    ClassDefInfo cls;
    cls.name = class_name;
    ++index;
    while (index < lines.size() && lines[index].indent >= indent + kIndentSize) {
        if (lines[index].indent != indent + kIndentSize) {
            ++index;
            continue;
        }
        std::string mline = Trim(lines[index].text);
        if (StartsWith(mline, "def ") && mline.back() == ':') {
            MethodDef m = ParseMethodDef(lines, index, indent + kIndentSize);
            cls.methods[m.name] = std::move(m);
            continue;
        }
        ++index;
    }
    InferClassAttrTypes(&cls);
    class_defs_[class_name] = std::move(cls);
}

PrimFunc Parser::ParseFunctionDef(const std::vector<Line>& lines, size_t& index, int indent) {
    std::string fn_name;
    std::vector<std::string> param_names;
    std::vector<DataType> param_types;
    DataType ret_type = DataType::Handle();
    ParseFuncSignature(lines[index].text, &fn_name, &param_names, &param_types, &ret_type);
    current_function_name_ = fn_name;

    ++index;
    std::vector<Line> body_lines;
    while (index < lines.size() && lines[index].indent >= indent + kIndentSize) {
        Line line = lines[index];
        line.indent -= (indent + kIndentSize);
        body_lines.push_back(std::move(line));
        ++index;
    }

    symbols_.clear();
    symbol_types_.clear();
    current_var_class_types_.clear();
    tmp_id_ = 0;

    Array<PrimVar> params;
    for (size_t i = 0; i < param_names.size(); ++i) {
        PrimVar v(param_names[i], param_types[i]);
        params.push_back(v);
        DefineVar(param_names[i], v, param_types[i]);
    }

    size_t body_idx = 0;
    Stmt body = ParseStmtBlock(body_lines, body_idx, 0);
    return PrimFunc(Str(fn_name), params, Array<PrimExpr>(), body, PrimType(ret_type));
}

PrimExpr Parser::ExpandClassMethod(const std::string& class_name,
                                   const std::string& method_name,
                                   PrimVar obj_var,
                                   const Array<PrimExpr>& args,
                                   Array<Stmt>* prelude) {
    auto cls_it = class_defs_.find(class_name);
    if (cls_it == class_defs_.end()) {
        throw std::runtime_error("Unknown class: " + class_name);
    }
    auto method_it = cls_it->second.methods.find(method_name);
    if (method_it == cls_it->second.methods.end()) {
        throw std::runtime_error("Unknown method: " + class_name + "." + method_name);
    }
    const MethodDef& method = method_it->second;
    if (method.param_names.empty() || method.param_names[0] != "self") {
        throw std::runtime_error("Method " + class_name + "." + method_name + " must have self");
    }
    size_t expect_args = method.param_names.size() - 1;
    if (args.size() != expect_args) {
        throw std::runtime_error(
            class_name + "." + method_name + " expects " + std::to_string(expect_args) + " args");
    }
    for (size_t i = 0; i < expect_args; ++i) {
        DataType expected = method.param_types[i + 1];
        DataType actual = InferExprType(args[i]);
        if (!expected.IsHandle() && expected != actual) {
            throw std::runtime_error(
                "Type mismatch in " + class_name + "." + method_name + " arg " +
                std::to_string(i + 1) + ": expected " + TypeToString(expected) +
                ", got " + TypeToString(actual));
        }
    }

    auto saved_symbols = symbols_;
    auto saved_types = symbol_types_;
    auto saved_var_classes = current_var_class_types_;
    std::string saved_fn = current_function_name_;

    DefineVar("self", obj_var, DataType::Handle());
    current_var_class_types_["self"] = class_name;

    for (size_t i = 0; i < expect_args; ++i) {
        const std::string& pname = method.param_names[i + 1];
        PrimExpr arg_expr = args[i];
        std::string tmp_name = NewTempName("__" + class_name + "_" + method_name + "_arg");
        DataType at = InferExprType(arg_expr);
        AllocaVarStmt arg_alloca(tmp_name, at, arg_expr);
        prelude->push_back(arg_alloca);
        DefineVar(pname, arg_alloca->var, at);
    }

    DataType ret_ty = method.ret_type.IsHandle() ? DataType::Handle() : method.ret_type;
    std::string ret_name = NewTempName("__" + class_name + "_" + method_name + "_ret");
    PrimExpr ret_init = ret_ty.IsHandle() ? PrimExpr(obj_var) : DefaultInitForType(ret_ty);
    AllocaVarStmt ret_alloca(ret_name, ret_ty, ret_init);
    prelude->push_back(ret_alloca);
    DefineVar(ret_name, ret_alloca->var, ret_ty);

    std::string has_ret_name = NewTempName("__" + class_name + "_" + method_name + "_has_ret");
    AllocaVarStmt has_ret_alloca(has_ret_name, DataType::Bool(), Bool(false));
    prelude->push_back(has_ret_alloca);
    DefineVar(has_ret_name, has_ret_alloca->var, DataType::Bool());

    size_t body_index = 0;
    Stmt body_stmt = ParseStmtBlock(method.body_lines, body_index, 0);
    Stmt lowered_body = RewriteMethodReturns(body_stmt,
                                             ret_alloca->var,
                                             has_ret_alloca->var,
                                             method.ret_type,
                                             class_name + "." + method_name);
    prelude->push_back(lowered_body);

    symbols_ = std::move(saved_symbols);
    symbol_types_ = std::move(saved_types);
    current_var_class_types_ = std::move(saved_var_classes);
    current_function_name_ = std::move(saved_fn);
    return ret_alloca->var;
}

Stmt Parser::RewriteMethodReturns(const Stmt& stmt,
                                  PrimVar ret_var,
                                  PrimVar has_ret_var,
                                  const DataType& ret_type,
                                  const std::string& method_qual_name) const {
    if (auto* n = stmt.As<ReturnStmtNode>()) {
        PrimExpr ret_expr = Downcast<PrimExpr>(n->value);
        if (!ret_type.IsHandle()) {
            DataType actual = InferExprType(ret_expr);
            if (actual != ret_type) {
                throw std::runtime_error(
                    "Type mismatch in return of " + method_qual_name +
                    ": expected " + TypeToString(ret_type) +
                    ", got " + TypeToString(actual));
            }
        }
        Array<Stmt> seq;
        seq.push_back(AssignStmt(ret_var, ret_expr));
        seq.push_back(AssignStmt(has_ret_var, Bool(true)));
        return SeqStmt(seq);
    }
    if (auto* n = stmt.As<SeqStmtNode>()) {
        Array<Stmt> seq;
        Stmt empty = SeqStmt(Array<Stmt>());
        for (const auto& s : n->s) {
            Stmt lowered = RewriteMethodReturns(s, ret_var, has_ret_var, ret_type, method_qual_name);
            seq.push_back(IfStmt(PrimNot(has_ret_var), lowered, empty));
        }
        return SeqStmt(seq);
    }
    if (auto* n = stmt.As<IfStmtNode>()) {
        Stmt then_case = RewriteMethodReturns(n->then_case,
                                              ret_var,
                                              has_ret_var,
                                              ret_type,
                                              method_qual_name);
        Stmt else_case = RewriteMethodReturns(n->else_case,
                                              ret_var,
                                              has_ret_var,
                                              ret_type,
                                              method_qual_name);
        return IfStmt(n->cond, then_case, else_case);
    }
    if (auto* n = stmt.As<WhileStmtNode>()) {
        PrimExpr guarded_cond = PrimAnd(PrimNot(has_ret_var), n->cond);
        Stmt body = RewriteMethodReturns(n->body,
                                         ret_var,
                                         has_ret_var,
                                         ret_type,
                                         method_qual_name);
        return WhileStmt(guarded_cond, body);
    }
    return stmt;
}

PrimExpr Parser::LowerClassCalls(PrimExpr expr, Array<Stmt>* prelude) {
    if (auto* n = expr.As<ContainerMethodCallNode>()) {
        PrimExpr obj = LowerClassCalls(Downcast<PrimExpr>(n->object), prelude);
        Array<PrimExpr> args;
        for (const auto& a : n->args) {
            args.push_back(LowerClassCalls(a, prelude));
        }
        if (auto* v = obj.As<PrimVarNode>()) {
            auto it = current_var_class_types_.find(v->var_name);
            if (it != current_var_class_types_.end()) {
                PrimVar obj_var = LookupVar(v->var_name);
                return ExpandClassMethod(it->second, n->method->value.c_str(), obj_var, args, prelude);
            }
        }
        return ContainerMethodCall(obj, n->method, args);
    }
    if (auto* n = expr.As<PrimAddNode>()) {
        return PrimAdd(LowerClassCalls(n->a, prelude), LowerClassCalls(n->b, prelude));
    }
    if (auto* n = expr.As<PrimSubNode>()) {
        return PrimSub(LowerClassCalls(n->a, prelude), LowerClassCalls(n->b, prelude));
    }
    if (auto* n = expr.As<PrimMulNode>()) {
        return PrimMul(LowerClassCalls(n->a, prelude), LowerClassCalls(n->b, prelude));
    }
    if (auto* n = expr.As<PrimDivNode>()) {
        return PrimDiv(LowerClassCalls(n->a, prelude), LowerClassCalls(n->b, prelude));
    }
    if (auto* n = expr.As<PrimModNode>()) {
        return PrimMod(LowerClassCalls(n->a, prelude), LowerClassCalls(n->b, prelude));
    }
    if (auto* n = expr.As<PrimEqNode>()) {
        return PrimEq(LowerClassCalls(n->a, prelude), LowerClassCalls(n->b, prelude));
    }
    if (auto* n = expr.As<PrimNeNode>()) {
        return PrimNe(LowerClassCalls(n->a, prelude), LowerClassCalls(n->b, prelude));
    }
    if (auto* n = expr.As<PrimLtNode>()) {
        return PrimLt(LowerClassCalls(n->a, prelude), LowerClassCalls(n->b, prelude));
    }
    if (auto* n = expr.As<PrimLeNode>()) {
        return PrimLe(LowerClassCalls(n->a, prelude), LowerClassCalls(n->b, prelude));
    }
    if (auto* n = expr.As<PrimGtNode>()) {
        return PrimGt(LowerClassCalls(n->a, prelude), LowerClassCalls(n->b, prelude));
    }
    if (auto* n = expr.As<PrimGeNode>()) {
        return PrimGe(LowerClassCalls(n->a, prelude), LowerClassCalls(n->b, prelude));
    }
    if (auto* n = expr.As<PrimAndNode>()) {
        return PrimAnd(LowerClassCalls(n->a, prelude), LowerClassCalls(n->b, prelude));
    }
    if (auto* n = expr.As<PrimOrNode>()) {
        return PrimOr(LowerClassCalls(n->a, prelude), LowerClassCalls(n->b, prelude));
    }
    if (auto* n = expr.As<PrimNotNode>()) {
        return PrimNot(LowerClassCalls(n->a, prelude));
    }
    if (auto* n = expr.As<ContainerGetItemNode>()) {
        return ContainerGetItem(LowerClassCalls(Downcast<PrimExpr>(n->object), prelude),
                                LowerClassCalls(n->index, prelude));
    }
    return expr;
}

Stmt Parser::ParseStmtBlock(const std::vector<Line>& lines, size_t& index, int indent) {
    struct AssignTarget {
        bool is_subscript = false;
        bool is_attr = false;
        std::string name;
        PrimExpr base;
        PrimExpr index;
    };

    auto assign_or_alloca = [&](const std::string& name, PrimExpr value) -> Stmt {
        auto it = symbols_.find(name);
        if (it == symbols_.end()) {
            DataType dtype = InferExprType(value);
            AllocaVarStmt alloca_stmt(name, dtype, value);
            DefineVar(name, alloca_stmt->var, dtype);
            return alloca_stmt;
        }
        return AssignStmt(it->second, value);
    };

    auto parse_assign_target = [&](const std::string& target) -> AssignTarget {
        AssignTarget info;
        size_t dot = target.find('.');
        if (dot != std::string::npos) {
            std::string base = Trim(target.substr(0, dot));
            std::string attr = Trim(target.substr(dot + 1));
            if (base.empty() || attr.empty()) {
                throw std::runtime_error("Invalid attribute assignment");
            }
            info.is_attr = true;
            info.name = base;
            info.base = LookupVar(base);
            info.index = StrImm(attr);
            return info;
        }
        size_t lbracket = target.find('[');
        if (lbracket == std::string::npos) {
            info.name = Trim(target);
            return info;
        }
        size_t rbracket = target.rfind(']');
        if (rbracket == std::string::npos || rbracket < lbracket) {
            throw std::runtime_error("Invalid subscript assignment");
        }
        std::string base = Trim(target.substr(0, lbracket));
        std::string idx = Trim(target.substr(lbracket + 1, rbracket - lbracket - 1));
        if (base.empty()) {
            throw std::runtime_error("Invalid subscript assignment");
        }
        info.is_subscript = true;
        info.name = base;
        info.base = LookupVar(base);
        info.index = ParseExpr(idx);
        return info;
    };

    auto parse_for = [&](const std::string& header) -> Stmt {
        std::string h = Trim(header.substr(4, header.size() - 5));
        size_t in_pos = h.find(" in ");
        if (in_pos == std::string::npos) {
            throw std::runtime_error("for requires 'in range(...)'");
        }
        std::string name = Trim(h.substr(0, in_pos));
        std::string rhs = Trim(h.substr(in_pos + 4));
        if (!StartsWith(rhs, "range(") || rhs.back() != ')') {
            throw std::runtime_error("for requires range(...)");
        }
        std::string args_text = rhs.substr(6, rhs.size() - 7);
        auto args = SplitArgs(args_text);
        PrimExpr start = MakeIntImm64(0);
        PrimExpr end = MakeIntImm64(0);
        PrimExpr step = MakeIntImm64(1);
        Array<Stmt> prefix;
        if (args.size() == 1) {
            auto end_l = ParseExprLowered(args[0]);
            for (const auto& s : end_l.stmts) {
                prefix.push_back(s);
            }
            end = end_l.expr;
        } else if (args.size() == 2) {
            auto start_l = ParseExprLowered(args[0]);
            auto end_l = ParseExprLowered(args[1]);
            for (const auto& s : start_l.stmts) {
                prefix.push_back(s);
            }
            for (const auto& s : end_l.stmts) {
                prefix.push_back(s);
            }
            start = start_l.expr;
            end = end_l.expr;
        } else if (args.size() == 3) {
            auto start_l = ParseExprLowered(args[0]);
            auto end_l = ParseExprLowered(args[1]);
            auto step_l = ParseExprLowered(args[2]);
            for (const auto& s : start_l.stmts) {
                prefix.push_back(s);
            }
            for (const auto& s : end_l.stmts) {
                prefix.push_back(s);
            }
            for (const auto& s : step_l.stmts) {
                prefix.push_back(s);
            }
            start = start_l.expr;
            end = end_l.expr;
            step = step_l.expr;
        } else {
            throw std::runtime_error("range expects 1-3 arguments");
        }
        Stmt init = assign_or_alloca(name, start);
        ++index;
        Stmt body = ParseStmtBlock(lines, index, indent + kIndentSize);
        PrimVar loop_var = LookupVar(name);
        PrimExpr step_is_neg = PrimLt(step, MakeIntImm64(0));
        PrimExpr cond_pos = PrimLt(loop_var, end);
        PrimExpr cond_neg = PrimGt(loop_var, end);
        PrimExpr cond = PrimOr(PrimAnd(step_is_neg, cond_neg),
                               PrimAnd(PrimNot(step_is_neg), cond_pos));
        AssignStmt inc(loop_var, PrimAdd(loop_var, step));
        Array<Stmt> while_body;
        while_body.push_back(body);
        while_body.push_back(inc);
        WhileStmt while_stmt(cond, SeqStmt(while_body));
        Array<Stmt> seq;
        for (const auto& s : prefix) {
            seq.push_back(s);
        }
        seq.push_back(init);
        seq.push_back(while_stmt);
        return SeqStmt(seq);
    };

    Array<Stmt> stmts;
    while (index < lines.size() && lines[index].indent == indent) {
        std::string line = Trim(lines[index].text);
        if (line.empty()) {
            ++index;
            continue;
        }
        if (StartsWith(line, "class ") && line.back() == ':') {
            ParseClassDef(lines, index, indent);
            continue;
        }
        if (StartsWith(line, "if ") && line.back() == ':') {
            std::string cond = Trim(line.substr(3, line.size() - 4));
            LoweredExpr cond_l = ParseExprLowered(cond);
            ++index;
            Stmt then_block = ParseStmtBlock(lines, index, indent + kIndentSize);
            Stmt else_block = SeqStmt(Array<Stmt>());
            if (index < lines.size() && lines[index].indent == indent &&
                Trim(lines[index].text) == "else:") {
                ++index;
                else_block = ParseStmtBlock(lines, index, indent + kIndentSize);
            }
            for (const auto& s : cond_l.stmts) {
                stmts.push_back(s);
            }
            stmts.push_back(IfStmt(cond_l.expr, then_block, else_block));
            continue;
        }
        if (StartsWith(line, "while ") && line.back() == ':') {
            std::string cond = Trim(line.substr(6, line.size() - 7));
            LoweredExpr prime_l = ParseExprLowered(cond);
            ++index;
            Stmt body = ParseStmtBlock(lines, index, indent + kIndentSize);
            if (prime_l.stmts.size() == 0) {
                stmts.push_back(WhileStmt(prime_l.expr, body));
                continue;
            }
            LoweredExpr update_l = ParseExprLowered(cond);
            DataType cond_ty = InferExprType(prime_l.expr);
            std::string cond_name = NewTempName("__while_cond");
            AllocaVarStmt cond_init(cond_name, cond_ty, DefaultInitForType(cond_ty));
            DefineVar(cond_name, cond_init->var, cond_ty);
            stmts.push_back(cond_init);
            for (const auto& s : prime_l.stmts) {
                stmts.push_back(s);
            }
            stmts.push_back(AssignStmt(cond_init->var, prime_l.expr));
            Array<Stmt> update_stmts;
            for (const auto& s : update_l.stmts) {
                update_stmts.push_back(s);
            }
            update_stmts.push_back(AssignStmt(cond_init->var, update_l.expr));
            Array<Stmt> while_body;
            while_body.push_back(body);
            while_body.push_back(SeqStmt(update_stmts));
            stmts.push_back(WhileStmt(cond_init->var, SeqStmt(while_body)));
            continue;
        }
        if (StartsWith(line, "for ") && line.back() == ':') {
            stmts.push_back(parse_for(line));
            continue;
        }
        if (StartsWith(line, "return ")) {
            std::string expr = Trim(line.substr(7));
            LoweredExpr l = ParseExprLowered(expr);
            for (const auto& s : l.stmts) {
                stmts.push_back(s);
            }
            stmts.push_back(ReturnStmt(l.expr));
            ++index;
            continue;
        }
        if (line == "pass") {
            ++index;
            continue;
        }
        size_t assign_pos = FindAssign(line);
        if (assign_pos != std::string::npos) {
            std::string target = Trim(line.substr(0, assign_pos));
            std::string expr = Trim(line.substr(assign_pos + 1));
            AssignTarget target_info = parse_assign_target(target);

            if (!target_info.is_subscript && !target_info.is_attr) {
                size_t lp = expr.find('(');
                size_t rp = expr.rfind(')');
                if (lp != std::string::npos && rp != std::string::npos && rp > lp) {
                    std::string maybe_class = Trim(expr.substr(0, lp));
                    auto cls_it = class_defs_.find(maybe_class);
                    if (cls_it != class_defs_.end()) {
                        std::string args_text = expr.substr(lp + 1, rp - lp - 1);
                        auto arg_texts = SplitArgs(args_text);

                        DataType obj_ty = DataType::Handle();
                        AllocaVarStmt obj_alloca(target_info.name, obj_ty,
                                                 DictLiteral(Array<PrimExpr>(), Array<PrimExpr>()));
                        symbols_[target_info.name] = obj_alloca->var;
                        symbol_types_[target_info.name] = obj_ty;
                        current_var_class_types_[target_info.name] = maybe_class;
                        stmts.push_back(obj_alloca);

                        Array<PrimExpr> args_expr;
                        for (const auto& at : arg_texts) {
                            auto l = ParseExprLowered(at);
                            for (const auto& s : l.stmts) {
                                stmts.push_back(s);
                            }
                            args_expr.push_back(l.expr);
                        }
                        PrimVar obj_var = LookupVar(target_info.name);
                        PrimExpr ignored = ExpandClassMethod(maybe_class, "__init__", obj_var, args_expr, &stmts);
                        (void)ignored;
                        ++index;
                        continue;
                    }
                }
            }

            LoweredExpr l = ParseExprLowered(expr);
            for (const auto& s : l.stmts) {
                stmts.push_back(s);
            }
            PrimExpr rhs = l.expr;
            if (target_info.is_subscript) {
                stmts.push_back(ExprStmt(ContainerSetItem(target_info.base, target_info.index, rhs)));
            } else if (target_info.is_attr) {
                if (auto* bv = target_info.base.As<PrimVarNode>()) {
                    auto vit = current_var_class_types_.find(bv->var_name);
                    if (vit != current_var_class_types_.end()) {
                        auto cit = class_defs_.find(vit->second);
                        if (cit != class_defs_.end()) {
                            if (auto* sn = target_info.index.As<StrImmNode>()) {
                                std::string attr = sn->value.c_str();
                                auto atit = cit->second.attr_types.find(attr);
                                if (atit != cit->second.attr_types.end()) {
                                    DataType actual = InferExprType(rhs);
                                    if (!atit->second.IsHandle() && atit->second != actual) {
                                        throw std::runtime_error(
                                            "Type mismatch for " + vit->second + "." + attr);
                                    }
                                }
                            }
                        }
                    }
                }
                stmts.push_back(ExprStmt(ContainerSetItem(target_info.base, target_info.index, rhs)));
            } else {
                stmts.push_back(assign_or_alloca(target_info.name, rhs));
                current_var_class_types_.erase(target_info.name);
            }
            ++index;
            continue;
        }
        LoweredExpr l = ParseExprLowered(line);
        for (const auto& s : l.stmts) {
            stmts.push_back(s);
        }
        stmts.push_back(ExprStmt(l.expr));
        ++index;
    }
    return SeqStmt(stmts);
}

ParseOutput Parser::ParseModule(const std::string& source, const ParseOptions& options) {
    std::istringstream iss(source);
    std::string line;
    std::vector<Line> lines;
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

    ParseOutput out;
    class_defs_.clear();

    bool has_toplevel_defs = false;
    for (const auto& l : lines) {
        std::string t = Trim(l.text);
        if (l.indent == 0 && (StartsWith(t, "class ") || StartsWith(t, "def "))) {
            has_toplevel_defs = true;
            break;
        }
    }

    if (!has_toplevel_defs) {
        Reset();
        size_t index = 0;
        Stmt body = ParseStmtBlock(lines, index, 0);
        PrimFunc fn(Str("main"), Array<PrimVar>(), Array<PrimExpr>(), body, PrimType(DataType::Handle()));
        out.functions.push_back(fn);
        out.entry_func = fn;
        out.has_entry = true;
        return out;
    }

    size_t index = 0;
    while (index < lines.size()) {
        if (lines[index].indent != 0) {
            ++index;
            continue;
        }
        std::string t = Trim(lines[index].text);
        if (StartsWith(t, "class ") && t.back() == ':') {
            ParseClassDef(lines, index, 0);
            continue;
        }
        if (StartsWith(t, "def ") && t.back() == ':') {
            PrimFunc fn = ParseFunctionDef(lines, index, 0);
            out.functions.push_back(fn);
            if (!options.entry_func_name.empty()) {
                if (fn->name == Str(options.entry_func_name)) {
                    out.entry_func = fn;
                    out.has_entry = true;
                }
            } else if (!out.has_entry) {
                out.entry_func = fn;
                out.has_entry = true;
            }
            continue;
        }
        ++index;
    }

    if (!out.has_entry && out.functions.size() > 0) {
        out.entry_func = out.functions[0];
        out.has_entry = true;
    }
    if (!out.has_entry) {
        throw std::runtime_error("No entry function found");
    }
    return out;
}

}  // namespace runtime
}  // namespace mc
