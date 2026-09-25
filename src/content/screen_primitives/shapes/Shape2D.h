#ifndef SHAPE2D_H
#define SHAPE2D_H

#include "content/screen_primitives/ScreenPrimitive.h"
#include "content/authoring/color_tools.h"
#include <optional>

namespace slope {

/*
 * 2D vector graphics drawn every frame with the draw list of ImGui.
 * They do not depend on the resolution and can be animated, since updaters can move the control points.
 * The intro draws the stroke progressively along its length.
 *
 * The static shapes (Line, Polyline, Bezier, Circle, Rect) store their geometry as offsets around their anchor.
 * They are placed, dragged and animated like any other screen primitive.
 *
 * Arrow2D joins two endpoints that are computed every frame.
 * An endpoint is a fixed position, a label, or another screen primitive, in which case the arrow stops at the edge of its bounding box.
 * So arrows follow dragged and moving targets.
 */

// Colors and stroke of a shape.
struct ShapeStyle {
    // Colors are fixed by default. Giving them a name, or a color id in a deck, makes them tunable in the Tuner.
    Color color = Color(0.f, 0.f, 0.f, 1.f);
    Color fill_color = Color(0.f, 0.f, 0.f, 0.25f);
    // Line width in pixels for a 1080p window, scaled with the window.
    float thickness = 3;
    bool filled = false;
};

class Shape2D;
using Shape2DPtr = std::shared_ptr<Shape2D>;

// A polyline, curve or polygon.
class Shape2D : public ScreenPrimitive
{
public:
    ShapeStyle style;

    // Builds a shape from points in relative coordinates.
    // The points are recentered, so the shape is placed by its anchor like any screen primitive.
    static Shape2DPtr Add(const std::vector<vec2>& pts, bool closed = false);

    // Segment from a to b.
    static Shape2DPtr Line(const vec2& a, const vec2& b);
    // Quadratic curve from a to b, drawn with N segments.
    static Shape2DPtr Bezier(const vec2& a, const vec2& control, const vec2& b, int N = 48);
    // Circle drawn with N segments. The radius is relative to the width of the window.
    static Shape2DPtr Circle(const vec2& center, scalar radius, int N = 64);
    // Rectangle of the given size.
    static Shape2DPtr Rect(const vec2& center, const vec2& size);

    // Size in pixels.
    vec2 getSize() const override;

    void draw(const TimeObject& t, const StateInSlide& sis) override;
    void playIntro(const TimeObject& t, const StateInSlide& sis) override;
    void playOutro(const TimeObject& t, const StateInSlide& sis) override;

    // Offsets from the anchor, in relative units.
    std::vector<vec2> points;
    // When true, the last point is joined to the first.
    bool closed = false;

protected:
    // Points in window pixels, with the placement of the state applied.
    std::vector<ImVec2> toPixels(const StateInSlide& sis) const;
};

class Box2D;
using Box2DPtr = std::shared_ptr<Box2D>;

// A rectangle around its targets, made of the union of their bounding boxes plus a padding.
// It is computed every frame, so it follows dragged targets.
class Box2D : public ScreenPrimitive
{
public:
    ShapeStyle style;
    // Gap around the targets in relative units, one value per axis.
    // It is used on every side that has no value below.
    vec2 padding = vec2(0.02, 0.02);
    // Sets the same padding on both axes.
    void setPadding(scalar p) { padding = vec2(p, p); }

    // Padding of a single side. When unset, padding.x is used for left and right and padding.y for top and bottom.
    std::optional<scalar> pad_left, pad_right, pad_top, pad_bot;

    // Primitives that the box surrounds.
    std::vector<ScreenPrimitivePtr> targets;

    // Until a fill color is chosen, a filled box uses the opaque background color, so it hides what it covers.
    bool use_background_fill = true;
    // Sets the fill color and stops using the background color.
    void setFillColor(const Color& c) {
        style.fill_color = c;
        use_background_fill = false;
    }

