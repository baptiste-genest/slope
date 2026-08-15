#ifndef STACK2D_H
#define STACK2D_H

#include "ScreenPrimitive.h"

namespace slope {

class Stack2D;
using Stack2DPtr = std::shared_ptr<Stack2D>;

/*
 * Vertical layout. Children are placed below one another with uniform
 * spacing, the whole block centered on a single handle anchor. The layout
 * is recomputed every frame, so it follows drag edits of the handle and
 * size changes of the children (hot-reloaded latex...).
 *
 * The layout is computed from all registered children, visible or not, so a
 * child appearing later fades in at its final position and the others hold.
 *
 * The stack draws nothing itself; it is a screen primitive so that arrows
 * and englobing boxes can target the whole block, and so that (with a
 * label handle) the block is drag-editable as one unit.
 */
class Stack2D : public ScreenPrimitive
{
public:
    enum class Align { LEFT, CENTER, RIGHT };
    Align align = Align::LEFT;
    scalar spacing = 0.015;    // vertical gap between children, relative units
    AnchorPtr handle;          // block center; a LabelAnchor makes it draggable

    static Stack2DPtr Add(AnchorPtr handle = nullptr);
    static Stack2DPtr Add(const std::string& label);

    void clearChildren();
    void addChild(ScreenPrimitivePtr child);
    const std::vector<ScreenPrimitivePtr>& getChildren() const {return children;}

    // slide state placing a child at its slot, recomputed every frame
    ScreenPrimitiveInSlide place(ScreenPrimitivePtr child, scalar alpha = 1);

    vec2 childPosition(const ScreenPrimitive* child) const;

    vec2 getSize() const override;
    void getBoundingBox(vec2& lo, vec2& hi) const override;

    // invisible, the stack only computes layout
    void draw(const TimeObject&, const StateInSlide&) override {}
    void playIntro(const TimeObject&, const StateInSlide&) override {}
    void playOutro(const TimeObject&, const StateInSlide&) override {}

protected:
    std::vector<ScreenPrimitivePtr> children;

    vec2 center() const;
    // total block size in relative units (max width, summed heights)
    vec2 blockSize() const;
};

}

#endif // STACK2D_H
