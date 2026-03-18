#include "printer.h"
#include "registry.h"

#include <iostream>


namespace mc {
namespace runtime {

class DocTextNode : public DocAtomNode {
public:
    static constexpr const std::string_view NAME = "printer.DocText";
    DEFINE_TYPEINDEX(DocTextNode, DocAtomNode);

    std::string str;
    explicit DocTextNode(std::string str) : str(str) {}
};

REGISTER_TYPEINDEX(DocTextNode);

class DocText : public DocAtom {
public:
    DEFINE_NODE_CLASS(DocText, DocAtom, DocTextNode);

    explicit DocText(std::string str) {
        if (str.find_first_of("\t\n") != str.npos) {
            std::cerr << "text node: '" << str << "' should not has tab or newline.";
            std::cerr << "\n";
        }
        data_ = MakeObject<DocTextNode>(str);
    }
};

class DocLineNode : public DocAtomNode {
public:
    static constexpr const std::string_view NAME = "printer.DocLineNode";
    DEFINE_TYPEINDEX(DocLineNode, DocAtomNode);

    int indent;
    explicit DocLineNode(int indent) : indent(indent) {}
};

REGISTER_TYPEINDEX(DocLineNode);

class DocLine : public DocAtom {
public:
    DEFINE_NODE_CLASS(DocLine, DocAtom, DocLineNode);

    explicit DocLine(int indent) {
        data_ = MakeObject<DocLineNode>(indent);
    }
};

Doc& Doc::operator<<(const Doc& r) {
    this->stream_.insert(this->stream_.end(), r.stream_.begin(), r.stream_.end());
    return *this;
}

Doc& Doc::operator<<(const std::string& r) {
   return *this << DocText(r); 
}

Doc& Doc::operator<<(const DocAtom& r) {
    this->stream_.push_back(r);
    return *this;
}

std::string Doc::str() {
    std::ostringstream os;
    for (auto atom : this->stream_) {
        if (auto* text = atom.As<DocTextNode>()) {
            os << text->str;
        } else if (auto* line = atom.As<DocLineNode>()) {
            os << "\n" << std::string(line->indent, ' ');
        } else {
            std::cerr << "do not expect type " << atom->Name();
        }
    }
    return os.str();
}

Doc Doc::NewLine(int indent) {
    return Doc() << DocLine(indent);
}

Doc Doc::Text(std::string text) {
    return Doc() << DocText(text);
}

Doc Doc::RawText(std::string text) {
    return Doc() << DocAtom(MakeObject<DocTextNode>(text));
}


