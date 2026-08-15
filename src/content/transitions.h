#ifndef TRANSITIONS_H
#define TRANSITIONS_H

#include "StateInSlide.h"
#include "TimeObject.h"
#include "../math/easing.h"

namespace slope {

using TransitionAction = std::function<StateInSlide(const TimeObject&,const StateInSlide&)>;

struct TransitionAnimator {
    TransitionAction intro,outro;

    RateFunc rate_in  = rate::smooth_squared;
    RateFunc rate_out = rate::smooth_squared;

    scalar shapeIn(scalar p) const {
        return (rate_in ? rate_in : rate::smooth_squared)(std::clamp(p,0.,1.));
    }
    scalar shapeOut(scalar p) const {
        return (rate_out ? rate_out : rate::smooth_squared)(std::clamp(p,0.,1.));
    }

    TransitionAnimator();
};

TransitionAnimator FadeInFadeOut();
TransitionAnimator SlideInSlideOut();

// leaves the StateInSlide alone, no fade and no offset, for a primitive whose
// entrance is animated by something else
TransitionAnimator NoTransition();

}

#endif // TRANSITIONS_H
