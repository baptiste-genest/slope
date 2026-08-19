#ifndef LIVEVEC_H
#define LIVEVEC_H

#include "../libslope.h"
#include <string>

namespace slope {

// a world vector given outright, or named by a snippet variable read each frame
struct LiveVec {
    vec fixed = vec::Zero();
    std::string snippet;

    bool live() const {return !snippet.empty();}
    vec value() const;
    // identifies the source, so a consumer can cache on it
    std::string key() const;
};

}

#endif // LIVEVEC_H
