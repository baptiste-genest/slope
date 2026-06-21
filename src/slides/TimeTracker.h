#pragma once

#include "../libslope.h"
#include <map>
#include <functional>
#include <string>

namespace slope {

class TimeTracker {
    std::map<std::string, TimeTypeSec> time_per_slide_group;
    TimeTypeSec time_from_start = 0;
    TimeStamp last_recorded_time;
    bool started = false;

    static std::string formatTime(float totalSeconds);

public:
    TimeTracker() : last_recorded_time(Time::now()) {}

    void start();
    void record(const std::string& slide_title);
    void reset();

    void drawMenu(int n_slides,
                  const std::function<std::string(int)>& get_title,
                  const std::function<void(int)>& go_to_slide);
};

} // namespace slope
