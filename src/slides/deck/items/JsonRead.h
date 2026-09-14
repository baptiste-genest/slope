#ifndef DECK_JSONREAD_H
#define DECK_JSONREAD_H

#include "libslope.h"
#include "content/authoring/Snippet.h"
#include "extern/json.hpp"

namespace slope {

class Color;

// yaml takes [0.5] as happily as [x, y], and reading past the end of a json
// array is undefined rather than an error, so every read is checked here
vec2 readVec2(const json& v, const std::string& what);
vec  readVec3(const json& v, const std::string& what);
// [x,y,z], or the name of a snippet variable read every frame
LiveVec readLiveVec(const json& v, const std::string& what);
vec2 parseVec2(const json& v);
vec  parseVec3(const json& v);
RGBA parseColor(const json& c);
// [r,g,b(,a)] or "#rrggbb", otherwise the name of a colour parameter or snippet
Color readColor(const json& c, const glm::vec4& def);

}

#endif // DECK_JSONREAD_H
