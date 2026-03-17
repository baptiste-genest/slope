#ifndef BACKGROUDCOLOR_H
#define BACKGROUDCOLOR_H

#include "../color_tools.h"
#include "PolyscopePrimitive.h"
#include "../../math/utils.h"

namespace slope {


class BackgroundColor;
using BackgroundColorPtr = std::shared_ptr<BackgroundColor>;

class BackgroundColor : public PolyscopePrimitive
{
public:

    static Color Default;

    BackgroundColor(Color _color) {
        color = _color;
    }

    static BackgroundColorPtr Add(float r,float g,float b) {
        return std::make_shared<BackgroundColor>(Color(r,g,b));
    }

    static BackgroundColorPtr Add(std::string palette_name) {
        return std::make_shared<BackgroundColor>(Color(palette_name));
    }

    // Primitive interface
protected:
    Color color;
    virtual void draw(const TimeObject &time, const StateInSlide &sis) override {
        forceEnable();
    }
    virtual void playIntro(const TimeObject &t, const StateInSlide &sis) override {
        polyscope::view::bgColor = Lerp(Default,color,t.transitionParameter).toArray();
    }
    virtual void playOutro(const TimeObject &t, const StateInSlide &sis) override {
        polyscope::view::bgColor = Lerp(Default,color,1-t.transitionParameter).toArray();
    }
    virtual void forceDisable() override {
        polyscope::view::bgColor = Default.toArray();;
    }
    virtual void forceEnable() override {
        polyscope::view::bgColor = color.toArray();
    }
};

}

#endif // BACKGROUDCOLOR_H
