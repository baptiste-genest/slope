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

    // rectangle (marquee) selection : ctrl+shift+drag draws a box; on
    // release every primitive whose position falls inside it joins the
    // group. A ctrl+shift press that is released without travelling stays
    // a plain toggle-click instead.
    bool marquee_pending = false;   // ctrl+shift pressed, not yet a drag
    bool marquee_active = false;    // travelled far enough : it is a marquee
    double marquee_start_x = 0, marquee_start_y = 0;
    PrimitivePtr marquee_press_hit = nullptr;

    // ctrl+clicking again at (roughly) the same spot cycles down through
    // the primitives stacked at that point instead of re-grabbing the
    // topmost one. The stack is captured at the first click of a sequence.
    std::vector<PrimitivePtr> pick_stack;
    double last_pick_x = -1e9, last_pick_y = -1e9;
    size_t pick_cycle_index = 0;

    // undo history : positions are written to the session on every frame of a
    // drag, so without this a mis-drag followed by Ctrl+S is unrecoverable.
    // One entry holds the state of every label an edit was about to touch,
    // captured before the first write.
    struct LabelState {
        std::string label;
        double x, y, scale;
    };
    using UndoEntry = std::vector<LabelState>;
    static constexpr size_t max_undo = 100;

    std::vector<UndoEntry> undo_stack;
    UndoEntry pending_undo;       // captured on pick, pushed on first real move
    bool pending_committed = false;

    void captureUndo(const std::vector<PrimitivePtr>& prims, Slide& cs);
    void commitUndo();

    // topmost-first : reverse depth order, so the primitive actually drawn
    // on top of the stack at (x,y) comes first
    std::vector<PrimitivePtr> getPrimitivesUnderMouse(Slide& s, scalar x, scalar y) const;
    // picks at (x,y), advancing through the stack on repeated same-spot clicks
    PrimitivePtr pickWithCycling(Slide& s, scalar x, scalar y);
    void selectPrimitive(Slide& cs, WindowManager& wm, const PrimitivePtr& prim, scalar x, scalar y);
    void releasePrimitive(Slide& cs);

    void toggleMember(Slide& cs, WindowManager& wm, const PrimitivePtr& prim);
    void handleGroup(Slide& cs, bool ctrl, bool shift, WindowManager& wm);
    void clearGroup(Slide& cs, WindowManager& wm);
    void handleMarquee(Slide& cs, WindowManager& wm);

public:
    DragEditor() : time_at_pick(Time::now()) {}

    void handle(Slide& current_slide, WindowManager& wm);

    // reverts the last move ; drops any selection first, otherwise the drag in
    // progress would immediately overwrite what we just restored
    bool undo(Slide& current_slide, WindowManager& wm);

    bool isActive() const { return selected_primitive != nullptr || !group.empty(); }
};

} // namespace slope
