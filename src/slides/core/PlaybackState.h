#pragma once
#include <optional>
#include "libslope.h"

namespace slope {

// Where the show is and how it got there.
struct PlaybackState {
    // Index of the current slide.
    size_t current = 0;
    // True while the transition into the current slide is running.
    bool locked = true;
    // True when the last move was backward.
    bool backward = false;
    bool done = false;
    // Time when the pause started, empty when not paused.
    std::optional<TimeStamp> pause_since;
    // Time of the last slide change.
    TimeStamp from_action;
    // Index of the furthest slide reached, or -1.
    int visited = -1;

    PlaybackState() : from_action(Time::now()) {}

    // Moves to the next slide and starts its transition.
    void goForward() {
        current++;
        _resetNav(false, true);
    }
    // Moves to the previous slide and starts its transition.
    void goBackward() {
        current--;
        _resetNav(true, true);
    }
    // Moves to the next slide with no transition.
    void skip() {
        current++;
        _resetNav(false, false);
    }
    // Moves to slide i with no transition.
    void jumpTo(size_t i) {
        current = i;
        from_action = Time::now();
        pause_since.reset();
    }
    // Ends the transition.
    void settle() { locked = false; }
    // Pause and resume.
    void startPause() { pause_since = Time::now(); }
    // Ends the pause.
    void stopPause() { pause_since.reset(); }
    // True while paused.
    bool isPaused() const { return pause_since.has_value(); }

private:
    void _resetNav(bool bwd, bool lck) {
        from_action = Time::now();
        locked = lck;
        backward = bwd;
        done = false;
        pause_since.reset();
    }
};

} // namespace slope
