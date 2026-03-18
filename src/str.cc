#include "str.h"
#include "visitor.h"
#include "registry.h"

namespace mc {
namespace runtime {

void StrNode::VisitAttrs(AttrVisitor* v) {
    v->Visit("data", &data_);
}

REGISTER_TYPEINDEX(StrNode);

REGISTER_GLOBAL("runtime.Str")
    .SetBody([](std::string str){
        return Str(str);
});

} // namespace runtime

} // namespace mc