#include "transitions.h"
#include "../math/kernels.h"

slope::TransitionAnimator slope::SlideInSlideOut()
{
    TransitionAnimator T;
    T.intro = [] (const TimeObject& t,const StateInSlide& sis){
        auto rslt = sis;
        if (!rslt.offseted)
            rslt.addOffset(vec2(0,-1)*smoothstep(1-t.transition_parameter));
        return rslt;
    };
    T.outro = [] (const TimeObject& t,const StateInSlide& sis){
        auto rslt = sis;
        if (!rslt.offseted)
            rslt.addOffset(vec2(0,1)*smoothstep(t.transition_parameter));
        return rslt;
    };
    return T;
}

slope::TransitionAnimator slope::FadeInFadeOut()
{
    TransitionAnimator T;
    T.intro = [] (const TimeObject& t,const StateInSlide& sis){
        auto rslt = sis;
        rslt.alpha *= smoothstep(t.transition_parameter);
        return rslt;
    };
    T.outro = [] (const TimeObject& t,const StateInSlide& sis){
        auto rslt = sis;
        rslt.alpha *= smoothstep(1-t.transition_parameter);
        return rslt;
    };
    return T;
}


slope::TransitionAnimator::TransitionAnimator()
{
    intro = [] (const TimeObject& t,const StateInSlide& sis){
        auto rslt = sis;
        rslt.alpha *= smoothstep(t.transition_parameter);
        return rslt;
    };
    outro = [] (const TimeObject& t,const StateInSlide& sis){
        auto rslt = sis;
        rslt.alpha *= smoothstep(1-t.transition_parameter);
        return rslt;
    };
}
