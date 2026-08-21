#pragma once
#include "libslope.h"
#include "content/config/Options.h"
#include "content/config/io.h"
#include "content/config/Params.h"

namespace slope {

// Get the chromatic opposite of a color in HSV space, input and output are RGB
inline glm::vec3 GetChromaticOpposite(const glm::vec3& color_rgb) {
    glm::vec3 color_hsv = polyscope::RGBtoHSV(color_rgb);
    color_hsv[0] = std::fmod(color_hsv[0] + 0.5, 1.0);
    return polyscope::HSVtoRGB(color_hsv);
}

using ColorType = glm::vec4;

/*
 * A colour, either a literal or a named one.
 *
 * A named colour is a Params entry like any other : edited in the Tuner, saved
 * to views/params.json with Ctrl+S, hot reloaded, and readable from a snippet
 * or bindable to a shader uniform by that same name. Names may be grouped,
 * "shape/clay". A project that still holds a views/<name>.color from the old
 * palette is read once, so the value carries over.
 */
class Color {
    std::string label = "";
    ColorType value = ColorType(1,1,1,1);
    Params::ColorParam handle;

public:
    Color() {}
    Color(std::string label, ColorType def = ColorType(0.4,0.1,0.8,1));

    Color(ColorType value) : value(value) {}

    Color(float r,float g,float b,float a = 1) : value(r,g,b,a) {}

    bool isPersistent() const { return !label.empty(); }

    ColorType getValue() const {
        if (!handle.entry)
            return value;
        const RGBA c = *handle;
        return ColorType(c.Value.x,c.Value.y,c.Value.z,c.Value.w);
    }

    ImColor getImColor() const {
        ColorType c = getValue();
        return ImColor(c.x,c.y,c.z,c.w);
    }

    std::array<float,4> toArray() const {
        ColorType c = getValue();
        return  {c.x,c.y,c.z,c.w};
    }
};

inline Color Lerp(const Color& c1,const Color& c2,float t) {
    Color result;
    auto v1 = c1.getValue();
    auto v2 = c2.getValue();
    result = Color(v1 + t*(v2-v1));
    return result;
}

}
