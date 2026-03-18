#include "map.h"
#include "registry.h"


namespace mc {
namespace runtime {

REGISTER_TYPEINDEX(MapNode);

static McValue NewMap(Parameters gs) {
    std::unordered_map<object_r, object_r, object_s, object_e> data;
    for (int i = 0; i < gs.size(); i+= 2) {
        object_r k = AsObject(gs[i]);
        object_r v = AsObject(gs[i+1]);
        data.emplace(std::move(k), std::move(v));
    }
    return Map<object_r, object_r>(std::move(data));
}

REGISTER_FUNCTION("runtime.Map", NewMap);

} // namespace runtime

} // namespace mc