#ifndef TRANSITIONS_H
#define TRANSITIONS_H

#include "content/core/StateInSlide.h"
#include "content/core/TimeObject.h"
#include "math/easing.h"

namespace slope {

// Returns the state to draw for a given time.
using TransitionAction = std::function<StateInSlide(const TimeObject&,const StateInSlide&)>;

// How a primitive appears and disappears.
struct TransitionAnimator {
    // Change the state while the primitive appears and disappears.
    TransitionAction intro,outro;

    // Easing applied to the transition parameter.

    RateFunc rate_in  = rate::smooth_squared;
    RateFunc rate_out = rate::smooth_squared;

    // Applies the intro easing to p, clamped to [0,1].
    scalar shapeIn(scalar p) const {
        return (rate_in ? rate_in : rate::smooth_squared)(std::clamp(p,0.,1.));
    }
    // Applies the outro easing to p, clamped to [0,1].
    scalar shapeOut(scalar p) const {
        return (rate_out ? rate_out : rate::smooth_squared)(std::clamp(p,0.,1.));
    }

    // Default transition, a fade.
    TransitionAnimator();
};

// Fades the primitive in and out.
TransitionAnimator FadeInFadeOut();
// Moves the primitive in from the top and out to the bottom.
TransitionAnimator SlideInSlideOut();

// Leaves the state unchanged, for a primitive that animates its own appearance.
TransitionAnimator NoTransition();

}

#endif // TRANSITIONS_H
