#ifndef PANEL_H
#define PANEL_H

#include "content/screen_primitives/layout/Anchor.h"
#include "content/screen_primitives/layout/Placement.h"
#include "slides/core/SlideManager.h"
#include "slides/core/Slide.h"

namespace slope {

class SlideManager;

// A group of screen primitives placed relative to each other around a common center.
// The first primitive added is the root, and the next ones are placed with RelativePlacement.
class Panel {
    vec2 meanpos;
    AnchorPtr anchor;
    ScreenPrimitiveInSlide last_inserted = {nullptr, StateInSlide()};
    ScreenPrimitivePtr root = nullptr;
    bool reveal;
    Slide buffer;

    std::vector<RelativePlacement> rel_hist;

public:
    // Panel centered at p. With reveal, each primitive after the root appears on its own slide.
    Panel(bool reveal = false, vec2 p = CENTER);

    // The anchor at the center of the panel.
    AnchorPtr getAnchor() const;

    // Sets the root primitive. Only the first primitive can be added without a placement.
    Panel& operator<<(ScreenPrimitivePtr ptr);

    // Places a primitive relative to the last one added, or to the one named by the placement.
    Panel& operator<<(const RelativePlacement& P);

    // Adds the primitives to the slide manager, centered on the anchor of the panel.
    void addToSlideManager(SlideManager& sm);
};

inline SlideManager& operator<<(SlideManager& sm, Panel& p) {
    p.addToSlideManager(sm);
    return sm;
}

} // namespace slope

#endif // PANEL_H
