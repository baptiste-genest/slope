#ifndef SCREENPRIMITIVE_H
#define SCREENPRIMITIVE_H

#include "../primitive.h"
#include "../StateInSlide.h"
#include "Anchor.h"

namespace slope {
class ScreenPrimitive;
using ScreenPrimitivePtr = std::shared_ptr<ScreenPrimitive>;
using ScreenPrimitiveInSlide = std::pair<ScreenPrimitivePtr,StateInSlide>;

class ScreenPrimitive : public Primitive
{
protected:
    AnchorPtr anchor;

    // scale the primitive is actually drawn at (slide state or persistent
    // anchor scale), mirrored on every play/intro/outro like the anchor,
    // so bounding boxes follow dynamic rescaling
    scalar drawn_scale = 1;
public:
    ScreenPrimitive();

    static ScreenPrimitivePtr get(PrimitiveID id);

    bool isScreenSpace() const override;

    // the primitive's own anchor mirrors where it is actually drawn (the
    // slide state's anchor), so followers (arrows, englobing boxes) resolve
    // live positions; synced on every play/intro/outro
    void play(const TimeObject& t, const StateInSlide& sis) override;
    void intro(const TimeObject& t, const StateInSlide& sis) override;
    void outro(const TimeObject& t, const StateInSlide& sis) override;

    AnchorPtr getAnchor() const;

    void updateAnchor(const vec2& p);


    ScreenPrimitiveInSlide at(const vec2& p,scalar alpha=1);

    // pastes onto a world plane. The named form lives in views/<id>.transform
    ScreenPrimitiveInSlide onPlane(const std::string& id,scalar alpha = 1);
    ScreenPrimitiveInSlide onPlane(const Transform& plane,scalar alpha = 1);
    ScreenPrimitiveInSlide onPlane(const vec& origin,const vec& u,const vec& normal,scalar alpha = 1);
    // the snippet driven form, re-read every frame
    ScreenPrimitiveInSlide onPlane(const LivePlane& plane,scalar alpha = 1);

    ScreenPrimitiveInSlide at(StateInSlide sis);


    ScreenPrimitiveInSlide at(scalar x,scalar y,scalar alpha=1);


    ScreenPrimitiveInSlide at(std::string label,scalar alpha = 1);

    ScreenPrimitiveInSlide at(const std::function<vec2()>& placer);
    ScreenPrimitiveInSlide track(const std::function<vec()>& toTrack,vec2 offset = vec2::Zero());
    ScreenPrimitiveInSlide at(const vec& worldPos,const vec2& offset = vec2::Zero());


    virtual vec2 getSize() const = 0;

    // only texture-backed primitives go through ImageRotated, the others would
    // silently ignore an angle, so the editor refuses to rotate them
    virtual bool canRotate() const {return false;}

    Size getRelativeSize() const;

    // pixels a primitive is drawn away from its anchor, applied to the centre
    // before rotation. A formula sits on its baseline, not on the centre of
    // its ink, so the editor cannot outline it from the anchor alone
    virtual vec2 getDrawOffset() const {return vec2::Zero();}

    // bounding box in relative [0,1]² coords, centered on the anchor unless a
    // primitive's geometry does not follow it (arrows, boxes)
    virtual void getBoundingBox(vec2& lo, vec2& hi) const;

};

struct TextualPrimitive : public ScreenPrimitive {
    std::string content;
};

using TextualPrimitivePtr = std::shared_ptr<TextualPrimitive>;


}

#endif // SCREENPRIMITIVE_H
