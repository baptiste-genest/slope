#include "primitive.h"

std::vector<slope::PrimitivePtr> slope::Primitive::primitives;

slope::TimeObject slope::TimeObject::operator()(Primitive* p) const {
    auto tmp = *this;
    tmp.inner_time = p->getInnerTime();
    tmp.relative_frame_number = p->relativeSlideIndex(tmp.absolute_frame_number);
    return tmp;
}

slope::TransitionAnimator slope::Primitive::DefaultTransition;

void slope::Primitive::addPrimitive(PrimitivePtr ptr) {
    ptr->pid = primitives.size();
    primitives.push_back(ptr);
}

slope::PrimitivePtr slope::Primitive::get(PrimitiveID id){
    return primitives[id];
}

slope::index slope::Primitive::relativeSlideIndex(index in) {
    return in - first_slide_to_appear;
}

void slope::Primitive::handleInnerTime() {
    inner_time = Time::now();
}

void slope::Primitive::play(const TimeObject &t, const StateInSlide &sis) {
    enable();
    auto it = t(this);
    draw(it,sis);
    updater(it,this);
}

void slope::Primitive::intro(const TimeObject &t, const StateInSlide &sis) {
    enable();
    auto it = t(this);
    playIntro(it,transition.intro(it,sis));
    updater(it,this);
}

void slope::Primitive::outro(const TimeObject &t, const StateInSlide &sis) {
    auto it = t(this);
    playOutro(it,transition.outro(it,sis));
    updater(it,this);
}

bool slope::Primitive::isEnabled() const {return enabled;}

void slope::Primitive::disable() {
    if (!enabled)
        return;
    enabled = false;
    forceDisable();
}

void slope::Primitive::enable() {
    if (enabled)
        return;
    enabled = true;
    forceEnable();
    handleInnerTime();
}

slope::TimeTypeSec slope::Primitive::getInnerTime(){
    return TimeFrom(inner_time);
}

bool slope::Primitive::isExclusive() const {
    return exclusive;
}

void slope::Primitive::initPolyscope() {}

void slope::Primitive::setDepth(int d) {depth = d;}

int slope::Primitive::getDepth() const {return depth;}

void slope::Primitive::upFirstSlideNumber(int f) {
    first_slide_to_appear = std::min(first_slide_to_appear,f);
}

void slope::Primitive::forceDisable() {}

void slope::Primitive::forceEnable() {}
