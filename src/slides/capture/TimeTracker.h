#pragma once

#include "libslope.h"
#include <map>
#include <functional>
#include <string>

namespace slope {

class TimeTracker {
    std::map<std::string, TimeTypeSec> time_per_slide_group;
    TimeTypeSec time_from_start = 0;
    TimeStamp last_recorded_time;
    bool started = false;
    // stops the rehearsal clock only, unlike SlideState's pause
    bool paused = false;

    // timings of the previous run, the reference to beat
    std::map<std::string, TimeTypeSec> previous_time_per_slide_group;
    TimeTypeSec previous_time_from_start = 0;
    bool has_previous = false;

    static std::string formatTime(float totalSeconds);
    static std::string formatDelta(float seconds);
    static path file();

public:
    TimeTracker() : last_recorded_time(Time::now()) {}

    void start();
    void record(const std::string& slide_title);
    void reset();

    void togglePause();
    bool isPaused() const { return paused; }

    // loaded at startup, saved when the presentation exits
    void load();
    void save() const;

    // whether this run holds timings worth saving, mirrors the guard in save()
    bool hasRecordableSession() const { return started && time_from_start >= 1; }

    void drawMenu(int n_slides,
                  const std::function<std::string(int)>& get_title,
                  const std::function<void(int)>& go_to_slide);
};

} // namespace slope
