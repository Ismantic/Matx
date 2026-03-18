#pragma once 

#include <string>
#include <vector>
#include <sstream>
#include <unordered_map>

#include "object.h"
#include "expression.h"
#include "statement.h"
#include "function.h"
#include "visitor.h"
#include "ops.h"

namespace mc {
namespace runtime {

class DocAtomNode : public object_t {
public:
    static constexpr const std::string_view NAME = "printer.DocAtom";
    DEFINE_TYPEINDEX(DocAtomNode, object_t);
};

class DocAtom : public object_r {
public:
    DEFINE_NODE_CLASS(DocAtom, object_r, DocAtomNode);
};

class Doc {
public:
  Doc() {}

  Doc& operator<<(const Doc& r);
  Doc& operator<<(const std::string& r);
  Doc& operator<<(const DocAtom& r);

  template <typename T, typename = typename std::enable_if<!std::is_class<T>::value>::type>
  Doc& operator<<(const T& value) {
    std::ostringstream os;
    os << value;
    return *this << os.str();
  }  

  std::string str();

  static Doc Text(std::string value);

  static Doc RawText(std::string value);

  static Doc NewLine(int indent = 0);

  static Doc Indent(int indent, Doc doc); // TODO

  //static Doc StrLiteral(const std::string& value, 
  //                      const std::string& quote = "\"");
  
  static Doc Brace(std::string open, const Doc& body, 
                   std::string close, int indent = 2);
  
