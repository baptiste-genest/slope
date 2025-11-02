#ifndef PANEL_H
#define PANEL_H

#include "../content/Anchor.h"
#include "../content/Placement.h"
#include "../content/ScreenPrimitive.h"
#include "SlideManager.h"
#include "Slide.h"

namespace slope {

class SlideManager;

class Panel {
    vec2 meanpos;
    AnchorPtr anchor;
    ScreenPrimitiveInSlide last_inserted = {nullptr,StateInSlide()};
    ScreenPrimitivePtr root = nullptr;
    bool reveal;
    Slide buffer;

    std::vector<RelativePlacement> rel_hist;

public:
    Panel(bool reveal = false,vec2 p = CENTER);

    AnchorPtr getAnchor() const;

    Panel& operator<<(ScreenPrimitivePtr ptr);

    Panel& operator<<(const RelativePlacement& P);

    void addToSlideManager(SlideManager& sm);
};

inline SlideManager& operator<<(SlideManager& sm,Panel& p) {
    p.addToSlideManager(sm);
    return sm;
}

}

#endif // PANEL_H