    // Builds a box around the targets.
    static Box2DPtr Add(const std::vector<ScreenPrimitivePtr>& targets = {});

    // Same, with the targets given as separate arguments.
    template<typename First, typename... Rest,
             typename = std::enable_if_t<std::is_convertible_v<First, ScreenPrimitivePtr>>>
    static Box2DPtr Add(const First& first, const Rest&... rest) {
        return Add(std::vector<ScreenPrimitivePtr>{first, rest...});
    }

    // Replaces the targets. Like any primitive the box is drawn in the order it was added,
    // so add it before its targets to draw it behind them.
    void setTargets(const std::vector<ScreenPrimitivePtr>& t);

    // Size in pixels.
    vec2 getSize() const override;
    void getBoundingBox(vec2& lo, vec2& hi) const override;

    void draw(const TimeObject& t, const StateInSlide& sis) override;
    void playIntro(const TimeObject& t, const StateInSlide& sis) override;
    void playOutro(const TimeObject& t, const StateInSlide& sis) override;

protected:
    // Computes the union of the bounding boxes of the targets plus padding, in relative coordinates.
    // Returns false when there is no target.
    bool bounds(vec2& lo, vec2& hi) const;
    // Draws the box at progress t, with opacity alpha.
    void drawBox(parameter t, float alpha);
};

class Arrow2D;
using Arrow2DPtr = std::shared_ptr<Arrow2D>;

// An arrow or a line between two endpoints.
class Arrow2D : public ScreenPrimitive
{
public:
    ShapeStyle style;
    // Curvature, given as the offset of the control point relative to the distance between the endpoints.
    scalar bend = 0;
    // Size of the arrowhead in relative units. 0 draws a plain line.
    scalar head = 0.015;
    // Gap between an endpoint and its target.
    scalar margin = 0.01;

    // One end of the arrow. It uses the first of prim, anchor and follow that is set, and fixed otherwise.
    struct Endpoint {
        // Fixed relative position.
        vec2 fixed = vec2(0.5, 0.5);
        // Primitive to attach to, at the edge of its bounding box.
        ScreenPrimitivePtr prim = nullptr;
        // Anchor to follow.
        AnchorPtr anchor = nullptr;
        // Function giving the position, for example from a live parameter.
        std::function<vec2()> follow = nullptr;
        // Shift applied after the attachment.
        vec2 offset = vec2(0, 0);

        // Current position of the endpoint, before clipping to the edge of a target.
        vec2 center() const;
    };
    Endpoint from, to;

    // Builds an arrow between two fixed relative positions.
    static Arrow2DPtr Add(const vec2& a, const vec2& b);
    // Builds an arrow between two endpoints.
    static Arrow2DPtr Add(const Endpoint& a, const Endpoint& b);
    // Endpoint attached to a primitive.
    static Endpoint Attach(ScreenPrimitivePtr p);
    // Endpoint that follows a label.
    static Endpoint AttachLabel(const std::string& label);

    // Size in pixels.
    vec2 getSize() const override;
    void getBoundingBox(vec2& lo, vec2& hi) const override;

    void draw(const TimeObject& t, const StateInSlide& sis) override;
    void playIntro(const TimeObject& t, const StateInSlide& sis) override;
    void playOutro(const TimeObject& t, const StateInSlide& sis) override;

protected:
    // Draws the arrow at progress t, with opacity alpha.
    void drawArrow(parameter t, float alpha) const;

    // Clips the segment from the center of the endpoint to `other` against the bounding box of its target plus margin.
    // The arrow then starts at the edge of the target.
    static vec2 attachPoint(const Endpoint& e, const vec2& other, scalar margin);

    // Control point of the curve. The bend is corrected for the aspect ratio of the screen, so the bulge looks the same in any direction.
    vec2 controlPoint(const vec2& a, const vec2& b) const;
};

}

#endif // SHAPE2D_H
