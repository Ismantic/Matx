#include "visitor.h"
#include "container.h"
#include "registry.h"

namespace mc {
namespace runtime {

McValue NodeGetAttrNames(object_t* o) {
    if (!o) return McValue();

    NodeAttrNameCollector c;
    o->VisitAttrs(&c);

    Tuple t = Tuple(c.names.begin(), c.names.end());
    return t;
}

McValue NodeGetAttr(object_t* o, const std::string& n) {
    if (!o) return McValue();

    NodeAttrGetter g(n);
    o->VisitAttrs(&g);
    return g.GetValue();
}

static McValue _GetAttrNames(Parameters gs) {
    object_t* s = static_cast<object_t*>(gs[0].value().u.v_pointer);
    auto ns = NodeGetAttrNames(s);
    return ns;
}

static McValue _GetAttr(Parameters gs) {
    object_t* s = static_cast<object_t*>(gs[0].value().u.v_pointer);
    auto n = gs[1].As<std::string>();
    return NodeGetAttr(s, n);
}

REGISTER_FUNCTION("runtime.NodeGetAttrNames", _GetAttrNames);

REGISTER_FUNCTION("runtime.NodeGetAttr", _GetAttr);

} // namespace runtime
} // namespace mc