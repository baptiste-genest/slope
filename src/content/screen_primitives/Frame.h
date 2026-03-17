#ifndef SLOPE_FRAME_H
#define SLOPE_FRAME_H

#include "ScreenPrimitive.h"
#include "../color_tools.h"


namespace slope {

class AbsoluteFrame;
using AbsoluteFramePtr = std::shared_ptr<AbsoluteFrame>;

void DrawRectangle(const vec2& center, const vec2& size, const ImColor& color, float roundness);

class Frame {
public:
    Color color;
    float roundness;

    Frame(vec2 size,Color c,float roundness) : size(size), color(c), roundness(roundness) {}
    Frame(){}
protected:
    vec2 size;

    void drawFrame(const vec2& pos,const vec2& size,Color c,float roundness,float alpha) const{
        auto color = c.getImColor();
        color.Value.w *= alpha;
        DrawRectangle(pos, size, color, roundness);
    }
};

class AbsoluteFrame : public ScreenPrimitive, public Frame
{
public:

    AbsoluteFrame(vec2 _size,Color _color,float r) : Frame(_size,_color,r) {
        depth = -100;
    }

    static AbsoluteFramePtr Add(vec2 size,Color color = Color(1,0.3,1),float round = Options::DefaultFrameRoundness)
    {
        return NewPrimitive<AbsoluteFrame>(size,color,round);
    }

    // Primitive interface
protected:

    virtual void draw(const TimeObject &time, const StateInSlide &sis) override {
        drawFrame(sis.getPosition(),size,color,roundness,sis.alpha);
    }
    virtual void playIntro(const TimeObject &t, const StateInSlide &sis) override {
        auto sist = transition.intro(t,sis);;
        drawFrame(sist.getPosition(),size,color,roundness,sist.alpha);
    }
    virtual void playOutro(const TimeObject &t, const StateInSlide &sis) override {
        auto sist = transition.outro(t,sis);;
        drawFrame(sist.getPosition(),size,color,roundness,sist.alpha);
    }

    // ScreenPrimitive interface
public:
    virtual vec2 getSize() const override;
};

class EnglobingFrame;
using EnglobingFramePtr = std::shared_ptr<EnglobingFrame>;

class EnglobingFrame : public ScreenPrimitive, public Frame
{
    using InsideType = ScreenPrimitiveInSlide;
    std::pair<vec2,vec2> ComputeBoundingBoxAndPosition(const std::vector<InsideType>& primitives) const;
    std::vector<InsideType> primitivesInside;
    scalar padding;

public:


    EnglobingFrame(Color c,float r, float padding, const std::vector<InsideType>& primitives) : primitivesInside(primitives) {
        this->color = c;
        roundness = r;
        depth = -100;
        this->padding = padding;
    }

    static EnglobingFramePtr Add(Color c,float r,float padding, const std::vector<InsideType>& primitives)
    {
        return NewPrimitive<EnglobingFrame>(c,r,padding,primitives);
    }

    template<typename... Args>
    static EnglobingFramePtr Add(Color c, float r,float padding, const Args&... primitives)
    {
        static_assert((std::is_same_v<Args, ScreenPrimitiveInSlide> && ...),
                      "All arguments must be ScreenPrimitiveInSlide");

        // use primitives...
        std::vector<ScreenPrimitiveInSlide> pack{primitives...};

        return Add(c,r,padding,pack);
    }

    vec2 getSize() const override {
        vec2 rslt = size;
        auto [size,pos] = ComputeBoundingBoxAndPosition(primitivesInside);
        auto W = ImGui::GetWindowSize();
        return vec2(rslt(0)*W.x, rslt(1)*W.y);
    }

protected:
    virtual void draw(const TimeObject &time, const StateInSlide &sis) override {
        auto [size,pos] = ComputeBoundingBoxAndPosition(primitivesInside);
        drawFrame(pos,size,color,roundness,sis.alpha);
    }
    virtual void playIntro(const TimeObject &t, const StateInSlide &sis) override {
        auto sist = transition.intro(t,sis);;
        auto [size,pos] = ComputeBoundingBoxAndPosition(primitivesInside);
        drawFrame(pos,size,color,roundness,sist.alpha);
    }
    virtual void playOutro(const TimeObject &t, const StateInSlide &sis) override {
        auto sist = transition.outro(t,sis);;
        auto [size,pos] = ComputeBoundingBoxAndPosition(primitivesInside);
        drawFrame(pos,size,color,roundness,sist.alpha);
    }
};


} // namespace slope

#endif // SLOPE_FRAME_H