  static Doc Concat(const std::vector<Doc>& vec, 
                    const Doc& sep = Text(", "));


private:
    std::vector<DocAtom> stream_;
};



class AstPrinter : public PrimExprVisitor<Doc(const PrimExpr&)>,
                   public StmtVisitor<Doc(const Stmt&)>,
                   public TypeVisitor<Doc(const Type&)> {
public:
    explicit AstPrinter() = default;

    Doc Print(const object_r& node);

    Doc PrintFunction(const PrimFunc& fn);
    Doc PrintFunction(const AstFunc& fn);
    Doc PrintFunction(const Doc& p, const PrimFunc& fn);
    Doc PrintFunction(const Doc& p, const AstFunc& fn);

private:
    Doc PrintType(const Type& t);

    Doc PrintDataType(DataType datatype);

    template <typename T>
    Doc PrintConstScalar(DataType datatype, const T& value) {
        if (datatype == DataType::Bool()) {
            return Doc::Text(value ? "True" : "False");
        }
        Doc doc;
        doc << value;
        return doc;
    }

    Doc GetName(const std::string& p);
    

    Doc VisitExpr_(const IntImmNode* op) override {
        return PrintConstScalar(op->datatype, op->value);
    }

    Doc VisitExpr_(const FloatImmNode* op) override {
        return PrintConstScalar(op->datatype, op->value);
    }

    Doc VisitExpr_(const NullImmNode* op) override {
        Doc doc;
        doc << "None";
        return doc;
    }

    Doc VisitExpr_(const PrimAddNode* op) override {
        Doc doc;
        doc << "(" << Print(op->a) << " + " << Print(op->b) << ")";
        return doc;
    }

    Doc VisitExpr_(const PrimMulNode* op) override {
        Doc doc;
        doc << "(" << Print(op->a) << " * " << Print(op->b) << ")";
        return doc;
    }

    Doc VisitExpr_(const PrimVarNode* op) override {
        Doc doc;
        doc << op->var_name;
        return doc;
    }

    Doc VisitExpr_(const PrimCallNode* op) override {
        Doc doc;
        auto* p = op->op.As<OpNode>();
        doc << "@" << Doc::Text(p->op_name) << "(";

        for (size_t i = 0; i < op->gs.size(); ++i) {
            if (i > 0) doc << ", ";
            doc << Print(op->gs[i]);
        }
        doc << ")";
        return doc;
    }

    Doc VisitExpr_(const StrImmNode* op) override {
        Doc doc;
        //doc << "\"" << op->value.c_str() << "\"";
        doc << op->value.c_str(); 
        return doc;
    }

    Doc VisitExpr_(const ClassGetItemNode* op) override {
        Doc doc;
        doc << Print(op->object) << "." << Print(op->item);
        return doc;
    }

    // Container literal visitors
    Doc VisitExpr_(const ListLiteralNode* op) override {
        Doc doc;
        doc << "[";
        for (size_t i = 0; i < op->elements.size(); ++i) {
            if (i > 0) doc << ", ";
            doc << Print(op->elements[i]);
        }
        doc << "]";
        return doc;
    }

    Doc VisitExpr_(const DictLiteralNode* op) override {
        Doc doc;
        doc << "{";
        for (size_t i = 0; i < op->keys.size(); ++i) {
            if (i > 0) doc << ", ";
            doc << Print(op->keys[i]) << ": " << Print(op->values[i]);
        }
        doc << "}";
        return doc;
    }

    Doc VisitExpr_(const SetLiteralNode* op) override {
        Doc doc;
        doc << "{";
        for (size_t i = 0; i < op->elements.size(); ++i) {
            if (i > 0) doc << ", ";
            doc << Print(op->elements[i]);
        }
        doc << "}";
        return doc;
    }

    // Container operation visitors
    Doc VisitExpr_(const ContainerGetItemNode* op) override {
        Doc doc;
        doc << Print(op->object) << "[" << Print(op->index) << "]";
        return doc;
    }

    Doc VisitExpr_(const ContainerSetItemNode* op) override {
        Doc doc;
        doc << Print(op->object) << "[" << Print(op->index) << "] = " << Print(op->value);
        return doc;
    }

    Doc VisitExpr_(const ContainerMethodCallNode* op) override {
        Doc doc;
        doc << Print(op->object) << "." << Print(op->method) << "(";
        for (size_t i = 0; i < op->args.size(); ++i) {
            if (i > 0) doc << ", ";
            doc << Print(op->args[i]);
        }
        doc << ")";
        return doc;
    }

    Doc VisitExpr_(const PrimEqNode* op) override {
        Doc doc;
        doc << "(" << Print(op->a) << " == " << Print(op->b) << ")";
        return doc;
    }

    Doc VisitExpr_(const PrimNeNode* op) override {
        Doc doc;
        doc << "(" << Print(op->a) << " != " << Print(op->b) << ")";
        return doc;
    }

    Doc VisitExpr_(const PrimLtNode* op) override {
        Doc doc;
        doc << "(" << Print(op->a) << " < " << Print(op->b) << ")";
        return doc;
    }

    Doc VisitExpr_(const PrimLeNode* op) override {
        Doc doc;
        doc << "(" << Print(op->a) << " <= " << Print(op->b) << ")";
        return doc;
    }

    Doc VisitExpr_(const PrimGtNode* op) override {
        Doc doc;
        doc << "(" << Print(op->a) << " > " << Print(op->b) << ")";
        return doc;
    }

    Doc VisitExpr_(const PrimGeNode* op) override {
        Doc doc;
        doc << "(" << Print(op->a) << " >= " << Print(op->b) << ")";
        return doc;
    }

    Doc VisitExpr_(const PrimAndNode* op) override {
        Doc doc;
        doc << "(" << Print(op->a) << " && " << Print(op->b) << ")";
        return doc;
    }

    Doc VisitExpr_(const PrimOrNode* op) override {
        Doc doc;
        doc << "(" << Print(op->a) << " || " << Print(op->b) << ")";
        return doc;
    }

    Doc VisitExpr_(const PrimNotNode* op) override {
        Doc doc;
        doc << "(!" << Print(op->a) << ")";
        return doc;
    }

    Doc VisitExpr_(const PrimSubNode* op) override {
        Doc doc;
        doc << "(" << Print(op->a) << " - " << Print(op->b) << ")";
        return doc;
    }

    Doc VisitExpr_(const PrimDivNode* op) override {
        Doc doc;
        doc << "(" << Print(op->a) << " / " << Print(op->b) << ")";
        return doc;
    }

    Doc VisitExpr_(const PrimModNode* op) override {
        Doc doc;
        doc << "(" << Print(op->a) << " % " << Print(op->b) << ")";
        return doc;
    }

    Doc VisitStmt_(const EvaluateNode* op) override {
        Doc doc;
        doc << Doc::Text("  ") << Print(op->value);
        return doc;
    }

    Doc VisitStmt_(const SeqStmtNode* op) override {
        Doc doc;
        for (const Stmt& stmt : op->s) {
            doc << Print(stmt) << Doc::NewLine();
        }
        return doc;
    }

    Doc VisitStmt_(const AllocaVarStmtNode* op) override {
        Doc doc;
        doc << "Alloca " << Print(op->var);
        doc << " = " << Print(op->init_value);
        return doc;
    }

    Doc VisitStmt_(const AssignStmtNode* op) override {
        Doc doc;
        doc << "Assign " << Print(op->u) << " = " << Print(op->v); 
        return doc;
    }

    Doc VisitStmt_(const ReturnStmtNode* op) override {
        Doc doc;
        doc << "Return " << Print(op->value);
        return doc;
    }

    Doc VisitStmt_(const ClassStmtNode* op) override {
        Doc doc;
        doc << "class " << op->name.c_str() << ":";

        for (size_t i = 0; i < op->body.size(); ++i) {
            doc << Doc::NewLine();
            doc << Doc::Indent(2, Print(op->body[i]));
        }
        return doc;
    }

    Doc VisitStmt_(const IfStmtNode* op) override {
        Doc doc;
        doc << "if (" << Print(op->cond) << "):" << Doc::NewLine();
        doc << Doc::Indent(2, Print(op->then_case));
        if (!op->else_case.none()) {
            doc << Doc::NewLine();
            doc << "else:" << Doc::NewLine();
            doc << Doc::Indent(2, Print(op->else_case));
        }
        return doc;
    }

    Doc VisitStmt_(const WhileStmtNode* op) override {
        Doc doc;
        doc << "while (" << Print(op->cond) << "):" << Doc::NewLine();
        doc << Doc::Indent(2, Print(op->body));
        return doc;
    }

    Doc VisitType_(const PrimTypeNode* node) override {
        Doc doc;
        doc << PrintDataType(node->datatype);
        return doc;
    }


    Doc PrintFunc(const PrimFunc& func);


private:
    size_t var_counter_{0};

    std::unordered_map<BaseExpr, Doc, object_s, object_e> var_memo_;

    std::unordered_map<Type, Doc, object_s, object_e> t_memo_;

    std::unordered_map<std::string, int> names_;

};

class TextPrinter {
public:
    explicit TextPrinter() : ast_printer_() {}

    AstPrinter ast_printer_;

    Doc Print(const object_r& n) {
        Doc doc;
        if (n->IsType<IRModuleNode>()) {
            doc << PrintModule(Downcast<IRModule>(n));
        } else {
            doc << ast_printer_.Print(n);
        }
        return doc;
    }

    Doc PrintModule(const IRModule& m);
};

} // namespace runtime
    
} // namespace mc
