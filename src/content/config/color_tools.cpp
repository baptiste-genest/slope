#include "content/config/color_tools.h"
#include "content/config/Options.h"
#include <spdlog/spdlog.h>

namespace slope {

Color::Color(std::string l, ColorType def) : label(l)
{
    // a project written before colours moved into Params keeps its file, and
    // it is worth more than the default the caller passed
    std::ifstream f(slope::Options::ProjectViewsPath + label + ".color");
    if (f) {
        ColorType c;
        if (f >> c.x >> c.y >> c.z >> c.w)
            def = c;
    }
    handle = Params::AddColor(label, RGBA(def.x,def.y,def.z,def.w));
}

}
