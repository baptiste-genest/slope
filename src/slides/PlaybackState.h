#pragma once
#include <optional>
#include "../libslope.h"

namespace slope {

struct PlaybackState {
    size_t current  = 0;
    bool   locked   = true;
    bool   backward = false;
    bool   done     = false;
    std::optional<TimeStamp> pause_since;
    TimeStamp from_action;
    int visited = -1;

    PlaybackState() : from_action(Time::now()) {}

    void goForward()  { current++; _resetNav(false, true);  }
    void goBackward() { current--; _resetNav(true,  true);  }
    void skip()       { current++; _resetNav(false, false); }
    void jumpTo(size_t i) { current = i; from_action = Time::now(); pause_since.reset(); }
    void settle()     { locked = false; }
    void startPause() { pause_since = Time::now(); }
    void stopPause()  { pause_since.reset(); }
    bool isPaused()   const { return pause_since.has_value(); }

private:
    void _resetNav(bool bwd, bool lck) {
        from_action = Time::now();
        locked      = lck;
        backward    = bwd;
        done        = false;
        pause_since.reset();
    }
};

} // namespace slope
