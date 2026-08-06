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

    // set by the slideshow : static, so the TimeObjects built outside the
    // main play path (backward navigation, transitions) answer keyframe
    // queries too
    static const std::map<std::string, int>* keyframes;

    bool afterKeyframe(const std::string& name) const;
    bool beforeKeyframe(const std::string& name) const;
    bool atKeyframe(const std::string& name) const;

    // an unknown keyframe is never reached : slidesSinceKeyframe answers this,
    // so the "... >= n" tests built on it stay false exactly like afterKeyframe
    static constexpr int keyframe_unreached = -(1 << 24);

    // distance in slides from a keyframe : negative before it, 0 on it,
    // positive after. Lets a primitive stage itself over several slides —
    //   stage = std::clamp(t.slidesSinceKeyframe("build"), 0, 3);
    // — instead of chaining afterKeyframe() tests.
    int slidesSinceKeyframe(const std::string& name) const;

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
