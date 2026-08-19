#include "JsonRead.h"

namespace slope {

vec2 readVec2(const json& v, const std::string& what)
{
    if (!v.is_array() || v.size() != 2)
        throw std::runtime_error("\"" + what + "\" must be [x, y]");
    return vec2(v[0].get<scalar>(), v[1].get<scalar>());
}

vec readVec3(const json& v, const std::string& what)
{
    if (!v.is_array() || v.size() != 3)
        throw std::runtime_error("\"" + what + "\" must be [x, y, z]");
    return vec(v[0].get<scalar>(), v[1].get<scalar>(), v[2].get<scalar>());
}

LiveVec readLiveVec(const json& v, const std::string& what)
{
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

vec2 parseVec2(const json& v)
{
    if (!v.is_array() || v.size() != 2)
        throw std::runtime_error("a vec2 uniform default must be [x, y]");
    return vec2(v[0].get<scalar>(), v[1].get<scalar>());
}

vec parseVec3(const json& v)
{
    if (!v.is_array() || v.size() != 3)
        throw std::runtime_error("a vec3 uniform default must be [x, y, z]");
    return vec(v[0].get<scalar>(), v[1].get<scalar>(), v[2].get<scalar>());
}

RGBA parseColor(const json& c)
{
    if (c.is_array()) {
        if (c.size() < 3 || c.size() > 4)
            throw std::runtime_error("color must be [r,g,b] or [r,g,b,a]");
        return RGBA((float)c[0], (float)c[1], (float)c[2],
                    c.size() > 3 ? (float)c[3] : 1.f);
    }
    std::string s = c;
    if (s.size() < 7 || s[0] != '#')
        throw std::runtime_error("color must be [r,g,b(,a)] or \"#rrggbb\"");
    auto hex = [&](int i) { return std::stoi(s.substr(i, 2), nullptr, 16); };
    return RGBA(hex(1)/255.f, hex(3)/255.f, hex(5)/255.f, 1.f);
}

}
