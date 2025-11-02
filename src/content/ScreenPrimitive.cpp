#include "ScreenPrimitive.h"

slope::ScreenPrimitive::ScreenPrimitive() {
    anchor = AbsoluteAnchor::Add(vec2(0,0));
}

slope::ScreenPrimitivePtr slope::ScreenPrimitive::get(PrimitiveID id) {
    return std::dynamic_pointer_cast<ScreenPrimitive>(Primitive::get(id));
}

bool slope::ScreenPrimitive::isScreenSpace() const {
    return true;
}

slope::AnchorPtr slope::ScreenPrimitive::getAnchor() const {return anchor;}

void slope::ScreenPrimitive::updateAnchor(const vec2 &p){
    anchor->updatePos(p);
}

slope::ScreenPrimitiveInSlide slope::ScreenPrimitive::at(const vec2 &p, scalar alpha) {
    StateInSlide sis(p);
    anchor->updatePos(p);
    sis.alpha = alpha;
    return {get(pid),sis};
}

slope::ScreenPrimitiveInSlide slope::ScreenPrimitive::at(StateInSlide sis) {
    return {get(pid),sis};
}

slope::ScreenPrimitiveInSlide slope::ScreenPrimitive::at(scalar x, scalar y, scalar alpha) {
    return at(vec2(x,y),alpha);
}

slope::ScreenPrimitiveInSlide slope::ScreenPrimitive::at(std::string label, scalar alpha) {
    StateInSlide sis;
    sis.anchor = LabelAnchor::Add(label);
    sis.alpha = alpha;
    return {get(pid),sis};
}

slope::ScreenPrimitiveInSlide slope::ScreenPrimitive::at(const std::function<vec2 ()> &placer) {
    StateInSlide sis;
    sis.anchor = DynamicAnchor::Add(placer);
    return {get(pid),sis};
}

slope::ScreenPrimitiveInSlide slope::ScreenPrimitive::track(const std::function<vec ()> &toTrack, vec2 offset) {
    StateInSlide sis;
    sis.anchor = DynamicAnchor::AddTracker(toTrack);
    offset(1) *= -1;
    sis.placer = [offset](vec2 p) { return vec2(p+offset); };
    return {get(pid),sis};
}

slope::ScreenPrimitiveInSlide slope::ScreenPrimitive::at(const vec &worldPos, const vec2 &offset) {
    StateInSlide sis;
    sis.anchor = DynamicAnchor::Add(worldPos);
    sis.placer = [offset](vec2 p) { return vec2(p+offset); };
    return {get(pid),sis};
}

slope::Primitive::Size slope::ScreenPrimitive::getRelativeSize() const {
    auto s = getSize();
    return Size(s(0)/Options::ScreenResolutionWidth,s(1)/Options::ScreenResolutionHeight);
}
