#ifndef SCREENPRIMITIVE_H
#define SCREENPRIMITIVE_H

#include "primitive.h"
#include "StateInSlide.h"
#include "Anchor.h"

namespace slope {
class ScreenPrimitive;
using ScreenPrimitivePtr = std::shared_ptr<ScreenPrimitive>;
using ScreenPrimitiveInSlide = std::pair<ScreenPrimitivePtr,StateInSlide>;

class ScreenPrimitive : public Primitive
{
protected:
    AnchorPtr anchor;
public:
    ScreenPrimitive();

    static ScreenPrimitivePtr get(PrimitiveID id);

    bool isScreenSpace() const override;

    AnchorPtr getAnchor() const;

    void updateAnchor(const vec2& p);


    ScreenPrimitiveInSlide at(const vec2& p,scalar alpha=1);

    ScreenPrimitiveInSlide at(StateInSlide sis);


    ScreenPrimitiveInSlide at(scalar x,scalar y,scalar alpha=1);


    ScreenPrimitiveInSlide at(std::string label,scalar alpha = 1);

    ScreenPrimitiveInSlide at(const std::function<vec2()>& placer);
    ScreenPrimitiveInSlide track(const std::function<vec()>& toTrack,vec2 offset = vec2::Zero());
    ScreenPrimitiveInSlide at(const vec& worldPos,const vec2& offset = vec2::Zero());


    virtual vec2 getSize() const = 0;

    Size getRelativeSize() const;

};

struct TextualPrimitive : public ScreenPrimitive {
    std::string content;
};

using TextualPrimitivePtr = std::shared_ptr<TextualPrimitive>;


}

#endif // SCREENPRIMITIVE_H
