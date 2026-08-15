#ifndef TIMEOBJECT_H
#define TIMEOBJECT_H
#include "../libslope.h"

namespace slope {

struct TimeObject
{
    TimeTypeSec from_begin = 0;
    TimeTypeSec from_action = 0;
    TimeTypeSec inner_time = 0;
    TimeTypeSec delta_time = 0;
    int absolute_frame_number = 0;
    int relative_frame_number = 0;
    parameter transition_parameter = 1;

    // static, so the TimeObjects built outside the main play path answer
    // keyframe queries too
    static const std::map<std::string, int>* keyframes;
    // when each slide was first reached, in seconds from the start of the show.
    // Going back drops the later entries, so re-entering a slide restarts it.
    static const std::map<int, TimeTypeSec>* slide_times;

    bool afterKeyframe(const std::string& name) const;
    bool beforeKeyframe(const std::string& name) const;
    bool atKeyframe(const std::string& name) const;

    // an unknown keyframe is never reached, so the "... >= n" tests built on
    // slidesSinceKeyframe stay false like afterKeyframe
    static constexpr int keyframe_unreached = -(1 << 24);

    // distance in slides from a keyframe, negative before it and 0 on it.
    //   stage = std::clamp(t.slidesSinceKeyframe("build"), 0, 3);
    int slidesSinceKeyframe(const std::string& name) const;

    // Seconds since a keyframe was reached, which is the clock an ease wants.
    // from_action restarts on every slide change, including ones that have
    // nothing to do with the animation, so easing on it snaps back.
    //   scalar a = smoothstep(t.secondsSinceKeyframe("wobble") / 0.8);
    // Never negative, 0 until the keyframe is reached, so an ease needs no guard.
    TimeTypeSec secondsSinceKeyframe(const std::string& name) const;

    // Where the show is, counted in slides but continuous, sliding from one to
    // the next across a transition.
    parameter slidePosition() const;

    // A weight that is 0, rises to 1 across the transition into `from`, stays
    // 1 until `to`, and falls back to 0 across the transition out of it. The
    // ramps are the transition itself, so the weights of neighbouring slides
    // always sum to 1 and a value can be written as a plain blend of states :
    //
    //   z1 = rest*t.duringKeyframe("a") + moved*t.duringKeyframe("b");
    //
    // One name is a single slide. Unknown names weigh 0. The ramps are eased,
    // which keeps the sum at 1 since smoothstep(x) + smoothstep(1-x) == 1.
    //
    // `sequential` makes a window close before the next one opens, each ramp
    // taking half the transition instead of all of it, for states that should
    // not be seen mixed.
    parameter duringKeyframe(const std::string& name, bool sequential = false) const;
    parameter duringKeyframe(const std::string& from, const std::string& to,
                             bool sequential = false) const;

    TimeObject() {}
    TimeObject(TimeTypeSec it,parameter transition) : inner_time(it),transition_parameter(transition) {}

    TimeObject operator()(Primitive* p) const ;

    inline TimeObject operator()(parameter t) const {
        TimeObject tmp = *this;
        tmp.transition_parameter = t;
        return tmp;
    }
};

using Updater = std::function<void(TimeObject)>;

using VertexTimeMap = std::function<vec(const Vertex&,const TimeObject&)>;

}

#endif // TIMEOBJECT_H
