#ifndef BACKGROUDCOLOR_H
#define BACKGROUDCOLOR_H

#include "PolyscopePrimitive.h"
#include "../../math/utils.h"

namespace slope {


class BackgroudColor;
using BackgroudColorPtr = std::shared_ptr<BackgroudColor>;

class BackgroudColor : public PolyscopePrimitive
{
public:
    BackgroudColor(vec _color) {
        color = _color;
    }

    static BackgroudColorPtr Add(float r,float g,float b) {
        return std::make_shared<BackgroudColor>(vec(r,g,b));
    }

    // Primitive interface
protected:
    static std::array<float,4> toArray(const vec& color) {
        std::array<float,4> col;
        for (int i = 0;i<3;i++)
            col[i] = color(i);
        col[3] = 1;
        return col;
    }
    vec color;
    virtual void draw(const TimeObject &time, const StateInSlide &sis) override {
        forceEnable();
    }
    virtual void playIntro(const TimeObject &t, const StateInSlide &sis) override {
        polyscope::view::bgColor = toArray(lerp<vec>(color,Options::DefaultBackgroundColor,t.transitionParameter));
    }
    virtual void playOutro(const TimeObject &t, const StateInSlide &sis) override {
        polyscope::view::bgColor = toArray(lerp<vec>(color,Options::DefaultBackgroundColor,1-t.transitionParameter));
    }
    virtual void forceDisable() override {
        polyscope::view::bgColor = toArray(Options::DefaultBackgroundColor);
    }
    virtual void forceEnable() override {
        polyscope::view::bgColor = toArray(color);
    }
};

}

#endif // BACKGROUDCOLOR_H
