#ifndef STACK2D_H
#define STACK2D_H

#include "content/screen_primitives/ScreenPrimitive.h"

namespace slope {

class Stack2D;
using Stack2DPtr = std::shared_ptr<Stack2D>;

/*
 * Vertical layout. The children are placed one below the other with the same spacing,
 * and the whole block is centered on one handle anchor.
 * The layout is computed every frame, so it follows dragging of the handle
 * and size changes of the children, for example after a LaTeX reload.
 *
 * The layout uses all the children, visible or not, so a child that appears later
 * fades in at its final position and the others do not move.
 *
 * The stack draws nothing. It is a screen primitive so that arrows and boxes can target the whole block,
 * and so that the block can be dragged as one unit when the handle is a label.
 */
class Stack2D : public ScreenPrimitive
{
public:
    // Horizontal alignment of the children inside the block.
    enum class Align { LEFT, CENTER, RIGHT };
    Align align = Align::LEFT;
    // Vertical gap between children, in relative units.
    scalar spacing = 0.015;
    // Center of the block. A LabelAnchor makes it draggable.
    AnchorPtr handle;

    // Builds a stack centered on the handle.
    static Stack2DPtr Add(AnchorPtr handle = nullptr);
    // Builds a stack whose handle is a label.
    static Stack2DPtr Add(const std::string& label);

    // Removes every child.
    void clearChildren();
    // Adds a child at the bottom of the stack.
    void addChild(ScreenPrimitivePtr child);
    const std::vector<ScreenPrimitivePtr>& getChildren() const {return children;}

    // Returns the state that puts a child in its slot. The slot is computed every frame.
    ScreenPrimitiveInSlide place(ScreenPrimitivePtr child, scalar alpha = 1);

    // Relative position of the center of a child.
    vec2 childPosition(const ScreenPrimitive* child) const;

    // Size in pixels.
    vec2 getSize() const override;
    void getBoundingBox(vec2& lo, vec2& hi) const override;

    // The stack draws nothing.
    void draw(const TimeObject&, const StateInSlide&) override {}
    void playIntro(const TimeObject&, const StateInSlide&) override {}
    void playOutro(const TimeObject&, const StateInSlide&) override {}

protected:
    std::vector<ScreenPrimitivePtr> children;

    // Center of the block.
    vec2 center() const;
    // Size of the block in relative units, with the largest width and the sum of the heights.
    vec2 blockSize() const;
};

}

#endif // STACK2D_H
