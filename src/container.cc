#include "container.h"
#include "registry.h"

#include <iostream>

namespace mc {
namespace runtime {

static McValue NewTuple(Parameters gs) {
    std::vector<McValue> fs;
    for(auto i = 0; i < gs.size(); ++i) {
        fs.push_back(McValue(gs[i]));
    }
    return Tuple(fs.begin(), fs.end());
}

static McValue GetTupleSize(Parameters gs) {
    const auto& t = gs[0].As<Tuple>();
    return static_cast<int64_t>(t.size());
}

static McValue GetTupleField(Parameters gs) {
    const auto& t = gs[0].As<Tuple>();
    int64_t i = gs[1].As<int64_t>();
    return t[i];
}

REGISTER_TYPEINDEX(TupleNode);

REGISTER_FUNCTION("runtime.Tuple", NewTuple);
REGISTER_FUNCTION("runtime.GetTupleSize", GetTupleSize);
REGISTER_FUNCTION("runtime.GetTupleField", GetTupleField);

} // namespace runtime
} // namespace mc