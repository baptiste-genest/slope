#include "primitive.h"

int slope::Primitive::updater_framerate = 1;

std::vector<slope::PrimitivePtr> slope::Primitive::primitives;

slope::TimeObject slope::TimeObject::operator()(Primitive* p) const {
    auto tmp = *this;
    tmp.inner_time = p->getInnerTime();
    tmp.relative_frame_number = p->relativeSlideIndex(tmp.absolute_frame_number);
    return tmp;
}

slope::TransitionAnimator slope::Primitive::DefaultTransition;