  Doc Doc::Indent(int indent, Doc doc) {
      if (doc.stream_.empty()) {
          return doc;
      }

      Doc result;
      bool first_item = true;

      for (const auto& atom : doc.stream_) {
          if (auto* text = atom.As<DocTextNode>()) {
              if (first_item) {
                  // 第一个文本项前面加缩进空格
                  result << DocText(std::string(indent, ' ')) << atom;
                  first_item = false;
              } else {
                  result << atom;
              }
          } else if (auto* line = atom.As<DocLineNode>()) {
              // 所有换行都增加缩进
              result << DocLine(indent + line->indent);
          } else {
              result << atom;
          }
      }

      return result;
  }


Doc Doc::Brace(std::string open, const Doc& body, std::string close, int indent) {
  Doc doc;
  doc << open;
  doc << Indent(indent, NewLine() << body) << NewLine();
  doc << close;
  return doc;
}

Doc Doc::Concat(const std::vector<Doc>& vec, const Doc& sep) {
  Doc seq;
  if (vec.size() != 0) {
    if (vec.size() == 1)
      return vec[0];
    seq << vec[0];
    for (size_t i = 1; i < vec.size(); ++i) {
      seq << sep << vec[i];
    }
  }
  return seq;
}

Doc AstPrinter::Print(const Object& node) {
    // TODO: !node.Define
    if (node.As<StmtNode>()) {
        return VisitStmt(Downcast<Stmt>(node));
    } 
    if (node.As<PrimExprNode>()) {
        return VisitExpr(Downcast<PrimExpr>(node));
    }
    if (node.As<PrimFuncNode>()) {
        return PrintFunc(Downcast<PrimFunc>(node));
    }
    if (node.As<TypeNode>()) {
        return PrintType(Downcast<Type>(node));
    }
    std::stringstream ss;
    ss << node->Name() << "@" << node.get();
    return Doc::Text(ss.str());
}

Doc AstPrinter::PrintFunc(const PrimFunc& func) {
    Doc doc;

    doc << "fn(";
    for (size_t i = 0; i < func->gs.size(); ++i) {
        if (i > 0) doc << ", ";
        doc << func->gs[i]->var_name << ": ";
        doc << (func->gs[i]->datatype == DataType::Bool() ? "bool" : "int32");
    }
    doc << ") -> ";
    if (auto* ty = func->rt.As<PrimTypeNode>()) {
        doc << (ty->datatype == DataType::Bool() ? "bool" : "int32");
    }
    
    doc << " {" << Doc::NewLine();
    if (!func->attrs.none()) {
        doc << "Attr = {";
        for (const auto& p : func->attrs->dict) {
            doc << p.first.c_str() << " = " << Downcast<Str>(p.second).c_str() << ", ";
        }
        doc << "}";
    }
    //doc << Doc::NewLine();
    doc <<  Print(func->body);
    doc << "}";

    return doc;
}


Doc AstPrinter::PrintType(const Type& t) {
    //if (auto* pt = t.As<PrimTypeNode>()) {
    //    return PrintDataType(pt->datatype);
    //}
    //return Doc::Text(t->Name());
    auto it = t_memo_.find(t);
    if (it != t_memo_.end()) {
        return it->second;
    }
    Doc doc;
    doc = VisitType(t);
    t_memo_[t] = doc;
    return doc;
}

Doc AstPrinter::PrintDataType(DataType datatype) {
    std::stringstream ss;
    if (datatype == DataType::Bool()) {
        ss << "bool";
    } else if (datatype == DataType::Int(32)) {
        ss << "int32";
    } else if (datatype == DataType::Int(64)) {
        ss << "int64";
    } else if (datatype == DataType::Handle()) {
        ss << "handle";
    } else {
        ss << "UNK";
    }
    return Doc::Text(ss.str());
}

Doc AstPrinter::GetName(const std::string& p) {
    auto it = names_.find(p);
    if (it != names_.end()) {
        return Doc::Text(p + std::to_string(++it->second));
    }
    names_[p] = 0;
    return Doc::Text(p);
}

Doc AstPrinter::PrintFunction(const PrimFunc& func) {
    Doc doc;

    doc << "fn("; // TODO
    for (size_t i = 0; i < func->gs.size(); ++i) {
        if (i > 0) doc << ", ";
        doc << func->gs[i]->var_name << ": ";
        doc << (func->gs[i]->datatype == DataType::Bool() ? "bool" : "int32");
    }
    doc << ") -> ";
    if (auto* ty = func->rt.As<PrimTypeNode>()) {
        doc << (ty->datatype == DataType::Bool() ? "bool" : "int32");
    }
    
    doc << " {" << Doc::NewLine();
    doc << Doc::Indent(2, Print(func->body));
    doc << "}";

    return doc;
}

Doc AstPrinter::PrintFunction(const AstFunc& func) {
    Doc doc;

    doc << "fn("; // TODO
    for (size_t i = 0; i < func->gs.size(); ++i) {
        //if (i > 0) doc << ", ";
        //doc << func->gs[i].var_name() << ": ";
        //doc << (func->gs[i]->datatype == DataType::Bool() ? "bool" : "int32");
    }
    doc << ") -> ";
    if (auto* ty = func->rt.As<PrimTypeNode>()) {
        doc << (ty->datatype == DataType::Bool() ? "bool" : "int32");
    }
    
    doc << " {" << Doc::NewLine();
    doc << Doc::Indent(2, Print(func->body));
    doc << "}";

    return doc;
}

Doc AstPrinter::PrintFunction(const Doc& p, const PrimFunc& func) {
    Doc doc;
    doc << p;
    doc << PrintFunction(func);
    return doc;
}

Doc AstPrinter::PrintFunction(const Doc& p, const AstFunc& func) {
    Doc doc;
    doc << p;
    doc << PrintFunction(func);
    return doc;
}

Doc TextPrinter::PrintModule(const IRModule& m) {
    Doc doc;
    int counter = 0;

    for (const auto& p : m->func_) {
        if (counter++ != 0)  {
            doc << Doc::NewLine();
        }
        std::ostringstream os;
        os << "fn @" << p.first.var_name() << " ";
        //doc << ast_printer_.PrintFunction(Doc::Text(os.str()), p.second);
        doc << Doc::NewLine();
    }

    return doc;
}

static std::string AsText(const Object& node) {
    Doc doc;
    doc << TextPrinter().Print(node);
    return doc.str();
}


REGISTER_GLOBAL("ast.AsText").SetBody(AsText);

static int64_t test_test(int64_t u, int64_t v) {
    return u+v;
}
REGISTER_GLOBAL("test_test").SetBody(test_test);

} // namespace runtime

} // namespace mc
