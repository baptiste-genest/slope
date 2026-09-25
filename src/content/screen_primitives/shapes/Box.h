#pragma once

#include "content/screen_primitives/ScreenPrimitive.h"
#include "content/authoring/color_tools.h"


namespace slope {

class FixedBox;
using AbsoluteBoxPtr = std::shared_ptr<FixedBox>;

// Color and corner roundness shared by the box primitives.
class Box {
public:
    Color color;
    float roundness;

    Box(Color c,float roundness) : color(c), roundness(roundness) {}
    Box(){}
protected:

    // Draws a filled rounded rectangle.
    void drawBox(const vec2& pos,const vec2& size,Color c,float roundness,float alpha) const;
};

// A rectangle of fixed size, in relative units.
class FixedBox : public ScreenPrimitive, public Box
{
public:

    FixedBox(vec2 _size,Color _color,float r) : Box(_color,r),size(_size) {
    }

    // Builds a box of the given relative size.
    static AbsoluteBoxPtr Add(vec2 size,Color color = Color(1,0.3,1),float round = Options::DefaultBoxRoundness)
    {
        return NewPrimitive<FixedBox>(size,color,round);
    }

    // Primitive interface
protected:

    vec2 size;

    virtual void draw(const TimeObject &time, const StateInSlide &sis) override {
        drawBox(sis.getPosition(),size,color,roundness,sis.getAlpha());
    }
    virtual void playIntro(const TimeObject &t, const StateInSlide &sis) override {
        drawBox(sis.getPosition(),size,color,roundness,sis.getAlpha());
    }
    virtual void playOutro(const TimeObject &t, const StateInSlide &sis) override {
        drawBox(sis.getPosition(),size,color,roundness,sis.getAlpha());
    }

    // ScreenPrimitive interface
public:
    // Size in pixels.
    virtual vec2 getSize() const override;
};

class EnglobingBox;
using EnglobingBoxPtr = std::shared_ptr<EnglobingBox>;


// A box drawn behind a set of primitives that covers all of them, with a padding around.
class EnglobingBox : public ScreenPrimitive, public Box
{
    using InsideType = ScreenPrimitiveInSlide;
    // Center and size of the box, and the smallest depth among the primitives inside.
    struct EnglobingParams {
        vec2 pos,bbox;
        int min_depth;
    };
    // Computes the box that covers the primitives.
    EnglobingParams ComputeEnglobing(const std::vector<InsideType>& primitives) const;
    std::vector<InsideType> primitivesInside;
    scalar padding;


public:


    // Builds a box with color c, roundness r and a padding in relative units.
    EnglobingBox(Color c,float r, float padding, const std::vector<InsideType>& primitives) : primitivesInside(primitives) {
        this->color = c;
        roundness = r;
        depth = -100;
        this->padding = padding;
    }

    // Builds a box around the primitives, drawn behind the deepest one.
    static EnglobingBoxPtr Add(Color c,float r,float padding, const std::vector<InsideType>& primitives)
    {
        auto rslt = NewPrimitive<EnglobingBox>(c,r,padding,primitives);

        auto E = rslt->ComputeEnglobing(primitives);
        rslt->setDepth(E.min_depth-1);

        return rslt;
    }

    // Same, with the primitives given as separate arguments.
    template<typename... Args>
    static EnglobingBoxPtr Add(Color c, float r,float padding, const Args&... primitives)
    {
        static_assert((std::is_same_v<Args, ScreenPrimitiveInSlide> && ...),
                      "All arguments must be ScreenPrimitiveInSlide");
        std::vector<ScreenPrimitiveInSlide> pack{primitives...};

        return Add(c,r,padding,pack);
    }

    // Size in pixels.
    vec2 getSize() const override {
        vec2 rslt = ComputeEnglobing(primitivesInside).bbox;
        auto W = ImGui::GetWindowSize();
        return vec2(rslt(0)*W.x, rslt(1)*W.y);
    }

protected:
    virtual void draw(const TimeObject &time, const StateInSlide &sis) override {
        auto E = ComputeEnglobing(primitivesInside);
        drawBox(E.pos,E.bbox,color,roundness,sis.getAlpha());
    }
    virtual void playIntro(const TimeObject &t, const StateInSlide &sis) override {
        auto E = ComputeEnglobing(primitivesInside);
        drawBox(E.pos,E.bbox,color,roundness,sis.getAlpha());
    }
    virtual void playOutro(const TimeObject &t, const StateInSlide &sis) override {
        auto E = ComputeEnglobing(primitivesInside);
        drawBox(E.pos,E.bbox,color,roundness,sis.getAlpha());
    }
};


} // namespace slope

