#ifndef TIMEOBJECT_H
#define TIMEOBJECT_H
#include "../libslope.h"

namespace slope {

struct TimeObject
{
    TimeTypeSec from_begin = 0;
    TimeTypeSec from_action = 0;
    TimeTypeSec inner_time = 0;
    int absolute_frame_number;
    int relative_frame_number;
    parameter transitionParameter = 1;

    TimeObject() {}
    TimeObject(TimeTypeSec it,parameter transition) : inner_time(it),transitionParameter(transition) {}

    TimeObject operator()(Primitive* p) const ;

    inline TimeObject operator()(parameter t) const {
        TimeObject tmp = *this;
        tmp.transitionParameter = t;
        return tmp;
    }
};

using Updater = std::function<void(TimeObject)>;

using VertexTimeMap = std::function<vec(const Vertex&,const TimeObject&)>;

}

#endif // TIMEOBJECT_H
