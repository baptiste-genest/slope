#ifndef DECK_JSONREAD_H
#define DECK_JSONREAD_H

#include "libslope.h"
#include "content/authoring/Snippet.h"
#include "extern/json.hpp"

namespace slope {

class Color;
struct LiveTransform;

// Yaml accepts [0.5] as well as [x, y], and reading past the end of a json array is undefined and not an error,
// so every read is checked by these functions. `what` names the field in error messages.
vec2 readVec2(const json& v, const std::string& what);
vec readVec3(const json& v, const std::string& what);
// Reads [x,y,z], or the name of a snippet variable that is read every frame.
LiveVec readLiveVec(const json& v, const std::string& what);
// Reads a number, or the name of a snippet variable that is read every frame.
LiveScalar readLiveScalar(const json& v, const std::string& what);
// Reads {pos, scale, axis, angle}, each one a value or a snippet name.
LiveTransform readTransform(const json& t);
// Reads a vector or a color with no check of the size.
vec2 parseVec2(const json& v);
vec parseVec3(const json& v);
RGBA parseColor(const json& c);
// Reads [r,g,b] or [r,g,b,a] or "#rrggbb". Any other text is the name of a color parameter or of a snippet.
Color readColor(const json& c, const glm::vec4& def);
// Reads the name of a snippet section and checks that it exists in the loaded files.
std::string requireSection(const json& v, const std::string& what);

} // namespace slope

#endif // DECK_JSONREAD_H
