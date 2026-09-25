#pragma once
#include "libslope.h"
#include "content/config/Options.h"
#include "content/config/io.h"
#include "content/authoring/Params.h"
#include "content/authoring/Snippet.h"

namespace slope {

// Returns the color with the hue shifted by half a turn. Input and output are RGB.
inline glm::vec3 GetChromaticOpposite(const glm::vec3& color_rgb) {
    glm::vec3 color_hsv = polyscope::RGBtoHSV(color_rgb);
    color_hsv[0] = std::fmod(color_hsv[0] + 0.5, 1.0);
    return polyscope::HSVtoRGB(color_hsv);
}

// RGBA color with components in [0,1].
using ColorType = glm::vec4;

/*
 * A color, either a fixed value or a named one.
 *
 * A named color is a Params entry like any other. It is edited in the Tuner,
 * saved to views/params.json with Ctrl+S and hot reloaded.
 * A snippet can read it and a shader uniform can use it under the same name.
 * Names can be grouped, as in "shape/clay".
 * A views/<name>.color file from the old palette is still read once.
 */
class Color {
    std::string label = "";
    ColorType value = ColorType(1,1,1,1);
    mutable Params::ColorParam handle;

public:
    // White.
    Color() {}
    // Named color, with a default used until it is edited.
    Color(std::string label, ColorType def = ColorType(0.4,0.1,0.8,1));

    // Fixed color.
    Color(ColorType value) : value(value) {}

    Color(float r,float g,float b,float a = 1) : value(r,g,b,a) {}

    // True for a named color.
    bool isPersistent() const { return !label.empty(); }

    // Current value, read from the snippet or parameter of that name when named.
    ColorType getValue() const {
        if (label.empty())
            return value;
        // A snippet can own the name instead, since names are shared with Params.
        if (Snippet::get(label).valid()) {
            const RGBA c = Snippet::get(label, 4).rgba();
            return ColorType(c.Value.x,c.Value.y,c.Value.z,c.Value.w);
        }
        // Declared here only when no section did it before, to avoid a name clash.
        if (!handle.entry)
            handle = Params::AddColor(label, RGBA(value.x,value.y,value.z,value.w));
        const RGBA c = *handle;
        return ColorType(c.Value.x,c.Value.y,c.Value.z,c.Value.w);
    }

    // Current value as an ImColor.
    ImColor getImColor() const {
        ColorType c = getValue();
        return ImColor(c.x,c.y,c.z,c.w);
    }

    // Current value as r, g, b, a.
    std::array<float,4> toArray() const {
        ColorType c = getValue();
        return  {c.x,c.y,c.z,c.w};
    }
};

// Linear blend of two colors, giving c1 at t=0 and c2 at t=1.
inline Color Lerp(const Color& c1,const Color& c2,float t) {
    Color result;
    auto v1 = c1.getValue();
    auto v2 = c2.getValue();
    result = Color(v1 + t*(v2-v1));
    return result;
}

}
