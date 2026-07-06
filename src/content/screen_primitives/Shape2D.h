#ifndef SHAPE2D_H
#define SHAPE2D_H

#include "ScreenPrimitive.h"

namespace slope {

/*
 * Live 2D vector graphics, drawn every frame through ImGui's draw list :
 * resolution independent, animatable (updaters can move control points),
 * with a draw-on intro (the stroke grows along its arc length).
 *
 * Static shapes (Line, Polyline, Bezier, Circle, Rect) store their geometry
 * as offsets around their anchor, so they are placed, dragged and
 * transitioned like any other screen primitive.
 *
 * Arrow2D is a connector : its endpoints are resolved every frame, either
 * fixed positions, persistent labels, or other screen primitives (attached
 * at their bounding box boundary), so arrows follow drag edits and moving
 * targets.
 */

struct ShapeStyle {
    RGBA color = RGBA(0.f, 0.f, 0.f, 1.f);
    RGBA fill_color = RGBA(0.f, 0.f, 0.f, 0.25f);
    float thickness = 3;   // pixels at 1080p, scaled with the window
    bool filled = false;
};

class Shape2D;
using Shape2DPtr = std::shared_ptr<Shape2D>;

class Shape2D : public ScreenPrimitive
{
public:
    ShapeStyle style;

    // geometry given in absolute [0,1]² coords; recentered internally so the
    // shape is positioned by its anchor like any screen primitive
    static Shape2DPtr Add(const std::vector<vec2>& pts, bool closed = false);

    static Shape2DPtr Line(const vec2& a, const vec2& b);
    static Shape2DPtr Bezier(const vec2& a, const vec2& control, const vec2& b, int N = 48);
    static Shape2DPtr Circle(const vec2& center, scalar radius, int N = 64);
    static Shape2DPtr Rect(const vec2& center, const vec2& size);

    vec2 getSize() const override;

    void draw(const TimeObject& t, const StateInSlide& sis) override;
    void playIntro(const TimeObject& t, const StateInSlide& sis) override;
    void playOutro(const TimeObject& t, const StateInSlide& sis) override;

    std::vector<vec2> points; // offsets around the anchor, relative units
    bool closed = false;

protected:
    // draws the arc-length prefix [0,t] of the (pixel space) polyline
    void strokePrefix(const std::vector<ImVec2>& px, parameter t, float alpha) const;

    std::vector<ImVec2> toPixels(const StateInSlide& sis) const;
};

class Box2D;
using Box2DPtr = std::shared_ptr<Box2D>;

// a rectangle englobing its targets : the union of their bounding boxes
// (plus padding) is recomputed every frame, so the box follows drag edits
// and moving content
class Box2D : public ScreenPrimitive
{
public:
    ShapeStyle style;
    // gap kept around the targets, relative units, per axis
    vec2 padding = vec2(0.02, 0.02);
    void setPadding(scalar p) { padding = vec2(p, p); }
    std::vector<ScreenPrimitivePtr> targets;

    // until a fill color is chosen, a filled box is painted in the current
    // background color (opaque), so it masks what it covers
    bool use_background_fill = true;
    void setFillColor(const RGBA& c) {
        style.fill_color = c;
        use_background_fill = false;
    }

    static Box2DPtr Add(const std::vector<ScreenPrimitivePtr>& targets = {});

    template<typename First, typename... Rest,
             typename = std::enable_if_t<std::is_convertible_v<First, ScreenPrimitivePtr>>>
    static Box2DPtr Add(const First& first, const Rest&... rest) {
        return Add(std::vector<ScreenPrimitivePtr>{first, rest...});
    }

    // replaces the englobed primitives; like any primitive, the box is
    // drawn at its insertion rank in the slide : add it before its targets
    // to frame them, after to cover them
    void setTargets(const std::vector<ScreenPrimitivePtr>& t);

    vec2 getSize() const override;
    void getBoundingBox(vec2& lo, vec2& hi) const override;

    void draw(const TimeObject& t, const StateInSlide& sis) override;
    void playIntro(const TimeObject& t, const StateInSlide& sis) override;
    void playOutro(const TimeObject& t, const StateInSlide& sis) override;

protected:
    // union of the targets' bounding boxes plus padding, relative coords;
    // false when there is no target to englobe
    bool bounds(vec2& lo, vec2& hi) const;
    void drawBox(parameter t, float alpha);
};

class Arrow2D;
using Arrow2DPtr = std::shared_ptr<Arrow2D>;

class Arrow2D : public ScreenPrimitive
{
public:
    ShapeStyle style;
    scalar bend = 0;        // curvature : offset of the control point,
                            // as a fraction of the endpoint distance
    scalar head = 0.015;    // arrowhead size, relative units
    scalar margin = 0.01;   // gap kept between an endpoint and its target

    struct Endpoint {
        vec2 fixed = vec2(0.5, 0.5);
        ScreenPrimitivePtr prim = nullptr; // attach to its bbox when set
        AnchorPtr anchor = nullptr;        // else follow this anchor if set
        vec2 offset = vec2(0, 0);          // shift applied after attachment

        vec2 center() const;
    };
    Endpoint from, to;

    static Arrow2DPtr Add(const vec2& a, const vec2& b);
    static Arrow2DPtr Add(const Endpoint& a, const Endpoint& b);
    static Endpoint Attach(ScreenPrimitivePtr p);
    static Endpoint AttachLabel(const std::string& label);

    vec2 getSize() const override;
    void getBoundingBox(vec2& lo, vec2& hi) const override;

    void draw(const TimeObject& t, const StateInSlide& sis) override;
    void playIntro(const TimeObject& t, const StateInSlide& sis) override;
    void playOutro(const TimeObject& t, const StateInSlide& sis) override;

protected:
    void drawArrow(parameter t, float alpha) const;

    // clips the segment [c, other] against the endpoint's bounding box
    // (plus margin), so connectors attach at the boundary of their target
    static vec2 attachPoint(const Endpoint& e, const vec2& other, scalar margin);
};

}

#endif // SHAPE2D_H
