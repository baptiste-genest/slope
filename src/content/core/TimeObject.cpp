#include "content/core/TimeObject.h"
#include "spdlog/spdlog.h"
#include <set>
#include <algorithm>
#include "math/kernels.h"
#include <spdlog/spdlog.h>

namespace slope {

const std::map<std::string, int>* TimeObject::keyframes = nullptr;
const std::map<int, TimeTypeSec>* TimeObject::slide_times = nullptr;

// -2 for an unknown label, warned once, so every query answers false
static int keyframeIndex(const std::map<std::string, int>* keyframes,
                         const std::string& name)
{
    if (keyframes) {
        auto it = keyframes->find(name);
        if (it != keyframes->end())
            return it->second;
    }
    static std::set<std::string> warned;
    if (warned.insert(name).second)
        spdlog::warn("unknown keyframe \"{}\"", name);
    return -2;
}

bool TimeObject::afterKeyframe(const std::string& name) const {
    int k = keyframeIndex(keyframes, name);
    return k >= 0 && absolute_frame_number >= k;
}

bool TimeObject::beforeKeyframe(const std::string& name) const {
    int k = keyframeIndex(keyframes, name);
    return k >= 0 && absolute_frame_number < k;
}

bool TimeObject::atKeyframe(const std::string& name) const {
    int k = keyframeIndex(keyframes, name);
    return absolute_frame_number == k;
}

TimeTypeSec TimeObject::secondsSinceKeyframe(const std::string& name) const {
    int k = keyframeIndex(keyframes, name);
    if (k < 0 || !slide_times)
        return 0;
    auto it = slide_times->find(k);
    if (it == slide_times->end())      // not reached yet
        return 0;
    return std::max<TimeTypeSec>(0, from_begin - it->second);
}

parameter TimeObject::slidePosition() const {
    // during a transition into slide N the parameter runs 0 to 1, and the
    // index is already N, so this leaves the previous slide and arrives at N
    return parameter(absolute_frame_number) - 1 + transition_parameter;
}

// The trapezoid, in slide position. Overlapping, a ramp spans a whole slide
// and neighbours cross at half weight; sequential doubles the slope so one
// window reaches 0 before the next leaves it.
static parameter window(parameter p, parameter a, parameter b, bool sequential) {
    const parameter k = sequential ? 2 : 1;
    const parameter rise = std::clamp<parameter>(k*(p - a) + 1, 0, 1);
    const parameter fall = std::clamp<parameter>(k*(b - p) + 1, 0, 1);
    return smoothstep(std::min(rise, fall));
}

parameter TimeObject::duringKeyframe(const std::string& name, bool sequential) const {
    int k = keyframeIndex(keyframes, name);
    if (k < 0) return 0;
    return window(slidePosition(), parameter(k), parameter(k), sequential);
}

parameter TimeObject::duringKeyframe(const std::string& from, const std::string& to,
                                     bool sequential) const {
    int a = keyframeIndex(keyframes, from);
    int b = keyframeIndex(keyframes, to);
    if (a < 0 || b < 0) return 0;
    if (b < a) std::swap(a, b);
    return window(slidePosition(), parameter(a), parameter(b), sequential);
}

int TimeObject::slidesSinceKeyframe(const std::string& name) const {
    int k = keyframeIndex(keyframes, name);
    if (k < 0)
        return keyframe_unreached;
    return absolute_frame_number - k;
}

}
