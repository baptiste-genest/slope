#include "slides/deck/items/JsonRead.h"
#include "slides/deck/items/DeckItem.h"
#include "content/authoring/color_tools.h"
#include "content/polyscope_primitives/LiveTransform.h"
#include "content/authoring/Snippet.h"
#include <spdlog/spdlog.h>

namespace slope {

vec2 readVec2(const json& v, const std::string& what) {
    if (!v.is_array() || v.size() != 2)
        throw std::runtime_error("\"" + what + "\" must be [x, y]");
    return vec2(v[0].get<scalar>(), v[1].get<scalar>());
}

vec readVec3(const json& v, const std::string& what) {
    if (!v.is_array() || v.size() != 3)
        throw std::runtime_error("\"" + what + "\" must be [x, y, z]");
    return vec(v[0].get<scalar>(), v[1].get<scalar>(), v[2].get<scalar>());
}

LiveVec readLiveVec(const json& v, const std::string& what) {
    LiveVec l;
    // yaml reads a bare n, y, on or off as a boolean, never as a variable name
    if (v.is_boolean())
        throw std::runtime_error("\"" + what + "\" read as a boolean. Quote snippet names "
                                               "like \"n\", \"y\", \"on\" or \"off\"");
    if (v.is_string())
        l.snippet = v.get<std::string>();
    else
        l.fixed = readVec3(v, what);
    return l;
}

LiveScalar readLiveScalar(const json& v, const std::string& what) {
    if (v.is_boolean())
        throw std::runtime_error("\"" + what + "\" read as a boolean. Quote snippet names "
                                               "like \"n\", \"y\", \"on\" or \"off\"");
    if (v.is_string())
        return LiveScalar(v.get<std::string>());
    if (!v.is_number())
        throw std::runtime_error("\"" + what + "\" must be a number or a snippet name");
    return LiveScalar(v.get<scalar>());
}

LiveTransform readTransform(const json& t) {
    if (!t.is_object())
        throw std::runtime_error("\"transform\" must be a map of pos, scale, axis and angle");
    for (const auto& [key, val] : t.items())
        if (key != "pos" && key != "scale" && key != "axis" && key != "angle")
            deckWarn("ignored key \"{}\" in \"transform\", which takes pos, scale, "
                     "axis and angle",
                     key);

    LiveTransform T;
    if (t.contains("pos"))
        T.pos = readLiveVec(t["pos"], "transform.pos");
    if (t.contains("scale")) {
        const json& s = t["scale"];
        T.scale = s.is_number() ? LiveVec(vec::Constant(s.get<scalar>()))
                                : readLiveVec(s, "transform.scale");
    }
    if (t.contains("axis"))
        T.axis = readLiveVec(t["axis"], "transform.axis");
    if (t.contains("angle"))
        T.angle = readLiveScalar(t["angle"], "transform.angle");
    return T;
}

vec2 parseVec2(const json& v) {
    if (!v.is_array() || v.size() != 2)
        throw std::runtime_error("a vec2 uniform default must be [x, y]");
    return vec2(v[0].get<scalar>(), v[1].get<scalar>());
}

vec parseVec3(const json& v) {
    if (!v.is_array() || v.size() != 3)
        throw std::runtime_error("a vec3 uniform default must be [x, y, z]");
    return vec(v[0].get<scalar>(), v[1].get<scalar>(), v[2].get<scalar>());
}

RGBA parseColor(const json& c) {
    if (c.is_array()) {
        if (c.size() < 3 || c.size() > 4)
            throw std::runtime_error("color must be [r,g,b] or [r,g,b,a]");
        return RGBA((float)c[0], (float)c[1], (float)c[2],
                    c.size() > 3 ? (float)c[3] : 1.f);
    }
    std::string s = c;
    if (s.size() != 7 || s[0] != '#' || s.find_first_not_of("0123456789abcdefABCDEF", 1) != std::string::npos)
        throw std::runtime_error("color must be [r,g,b(,a)] or \"#rrggbb\", not \"" + s + "\"");
    auto hex = [&](int i) { return std::stoi(s.substr(i, 2), nullptr, 16); };
    return RGBA(hex(1) / 255.f, hex(3) / 255.f, hex(5) / 255.f, 1.f);
}

Color readColor(const json& c, const glm::vec4& def) {
    if (c.is_boolean())
        throw std::runtime_error("\"color\" read as a boolean. Quote names like \"on\" or \"off\"");
    if (c.is_string() && !c.get<std::string>().starts_with('#'))
        return Color(c.get<std::string>(), def);
    const RGBA v = parseColor(c);
    return Color(v.Value.x, v.Value.y, v.Value.z, v.Value.w);
}

std::string requireSection(const json& v, const std::string& what) {
    if (!v.is_string())
        throw std::runtime_error("\"" + what + "\" takes the name of a snippet section");
    const std::string name = v.get<std::string>();
    // with no file loaded yet, a C++ one may still come, the first frame reports it
    if (Snippet::loadedAny() && !Snippet::hasSection(name))
        throw std::runtime_error("\"" + what + ": " + name + "\" : no snippet section called \"" + name + "\"");
    return name;
}

} // namespace slope
