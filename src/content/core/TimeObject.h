#ifndef TIMEOBJECT_H
#define TIMEOBJECT_H
#include "libslope.h"

namespace slope {

/*
 * The clock every animation reads, with the same interface in three languages.
 *
 * Whatever is added here is added to all three, under the same name and with
 * the same arguments.
 *
 *   C++    t.duringKeyframe("a")        this struct
 *   Lua    t:duringKeyframe("a")        the bindings in Snippet.cpp
 *   GLSL   duringKeyframe("a")          the prelude in Shader.cpp
 *
 * GLSL has no string type, so a keyframe name there is substituted for its
 * slide index before the compile. The one allowed divergence is a snippet
 * having no moment of appearing, which leaves it without inner_time and
 * relative_frame_number.
 */
struct TimeObject
{
    TimeTypeSec from_begin = 0;
    TimeTypeSec from_action = 0;
    TimeTypeSec inner_time = 0;
    TimeTypeSec delta_time = 0;
    int absolute_frame_number = 0;
    int relative_frame_number = 0;
    // Goes from 0 to 1 during the intro or outro of this primitive, eased by its animator.
    parameter transition_parameter = 1;
    // Goes from 0 to 1 during the whole slide change, the same for every primitive.
    parameter slide_progress = 1;

    // Static, so a TimeObject built outside the main loop also answers keyframe queries.
    static const std::map<std::string, int>* keyframes;
    // Time in seconds when each slide was first reached.
    // Going back drops the later entries, so a slide entered again restarts.
    static const std::map<int, TimeTypeSec>* slide_times;

    // Slide used by the boolean queries below, which changes at the middle of a transition.
    int shownFrame() const;

    // True from the keyframe onward.
    bool afterKeyframe(const std::string& name) const;
    // True before the keyframe.
    bool beforeKeyframe(const std::string& name) const;
    // True on the slide of the keyframe only.
    bool atKeyframe(const std::string& name) const;

    // Returned by slidesSinceKeyframe for an unknown keyframe, so tests like "... >= n" stay false.
    static constexpr int keyframe_unreached = -(1 << 24);

    // Number of slides since a keyframe, negative before it and 0 on it.
    //   stage = std::clamp(t.slidesSinceKeyframe("build"), 0, 3);
    int slidesSinceKeyframe(const std::string& name) const;

    // Seconds since a keyframe was reached, to be used as the clock of an ease.
    // from_action restarts on every slide change, so easing on it jumps back.
    //   scalar a = smoothstep(t.secondsSinceKeyframe("wobble") / 0.8);
    // It is 0 until the keyframe is reached and never negative.
    TimeTypeSec secondsSinceKeyframe(const std::string& name) const;

    // Position in the show counted in slides. It moves smoothly from one slide to the next during a transition.
    parameter slidePosition() const;

    // Weight that is 0, rises to 1 during the transition into `from`, stays 1 until `to`,
    // and falls back to 0 during the transition out of it.
    // The weights of neighbouring slides always sum to 1, so a value is a plain blend of states.
    //
    //   z1 = rest*t.duringKeyframe("a") + moved*t.duringKeyframe("b");
    //
    // One name means a single slide. An unknown name gives 0.
    // With `sequential`, a window closes before the next one opens, and each ramp takes half
    // of the transition. This avoids showing two states mixed.
    parameter duringKeyframe(const std::string& name, bool sequential = false) const;
    parameter duringKeyframe(const std::string& from, const std::string& to,
                             bool sequential = false) const;

    // Rises like duringKeyframe("a") and stays at 1 after its keyframe.
    //   opacity = t.sinceKeyframe("reveal");
    parameter sinceKeyframe(const std::string& name, bool sequential = false) const;

    TimeObject() {}
    // Time object with the given inner time and transition parameter.
    TimeObject(TimeTypeSec it,parameter transition)
        : inner_time(it),transition_parameter(transition),slide_progress(transition) {}

    // Copy of this object with the inner time and relative frame number of p.
    TimeObject operator()(Primitive* p) const ;

    // Copy of this object with another transition parameter.
    inline TimeObject operator()(parameter t) const {
        TimeObject tmp = *this;
        tmp.transition_parameter = t;
        return tmp;
    }
};

// Called every frame with the current time.
using Updater = std::function<void(TimeObject)>;

// Gives the position of a vertex at a given time.
using VertexTimeMap = std::function<vec(const Vertex&,const TimeObject&)>;

}

#endif // TIMEOBJECT_H
