#include "content/core/transitions.h"

slope::TransitionAnimator slope::SlideInSlideOut()
{
    TransitionAnimator T;
    T.intro = [] (const TimeObject& t,const StateInSlide& sis){
        auto rslt = sis;
        if (!rslt.offseted)
            rslt.addOffset(vec2(0,-1)*(1-t.transition_parameter));
        return rslt;
    };
    T.outro = [] (const TimeObject& t,const StateInSlide& sis){
        auto rslt = sis;
        if (!rslt.offseted)
            rslt.addOffset(vec2(0,1)*t.transition_parameter);
        return rslt;
    };
    return T;
}

slope::TransitionAnimator slope::NoTransition()
{
    TransitionAnimator T;
    T.intro = [] (const TimeObject&,const StateInSlide& sis){ return sis; };
    T.outro = [] (const TimeObject&,const StateInSlide& sis){ return sis; };
    return T;
}

slope::TransitionAnimator slope::FadeInFadeOut()
{
    TransitionAnimator T;
    T.intro = [] (const TimeObject& t,const StateInSlide& sis){
        auto rslt = sis;
        rslt.alpha *= t.transition_parameter;
        return rslt;
    };
    T.outro = [] (const TimeObject& t,const StateInSlide& sis){
        auto rslt = sis;
        rslt.alpha *= 1-t.transition_parameter;
        return rslt;
    };
    return T;
}


slope::TransitionAnimator::TransitionAnimator()
{
    intro = [] (const TimeObject& t,const StateInSlide& sis){
        auto rslt = sis;
        rslt.alpha *= t.transition_parameter;
        return rslt;
    };
    outro = [] (const TimeObject& t,const StateInSlide& sis){
        auto rslt = sis;
        rslt.alpha *= 1-t.transition_parameter;
        return rslt;
    };
}
