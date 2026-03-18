#include "array.h"
#include "registry.h"


namespace mc {
namespace runtime {

REGISTER_TYPEINDEX(ArrayNode);

static McValue NewArray(Parameters gs) {
    std::vector<Object> data;
    data.reserve(gs.size());
    for (int i = 0; i < gs.size(); ++i) {
        data.push_back(AsObject(gs[i]));
    }
    return Array<Object>(data);
}

REGISTER_FUNCTION("runtime.Array", NewArray);

/*
REGISTER_GLOBAL("runtime.Array")
    .SetBody([](Parameters gs) -> McValue {
    std::vector<Object> data;
    for (int i = 0; i < gs.size(); ++i) {
        data.push_back(AsObject(gs[i]));
    }
    return Array<Object>(data);
});
*/

} // namespace runtime

} // namespace mc