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
