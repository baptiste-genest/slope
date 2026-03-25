#pragma once
#include "../libslope.h"
#include "Options.h"
#include "io.h"

namespace slope {

// Get the chromatic opposite of a color in HSV space, input and output are RGB
inline glm::vec3 GetChromaticOpposite(const glm::vec3& color_rgb) {
    glm::vec3 color_hsv = polyscope::RGBtoHSV(color_rgb);
    color_hsv[0] = std::fmod(color_hsv[0] + 0.5, 1.0);
    return polyscope::HSVtoRGB(color_hsv);
}

using ColorType = glm::vec4;

class PaletteHandler {
    static std::set<std::string> labels;
public:
    static void RegisterColor(const std::string& label) {
        if (!labels.contains(label)) {
            WriteToLabel(label, ColorType(0.4,0.1,0.8,1),false);
            labels.insert(label);
        }
    }
    static ColorType GetColorFromLabel(std::string label);

    static void WriteToLabel(std::string label, ColorType color,bool override);

    static void ShowColorPickingModule();
};

class Color {
    std::string label = "";
    ColorType value;

public:
    Color() {}
    Color(std::string label) : label(label) {PaletteHandler::RegisterColor(label);}

    Color(ColorType value) : value(value) {}

    Color(float r,float g,float b,float a = 1) : value(r,g,b,a) {}

    bool isPersistent() const { return !label.empty(); }

    ColorType getValue() const {
        if (isPersistent())
            return PaletteHandler::GetColorFromLabel(label);
        else
            return value;
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
