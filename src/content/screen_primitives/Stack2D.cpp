#include "Stack2D.h"

namespace slope {

Stack2DPtr Stack2D::Add(AnchorPtr handle)
{
    auto s = NewPrimitive<Stack2D>();
    s->handle = handle;
    return s;
}

Stack2DPtr Stack2D::Add(const std::string& label)
{
    return Add(LabelAnchor::Add(label));
}

void Stack2D::clearChildren()
{
    children.clear();
}

void Stack2D::addChild(ScreenPrimitivePtr child)
{
    children.push_back(child);
}

ScreenPrimitiveInSlide Stack2D::place(ScreenPrimitivePtr child, scalar alpha)
{
    auto self = std::static_pointer_cast<Stack2D>(get(pid));
    StateInSlide sis;
    sis.anchor = DynamicAnchor::Add([self, c = child.get()]() {
        return self->childPosition(c);
    });
    sis.alpha = alpha;
    return {child, sis};
}

vec2 Stack2D::center() const
{
    return handle ? handle->getPos() : anchor->getPos();
}

vec2 Stack2D::blockSize() const
{
    vec2 size(0, 0);
    for (const auto& child : children) {
        vec2 s = child->getRelativeSize();
        size(0) = std::max(size(0), s(0));
        size(1) += s(1);
    }
    if (children.size() > 1)
        size(1) += spacing * (children.size() - 1);
    return size;
}

vec2 Stack2D::childPosition(const ScreenPrimitive* child) const
{
    vec2 c = center();
    vec2 block = blockSize();
    scalar y = c(1) - block(1) * 0.5;
    for (const auto& ch : children) {
        vec2 s = ch->getRelativeSize();
        if (ch.get() == child) {
            scalar x = c(0);
            if (align == Align::LEFT)
                x = c(0) - block(0) * 0.5 + s(0) * 0.5;
            else if (align == Align::RIGHT)
                x = c(0) + block(0) * 0.5 - s(0) * 0.5;
            return vec2(x, y + s(1) * 0.5);
        }
        y += s(1) + spacing;
    }
    return c; // not a child : degrade to the block center
}

vec2 Stack2D::getSize() const
{
    vec2 block = blockSize();
    return vec2(block(0) * Options::ScreenResolutionWidth,
                block(1) * Options::ScreenResolutionHeight);
}

void Stack2D::getBoundingBox(vec2& lo, vec2& hi) const
{
    vec2 c = center();
    vec2 h = blockSize() * 0.5;
    lo = c - h;
    hi = c + h;
}

}
