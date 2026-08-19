#ifndef DECK_JSONREAD_H
#define DECK_JSONREAD_H

#include "../../libslope.h"
#include "extern/json.hpp"

namespace slope {

// yaml takes [0.5] as happily as [x, y], and reading past the end of a json
// array is undefined rather than an error, so every read is checked here
vec2 readVec2(const json& v, const std::string& what);
vec  readVec3(const json& v, const std::string& what);
vec2 parseVec2(const json& v);
vec  parseVec3(const json& v);
RGBA parseColor(const json& c);

}

#endif // DECK_JSONREAD_H
