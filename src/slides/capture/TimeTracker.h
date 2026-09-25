#pragma once

#include "libslope.h"
#include <map>
#include <functional>
#include <string>

namespace slope {

// Rehearsal timer. It records the time spent on each slide and compares it with the previous run.
class TimeTracker {
    std::map<std::string, TimeTypeSec> time_per_slide_group;
    TimeTypeSec time_from_start = 0;
    TimeStamp last_recorded_time;
    bool started = false;
    // Stops the rehearsal clock only, unlike the pause of PlaybackState.
    bool paused = false;

    // Timings of the previous run, used as reference.
    std::map<std::string, TimeTypeSec> previous_time_per_slide_group;
    TimeTypeSec previous_time_from_start = 0;
    bool has_previous = false;

    // Text for a duration, and for a difference with the previous run.
    static std::string formatTime(float totalSeconds);
    static std::string formatDelta(float seconds);
    // Path of the file where timings are saved.
    static path file();

public:
    TimeTracker() : last_recorded_time(Time::now()) {}

    // Starts the clock.
    void start();
    // Adds the time since the last call to the slide with this title.
    void record(const std::string& slide_title);
    // Erases the timings of this run.
    void reset();

    // Stops or resumes the rehearsal clock.
    void togglePause();
    // True while the rehearsal clock is stopped.
    bool isPaused() const { return paused; }

    // Reads the previous timings at startup, and writes the new ones when the presentation exits.
    void load();
    // Writes the timings of this run to the file.
    void save() const;

    // True when this run has timings worth saving. It follows the same rule as save().
    bool hasRecordableSession() const { return started && time_from_start >= 1; }

    // Draws the menu with the timings and a list of slides to jump to.
    void drawMenu(int n_slides,
                  const std::function<std::string(int)>& get_title,
                  const std::function<void(int)>& go_to_slide);
};

} // namespace slope
