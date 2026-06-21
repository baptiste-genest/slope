#pragma once

#include "Slide.h"
#include "WindowManager.h"

namespace slope {

class DragEditor {
    PrimitivePtr selected_primitive = nullptr;
    double x_offset = 0;
    double y_offset = 0;
    double original_alpha = 0;
    TimeStamp time_at_pick;

    PrimitivePtr getPrimitiveUnderMouse(const Slide& s, scalar x, scalar y) const;

public:
    DragEditor() : time_at_pick(Time::now()) {}

    void handle(Slide& current_slide, WindowManager& wm);

    bool isActive() const { return selected_primitive != nullptr; }
};

} // namespace slope
