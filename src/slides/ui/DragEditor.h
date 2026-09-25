#pragma once

#include "slides/core/Slide.h"
#include "slides/ui/WindowManager.h"

namespace slope {

// Selects screen primitives with the mouse and drags them, which edits the position of their label.
class DragEditor {
    PrimitivePtr selected_primitive = nullptr;
    double x_offset = 0;
    double y_offset = 0;
    TimeStamp time_at_pick;

    // Ctrl+Shift+click adds or removes a primitive from the group.
    // Holding the left click then moves the labels of all members by the movement of the mouse.
    struct Member {
        PrimitivePtr prim;
    };
    std::vector<Member> group;
    bool group_dragging = false;
    double drag_last_x = 0, drag_last_y = 0, drag_travel = 0;

    // Ctrl+Shift+drag draws a box, and when released every primitive inside it joins the group.
    // A release without movement is only a click that adds or removes one primitive.
    // Set when Ctrl+Shift is pressed and the drag has not started.
    bool marquee_pending = false;
    // Set when the mouse moved far enough to make a box.
    bool marquee_active = false;
    double marquee_start_x = 0, marquee_start_y = 0;
    PrimitivePtr marquee_press_hit = nullptr;

    // Ctrl+click again at about the same place selects the next primitive below the first one,
    // instead of selecting the top one again.
    // The stack of primitives is taken at the first click of a sequence.
    std::vector<PrimitivePtr> pick_stack;
    double last_pick_x = -1e9, last_pick_y = -1e9;
    size_t pick_cycle_index = 0;

    // Undo history. A drag writes to the session at every frame, so without it a wrong drag followed by Ctrl+S
    // could not be undone. One entry holds every label that the edit was about to change, saved before the first write.
    struct LabelState {
        std::string label;
        AnchorState state;
    };
    using UndoEntry = std::vector<LabelState>;
    static constexpr size_t max_undo = 100;

    std::vector<UndoEntry> undo_stack;
    // Saved when a primitive is picked, and pushed on the first real move.
    UndoEntry pending_undo;
    bool pending_committed = false;

    // Saves the state of the labels of some primitives.
    void captureUndo(const std::vector<PrimitivePtr>& prims, Slide& cs);
    // Pushes the saved state on the undo stack.
    void commitUndo();

    // Draws the outline of a primitive with its own rotated bounding box.
    void drawSelectionBox(const PrimitivePtr& prim, const StateInSlide& sis,
                          const ImVec2& S, float cx, float cy, float pulse) const;

    // Primitives at (x,y) in reverse drawing order, so the one on top comes first.
    std::vector<PrimitivePtr> getPrimitivesUnderMouse(Slide& s, scalar x, scalar y) const;
    // Picks a primitive at (x,y). Repeated clicks at the same place move down the stack.
    PrimitivePtr pickWithCycling(Slide& s, scalar x, scalar y);
    // Starts dragging a primitive picked at (x,y).
    void selectPrimitive(Slide& cs, WindowManager& wm, const PrimitivePtr& prim, scalar x, scalar y);
    // Ends the drag.
    void releasePrimitive(Slide& cs);

    // Adds a primitive to the group, or removes it.
    void toggleMember(Slide& cs, WindowManager& wm, const PrimitivePtr& prim);
    // Handles clicks and drags of the group.
    void handleGroup(Slide& cs, bool ctrl, bool shift, WindowManager& wm);
    // Empties the group.
    void clearGroup(Slide& cs, WindowManager& wm);
    // Handles the selection box.
    void handleMarquee(Slide& cs, WindowManager& wm);

public:
    DragEditor() : time_at_pick(Time::now()) {}

    // Handles the mouse for the current slide. Call it once per frame.
    void handle(Slide& current_slide, WindowManager& wm);

    // Reverts the last move. It first drops the selection, otherwise the drag in progress
    // would immediately overwrite what was restored. Returns false when there is nothing to undo.
    bool undo(Slide& current_slide, WindowManager& wm);

    // True when something is selected.
    bool isActive() const { return selected_primitive != nullptr || !group.empty(); }

    // The selected primitive, or null.
    PrimitivePtr getSelected() const { return selected_primitive; }

    // True while a primitive is being dropped with a plain click, which also lands on whatever is under it.
    // Primitives are drawn before the editor runs, so they read this to tell that click from one meant for them.
    static bool isPlacing() { return placing; }

private:
    static bool placing;
};

} // namespace slope
