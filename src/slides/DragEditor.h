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

    // group selection mode : ctrl+shift+click toggles members in and out
    // without moving them; holding left click then translates all their
    // labels by the mouse delta, preserving relative spacing
    struct Member {
        PrimitivePtr prim;
        scalar original_alpha;
    };
    std::vector<Member> group;
    bool group_dragging = false;
    double drag_last_x = 0, drag_last_y = 0, drag_travel = 0;

    PrimitivePtr getPrimitiveUnderMouse(const Slide& s, scalar x, scalar y) const;

    void toggleMember(Slide& cs, WindowManager& wm, const PrimitivePtr& prim);
    void handleGroup(Slide& cs, bool ctrl, bool shift, WindowManager& wm);
    void clearGroup(Slide& cs, WindowManager& wm);

public:
    DragEditor() : time_at_pick(Time::now()) {}

    void handle(Slide& current_slide, WindowManager& wm);

    bool isActive() const { return selected_primitive != nullptr || !group.empty(); }
};

} // namespace slope
