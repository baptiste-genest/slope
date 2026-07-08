#include "TimeObject.h"
#include "spdlog/spdlog.h"
#include <set>

namespace slope {

const std::map<std::string, int>* TimeObject::keyframes = nullptr;

// -2 : unknown label (warned once), so every query answers false
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

}
