#include "ScreenPrimitive.h"
#include "PlaneWarp.h"

slope::ScreenPrimitive::ScreenPrimitive() {
    anchor = AbsoluteAnchor::Add(vec2(0,0));

}

slope::ScreenPrimitivePtr slope::ScreenPrimitive::get(PrimitiveID id) {
    return std::dynamic_pointer_cast<ScreenPrimitive>(Primitive::get(id));
}

bool slope::ScreenPrimitive::isScreenSpace() const {
    return true;
}

void slope::ScreenPrimitive::play(const TimeObject &t, const StateInSlide &sis) {
    anchor->updatePos(sis.getPosition());
    drawn_scale = sis.getScale();
    Primitive::play(t,sis);
}

void slope::ScreenPrimitive::intro(const TimeObject &t, const StateInSlide &sis) {
    anchor->updatePos(sis.getPosition());
    drawn_scale = sis.getScale();
    Primitive::intro(t,sis);
}

void slope::ScreenPrimitive::outro(const TimeObject &t, const StateInSlide &sis) {
    anchor->updatePos(sis.getPosition());
    drawn_scale = sis.getScale();
    Primitive::outro(t,sis);
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

static slope::StateInSlide planeState(slope::scalar alpha) {
    slope::StateInSlide sis;
    // an unplaced plane draws as a billboard here, so it starts mid screen
    sis.anchor = slope::AbsoluteAnchor::Add(slope::vec2(0.5,0.5));
    sis.alpha = alpha;
    return sis;
}

slope::ScreenPrimitiveInSlide slope::ScreenPrimitive::onPlane(const std::string &id, scalar alpha) {
    StateInSlide sis = planeState(alpha);
    sis.persistentTransform = PersistentTransform(id);
    return {get(pid),sis};
}

slope::ScreenPrimitiveInSlide slope::ScreenPrimitive::onPlane(const Transform &plane, scalar alpha) {
    StateInSlide sis = planeState(alpha);
    sis.plane.frame = plane;
    return {get(pid),sis};
}

slope::ScreenPrimitiveInSlide slope::ScreenPrimitive::onPlane(const vec &origin, const vec &u, const vec &normal, scalar alpha) {
    return onPlane(TransformFromWidth(origin,u,normal),alpha);
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

void slope::ScreenPrimitive::getBoundingBox(vec2 &lo, vec2 &hi) const {
    vec2 c = anchor->getPos();
    vec2 h = getRelativeSize()*0.5*drawn_scale;
    lo = c - h;
    hi = c + h;
}
