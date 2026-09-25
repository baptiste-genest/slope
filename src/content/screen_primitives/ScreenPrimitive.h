#ifndef SCREENPRIMITIVE_H
#define SCREENPRIMITIVE_H

#include "content/core/primitive.h"
#include "content/core/StateInSlide.h"
#include "content/screen_primitives/layout/Anchor.h"

namespace slope {
class ScreenPrimitive;
using ScreenPrimitivePtr = std::shared_ptr<ScreenPrimitive>;
using ScreenPrimitiveInSlide = std::pair<ScreenPrimitivePtr,StateInSlide>;

// A 2D primitive drawn over the scene. Positions are relative to the window, from (0,0) at the top left to (1,1) at the bottom right.
class ScreenPrimitive : public Primitive
{
protected:
    // Where the primitive is drawn now.
    AnchorPtr anchor;

    // Scale used to draw the primitive, from the slide state or from a persistent anchor.
    // It is updated at every frame like the anchor, so bounding boxes follow rescaling.
    scalar drawn_scale = 1;

    // Angle used to draw the primitive, updated like drawn_scale.
    // It lets the axis-aligned bounding box wrap a rotated primitive.
    scalar drawn_angle = 0;
public:
    ScreenPrimitive();

    // Returns the screen primitive with this pid.
    static ScreenPrimitivePtr get(PrimitiveID id);

    bool isScreenSpace() const override;

    // The anchor of the primitive follows the place where it is drawn, given by the anchor of the slide state.
    // Arrows and boxes that follow the primitive then use its current position.
    // It is updated at every play, intro and outro.
    void play(const TimeObject& t, const StateInSlide& sis) override;
    void intro(const TimeObject& t, const StateInSlide& sis) override;
    void outro(const TimeObject& t, const StateInSlide& sis) override;

    // The anchor that follows the primitive.
    AnchorPtr getAnchor() const;

    // Moves the anchor to p.
    void updateAnchor(const vec2& p);
    // Copies position, scale and angle from a state, so a primitive not drawn yet has the right size.
    void syncToState(const StateInSlide& sis);


    // Places the primitive at the relative position p.
    ScreenPrimitiveInSlide at(const vec2& p,scalar alpha=1);

    // Pastes the primitive onto a plane of the scene. The named form reads the plane from views/<id>.transform.
    ScreenPrimitiveInSlide onPlane(const std::string& id,scalar alpha = 1);
    ScreenPrimitiveInSlide onPlane(const Transform& plane,scalar alpha = 1);
    ScreenPrimitiveInSlide onPlane(const vec& origin,const vec& u,const vec& normal,scalar alpha = 1);
    // Same, with a plane given by snippets and read every frame.
    ScreenPrimitiveInSlide onPlane(const LivePlane& plane,scalar alpha = 1);

    // Places the primitive with a full state.
    ScreenPrimitiveInSlide at(StateInSlide sis);

    // Places the primitive at the relative position (x,y).
    ScreenPrimitiveInSlide at(scalar x,scalar y,scalar alpha=1);

    // Places the primitive at a label, whose position is stored in views/<label>.pos and can be dragged.
    ScreenPrimitiveInSlide at(std::string label,scalar alpha = 1);

    // Places the primitive at the relative position returned by the function, called every frame.
    ScreenPrimitiveInSlide at(const std::function<vec2()>& placer);
    // Places the primitive on the screen projection of the 3D point returned every frame, shifted by offset.
    ScreenPrimitiveInSlide track(const std::function<vec()>& toTrack,vec2 offset = vec2::Zero());
    // Places the primitive on the screen projection of a fixed 3D point, shifted by offset.
    ScreenPrimitiveInSlide at(const vec& worldPos,const vec2& offset = vec2::Zero());


    // Size in pixels.
    virtual vec2 getSize() const = 0;

    // True for a primitive that decides its own position, such as a Plot placed on the rectangle of its Board.
    // It gets no default label, so no views/*.pos file is created for a placement it ignores.
    virtual bool placesItself() const {return false;}

    // True when an angle has an effect. Only primitives drawn from a texture rotate,
    // and the editor refuses to rotate the others.
    virtual bool canRotate() const {return false;}

    // Size relative to the window. It grows to wrap the primitive when rotated.
    Size getRelativeSize() const;

    // Distance in pixels between the anchor and the center of the drawing, applied before the rotation.
    // A formula is placed by its baseline and not by the center of its ink,
    // so the editor needs this offset to outline it.
    virtual vec2 getDrawOffset() const {return vec2::Zero();}

    // Bounding box in relative coordinates. It is centered on the anchor,
    // except for primitives whose geometry does not follow it, such as arrows and boxes.
    virtual void getBoundingBox(vec2& lo, vec2& hi) const;

};

// A screen primitive that holds text.
struct TextualPrimitive : public ScreenPrimitive {
    std::string content;
};

using TextualPrimitivePtr = std::shared_ptr<TextualPrimitive>;


}

#endif // SCREENPRIMITIVE_H
