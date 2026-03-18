#pragma once

#include "ScreenPrimitive.h"
#include "../color_tools.h"


namespace slope {

class FixedBox;
using AbsoluteBoxPtr = std::shared_ptr<FixedBox>;

void DrawRectangle(const vec2& center, const vec2& size, const ImColor& color, float roundness);

class Box {
public:
    Color color;
    float roundness;

    Box(Color c,float roundness) : color(c), roundness(roundness) {}
    Box(){}
protected:

    void drawBox(const vec2& pos,const vec2& size,Color c,float roundness,float alpha) const;
};

class FixedBox : public ScreenPrimitive, public Box
{
public:

    FixedBox(vec2 _size,Color _color,float r) : Box(_color,r),size(_size) {
    }

    static AbsoluteBoxPtr Add(vec2 size,Color color = Color(1,0.3,1),float round = Options::DefaultBoxRoundness)
    {
        return NewPrimitive<FixedBox>(size,color,round);
    }

    // Primitive interface
protected:

    vec2 size;

    virtual void draw(const TimeObject &time, const StateInSlide &sis) override {
        drawBox(sis.getPosition(),size,color,roundness,sis.alpha);
    }
    virtual void playIntro(const TimeObject &t, const StateInSlide &sis) override {
        auto sist = transition.intro(t,sis);;
        drawBox(sist.getPosition(),size,color,roundness,sist.alpha);
    }
    virtual void playOutro(const TimeObject &t, const StateInSlide &sis) override {
        auto sist = transition.outro(t,sis);;
        drawBox(sist.getPosition(),size,color,roundness,sist.alpha);
    }

    // ScreenPrimitive interface
public:
    virtual vec2 getSize() const override;
};

class EnglobingBox;
using EnglobingBoxPtr = std::shared_ptr<EnglobingBox>;


class EnglobingBox : public ScreenPrimitive, public Box
{
    using InsideType = ScreenPrimitiveInSlide;
    struct EnglobingParams {
        vec2 pos,bbox;
        int min_depth;
    };
    EnglobingParams ComputeEnglobing(const std::vector<InsideType>& primitives) const;
    std::vector<InsideType> primitivesInside;
    scalar padding;


public:


    EnglobingBox(Color c,float r, float padding, const std::vector<InsideType>& primitives) : primitivesInside(primitives) {
        this->color = c;
        roundness = r;
        depth = -100;
        this->padding = padding;
    }

    static EnglobingBoxPtr Add(Color c,float r,float padding, const std::vector<InsideType>& primitives)
    {
        auto rslt = NewPrimitive<EnglobingBox>(c,r,padding,primitives);

        auto E = rslt->ComputeEnglobing(primitives);
        rslt->setDepth(E.min_depth-1);

        return rslt;
    }

    template<typename... Args>
    static EnglobingBoxPtr Add(Color c, float r,float padding, const Args&... primitives)
    {
        static_assert((std::is_same_v<Args, ScreenPrimitiveInSlide> && ...),
                      "All arguments must be ScreenPrimitiveInSlide");
        std::vector<ScreenPrimitiveInSlide> pack{primitives...};

        return Add(c,r,padding,pack);
    }

    vec2 getSize() const override {
        vec2 rslt = ComputeEnglobing(primitivesInside).bbox;
        auto W = ImGui::GetWindowSize();
        return vec2(rslt(0)*W.x, rslt(1)*W.y);
    }

protected:
    virtual void draw(const TimeObject &time, const StateInSlide &sis) override {
        auto E = ComputeEnglobing(primitivesInside);
        drawBox(E.pos,E.bbox,color,roundness,sis.alpha);
    }
    virtual void playIntro(const TimeObject &t, const StateInSlide &sis) override {
        auto sist = transition.intro(t,sis);;
        auto E = ComputeEnglobing(primitivesInside);
        drawBox(E.pos,E.bbox,color,roundness,sist.alpha);
    }
    virtual void playOutro(const TimeObject &t, const StateInSlide &sis) override {
        auto sist = transition.outro(t,sis);;
        auto E = ComputeEnglobing(primitivesInside);
        drawBox(E.pos,E.bbox,color,roundness,sist.alpha);
    }
};


} // namespace slope

