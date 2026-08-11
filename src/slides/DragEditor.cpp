#include "DragEditor.h"
#include "../content/screen_primitives/Anchor.h"

std::vector<slope::PrimitivePtr> slope::DragEditor::getPrimitivesUnderMouse(Slide& s, scalar x, scalar y) const
{
    auto S = ImGui::GetWindowSize();
    std::vector<PrimitivePtr> hits;
    auto sorted = s.getDepthSorted(); // back-to-front draw order
    for (auto it = sorted.rbegin(); it != sorted.rend(); ++it) { // front-to-back : topmost first
        auto& [ptr, sis] = *it;
        if (!ptr->isScreenSpace())
            continue;
        if (!sis.anchor->isPersistent())
            continue;
        // only label-anchored primitives have a writable position, so only
        // they can be picked or dragged ; this also keeps the englobing
        // boxes (depth -100, spanning most of the screen) out of the way
        if (std::dynamic_pointer_cast<LabelAnchor>(sis.anchor) == nullptr)
            continue;
        auto sp = std::static_pointer_cast<ScreenPrimitive>(ptr);
        auto p = sis.getPosition();
        auto prim_size = sp->getSize() * sis.getScale();
        if (std::abs(p(0) - x) < prim_size(0)/2/S.x && std::abs(p(1) - y) < prim_size(1)/2/S.y)
            hits.push_back(ptr);
    }
    return hits;
}

slope::PrimitivePtr slope::DragEditor::pickWithCycling(Slide& s, scalar x, scalar y)
{
    constexpr double same_spot_thr = 0.01;
    bool same_spot = std::abs(x - last_pick_x) < same_spot_thr
                  && std::abs(y - last_pick_y) < same_spot_thr
                  && !pick_stack.empty();

    if (same_spot) {
        // keep the stack captured at the first click of the sequence : the
        // primitive picked last is sitting under the cursor and would
        // otherwise dominate every subsequent hit test
        pick_cycle_index = (pick_cycle_index + 1) % pick_stack.size();
    } else {
        pick_stack = getPrimitivesUnderMouse(s, x, y);
        pick_cycle_index = 0;
        last_pick_x = x;
        last_pick_y = y;
    }

    if (pick_stack.empty()) {
        last_pick_x = last_pick_y = -1e9;
        return nullptr;
    }
    return pick_stack[pick_cycle_index];
}

void slope::DragEditor::captureUndo(const std::vector<PrimitivePtr>& prims, Slide& cs)
{
    pending_undo.clear();
    pending_committed = false;
    for (const auto& p : prims) {
        auto it = cs.find(p);
        if (it == cs.end())
            continue;
        auto lab = std::dynamic_pointer_cast<LabelAnchor>(it->second.anchor);
        if (lab == nullptr)
            continue;
        auto v = lab->readFromLabel();
        pending_undo.push_back({lab->getLabel(), v(0), v(1), v(2)});
    }
}

void slope::DragEditor::commitUndo()
{
    if (pending_committed || pending_undo.empty())
        return;
    undo_stack.push_back(pending_undo);
    if (undo_stack.size() > max_undo)
        undo_stack.erase(undo_stack.begin());
    pending_committed = true;
}

bool slope::DragEditor::undo(Slide& cs, WindowManager& wm)
{
    // whatever is being dragged would write over the restored values on the
    // next frame, so let go of it first
    releasePrimitive(cs);
    if (!group.empty())
        clearGroup(cs, wm);
    else if (wm.isOpen(WindowType::DragAndDrop))
        wm.Toggle(WindowType::DragAndDrop);
    pick_stack.clear();
    last_pick_x = last_pick_y = -1e9;
    pending_undo.clear();
    pending_committed = false;

    if (undo_stack.empty()) {
        spdlog::info("nothing to undo");
        return false;
    }
    for (const auto& s : undo_stack.back())
        LabelAnchor::writeToSessionAt(s.label, s.x, s.y, s.scale);
    undo_stack.pop_back();
    spdlog::info("undo ({} left)", undo_stack.size());
    return true;
}

void slope::DragEditor::selectPrimitive(Slide& cs, WindowManager& wm, const PrimitivePtr& prim, scalar x, scalar y)
{
    auto it = cs.find(prim);
    if (it == cs.end())
        return;
    auto lab = std::dynamic_pointer_cast<LabelAnchor>(it->second.anchor);
    if (lab == nullptr)
        return;
    captureUndo({prim}, cs);
    selected_primitive = prim;
    x_offset       = lab->getPos()(0) - x;
    y_offset       = lab->getPos()(1) - y;
    original_alpha = it->second.alpha;
    time_at_pick   = Time::now();
    if (!wm.isOpen(WindowType::DragAndDrop))
        wm.Toggle(WindowType::DragAndDrop);
}

void slope::DragEditor::releasePrimitive(Slide& cs)
{
    if (selected_primitive == nullptr)
        return;
    auto it = cs.find(selected_primitive);
    if (it != cs.end())
        it->second.alpha = original_alpha;
    selected_primitive = nullptr;
}

void slope::DragEditor::toggleMember(Slide& cs, WindowManager& wm, const PrimitivePtr& prim)
{
    auto member = std::find_if(group.begin(), group.end(),
                               [&](const Member& m) { return m.prim == prim; });
    if (member != group.end()) {
        auto it = cs.find(prim);
        if (it != cs.end())
            it->second.alpha = member->original_alpha;
        group.erase(member);
        if (group.empty())
            clearGroup(cs, wm);
        return;
    }
    auto it = cs.find(prim);
    if (it == cs.end())
        return;
    // only label-anchored primitives have a writable position
    if (std::dynamic_pointer_cast<LabelAnchor>(it->second.anchor) == nullptr)
        return;
    if (group.empty()) {
        wm.Toggle(WindowType::DragAndDrop);
        time_at_pick = Time::now();
    }
    group.push_back({prim, it->second.alpha});
}

void slope::DragEditor::clearGroup(Slide& cs, WindowManager& wm)
{
    for (const auto& m : group) {
        auto it = cs.find(m.prim);
        if (it != cs.end())
            it->second.alpha = m.original_alpha;
    }
    group.clear();
    group_dragging = false;
    if (wm.isOpen(WindowType::DragAndDrop))
        wm.Toggle(WindowType::DragAndDrop);
}

void slope::DragEditor::handleMarquee(Slide& cs, WindowManager& wm)
{
    auto io = ImGui::GetIO();
    ImGui::SetNextFrameWantCaptureMouse(true);
    ImGui::SetNextFrameWantCaptureKeyboard(false);

    auto S = ImGui::GetWindowSize();
    double x = double(io.MousePos.x) / S.x;
    double y = double(io.MousePos.y) / S.y;

    // a ctrl+shift press only becomes a marquee once it has travelled :
    // a press released in place stays a plain toggle-click
    constexpr double marquee_thr = 0.01;
    if (!marquee_active
        && (std::abs(x - marquee_start_x) > marquee_thr || std::abs(y - marquee_start_y) > marquee_thr))
        marquee_active = true;

    if (marquee_active) {
        auto* dl = ImGui::GetWindowDrawList();
        constexpr ImU32 fill_col   = IM_COL32(80, 180, 255, 40);
        constexpr ImU32 border_col = IM_COL32(80, 180, 255, 200);
        ImVec2 p0{ float(std::min(marquee_start_x, x)) * S.x, float(std::min(marquee_start_y, y)) * S.y };
        ImVec2 p1{ float(std::max(marquee_start_x, x)) * S.x, float(std::max(marquee_start_y, y)) * S.y };
        dl->AddRectFilled(p0, p1, fill_col);
        dl->AddRect(p0, p1, border_col);
    }

    if (!io.MouseReleased[0])
        return;

    if (!marquee_active) { // never travelled : behave like the old toggle-click
        if (marquee_press_hit != nullptr)
            toggleMember(cs, wm, marquee_press_hit);
        else
            clearGroup(cs, wm);
    } else {
        double xmin = std::min(marquee_start_x, x), xmax = std::max(marquee_start_x, x);
        double ymin = std::min(marquee_start_y, y), ymax = std::max(marquee_start_y, y);
        bool was_empty = group.empty();
        for (auto& [pptr, sis] : cs.getScreenPrimitives()) {
            if (!sis.anchor->isPersistent())
                continue;
            if (std::dynamic_pointer_cast<LabelAnchor>(sis.anchor) == nullptr)
                continue;
            // the primitive must be entirely englobed by the rectangle, not
            // merely have its centre inside it
            auto p = sis.getPosition();
            float hw = pptr->getRelativeSize()(0) * float(sis.getScale()) * 0.5f;
            float hh = pptr->getRelativeSize()(1) * float(sis.getScale()) * 0.5f;
            if (p(0) - hw < xmin || p(0) + hw > xmax
             || p(1) - hh < ymin || p(1) + hh > ymax)
                continue;
            bool already = std::any_of(group.begin(), group.end(),
                                       [&](const Member& m) { return m.prim == pptr; });
            if (!already)
                group.push_back({pptr, sis.alpha});
        }
        if (was_empty && !group.empty()) {
            wm.Toggle(WindowType::DragAndDrop);
            time_at_pick = Time::now();
        }
    }

    marquee_pending = false;
    marquee_active  = false;
    marquee_press_hit = nullptr;
}

void slope::DragEditor::handleGroup(Slide& cs, bool ctrl, bool shift, WindowManager& wm)
{
    auto io = ImGui::GetIO();
    ImGui::SetNextFrameWantCaptureMouse(true);
    ImGui::SetNextFrameWantCaptureKeyboard(false);

    // pulse the members so the selection is visible
    scalar pulse = (std::cos(TimeFrom(time_at_pick) * 5) + 1) * 0.8 + 0.2;
    for (const auto& m : group) {
        auto it = cs.find(m.prim);
        if (it != cs.end())
            it->second.alpha = pulse;
    }

    auto S = ImGui::GetWindowSize();
    double x = double(io.MousePos.x) / S.x;
    double y = double(io.MousePos.y) / S.y;

    if (ctrl && shift) { // still selecting : clicks must not start a drag
        group_dragging = false;
        return;
    }

    if (io.MouseDown[0]) {
        if (!group_dragging) {
            group_dragging = true;
            drag_travel = 0;
            std::vector<PrimitivePtr> prims;
            for (const auto& m : group)
                prims.push_back(m.prim);
            captureUndo(prims, cs);
        } else {
            double dx = x - drag_last_x, dy = y - drag_last_y;
            drag_travel += std::abs(dx) + std::abs(dy);
            if (dx != 0 || dy != 0) {
                commitUndo();
                for (const auto& m : group) {
                    auto it = cs.find(m.prim);
                    if (it == cs.end())
                        continue;
                    auto lab = std::dynamic_pointer_cast<LabelAnchor>(it->second.anchor);
                    if (lab == nullptr)
                        continue;
                    auto p = lab->getPos();
                    lab->writePosAtLabel(p(0) + dx, p(1) + dy, true);
                }
            }
        }
        drag_last_x = x;
        drag_last_y = y;
    }
    else if (io.MouseReleased[0]) {
        // a plain click that did not drag dismisses the selection
        if (group_dragging && drag_travel < 0.003)
            clearGroup(cs, wm);
        group_dragging = false;
    }
}

bool slope::DragEditor::placing = false;

void slope::DragEditor::handle(Slide& cs, WindowManager& wm)
{
    placing = isActive();
    if (wm.isOtherOpen(WindowType::DragAndDrop))
        return;

    auto io = ImGui::GetIO();
    bool ctrl  = ImGui::IsKeyDown(ImGuiKey_LeftCtrl);
    bool shift = ImGui::IsKeyDown(ImGuiKey_LeftShift);
    bool click = io.MouseClicked[0];

    if (marquee_pending) {
        handleMarquee(cs, wm);
        return;
    }

    if (ctrl && shift && click && selected_primitive == nullptr) {
        auto S = ImGui::GetWindowSize();
        auto x = double(io.MousePos.x) / S.x;
        auto y = double(io.MousePos.y) / S.y;
        auto hits = getPrimitivesUnderMouse(cs, x, y);
        // defer the decision to the release : travelling makes it a marquee,
        // staying put makes it a toggle-click on whatever was under the cursor
        marquee_pending   = true;
        marquee_active    = false;
        marquee_start_x   = x;
        marquee_start_y   = y;
        marquee_press_hit = hits.empty() ? nullptr : hits.front();
        return;
    }
    if (!group.empty() && selected_primitive == nullptr) {
        handleGroup(cs, ctrl, shift, wm);
        return;
    }

    if (ctrl && !shift && click) {
        auto S = ImGui::GetWindowSize();
        auto x = double(io.MousePos.x) / S.x;
        auto y = double(io.MousePos.y) / S.y;
        // ctrl+clicking again at the same spot cycles to the primitive below
        releasePrimitive(cs);
        auto prim = pickWithCycling(cs, x, y);
        if (prim != nullptr)
            selectPrimitive(cs, wm, prim, x, y);
        else if (wm.isOpen(WindowType::DragAndDrop))
            wm.Toggle(WindowType::DragAndDrop);
    }

    if (!ctrl && click && selected_primitive != nullptr) {
        releasePrimitive(cs);
        pick_stack.clear();
        last_pick_x = last_pick_y = -1e9;
        if (wm.isOpen(WindowType::DragAndDrop))
            wm.Toggle(WindowType::DragAndDrop);
        return;
    }

    if (selected_primitive == nullptr)
        return;

    bool horizontal = ImGui::IsKeyDown(ImGuiKey_H);
    bool vertical   = ImGui::IsKeyDown(ImGuiKey_V);

    ImGui::SetNextFrameWantCaptureKeyboard(false);
    ImGui::SetNextFrameWantCaptureMouse(true);

    auto S = ImGui::GetWindowSize();
    auto x = double(io.MousePos.x) / S.x;
    auto y = double(io.MousePos.y) / S.y;
    auto& pis = cs[selected_primitive];
    LabelAnchorPtr lab = std::dynamic_pointer_cast<LabelAnchor>(pis.anchor);
    if (lab == nullptr) { // nothing writable to drag
        selected_primitive = nullptr;
        return;
    }

    constexpr scalar zoom = 1.1;
    if (io.MouseWheel != 0.0f)
        commitUndo();
    if (io.MouseWheel > 0.0f)
        lab->writeScaleAtLabel(lab->getScale() * zoom, true);
    else if (io.MouseWheel < 0.0f)
        lab->writeScaleAtLabel(lab->getScale() / zoom, true);

    if (horizontal) { x_offset = 0.5 - x; x = 0.5; }
    if (vertical)   { y_offset = 0.5 - y; y = 0.5; }

    float cx = float(x + x_offset);
    float cy = float(y + y_offset);

    auto drag_sp = std::static_pointer_cast<ScreenPrimitive>(selected_primitive);
    float dhw = drag_sp->getRelativeSize()(0) * float(pis.getScale()) * 0.5f;
    float dhh = drag_sp->getRelativeSize()(1) * float(pis.getScale()) * 0.5f;
    float d_ax[3] = { cx - dhw, cx, cx + dhw };
    float d_ay[3] = { cy - dhh, cy, cy + dhh };

    constexpr float snap_thr = 0.005f;
    float snap_dx = snap_thr, snap_dy = snap_thr;
    float guide_x = -1.f, guide_y = -1.f;

    for (auto& [pptr, sis] : cs.getScreenPrimitives()) {
        if (pptr == drag_sp) continue;
        auto op = sis.getPosition();
        float ohw = pptr->getRelativeSize()(0) * float(sis.getScale()) * 0.5f;
        float ohh = pptr->getRelativeSize()(1) * float(sis.getScale()) * 0.5f;
        float o_ax[3] = { float(op(0))-ohw, float(op(0)), float(op(0))+ohw };
        float o_ay[3] = { float(op(1))-ohh, float(op(1)), float(op(1))+ohh };
        for (int di = 0; di < 3; ++di)
            for (int oi = 0; oi < 3; ++oi) {
                float dx = o_ax[oi] - d_ax[di];
                if (std::abs(dx) < std::abs(snap_dx)) { snap_dx = dx; guide_x = o_ax[oi]; }
                float dy = o_ay[oi] - d_ay[di];
                if (std::abs(dy) < std::abs(snap_dy)) { snap_dy = dy; guide_y = o_ay[oi]; }
            }
    }

    auto* dl = ImGui::GetWindowDrawList();
    constexpr ImU32 guide_col = IM_COL32(80, 180, 255, 200);

    if (!horizontal && std::abs(snap_dx) < snap_thr) {
        cx += snap_dx;
        dl->AddLine({guide_x * S.x, 0.f}, {guide_x * S.x, S.y}, guide_col, 1.0f);
    }
    if (!vertical && std::abs(snap_dy) < snap_thr) {
        cy += snap_dy;
        dl->AddLine({0.f, guide_y * S.y}, {S.x, guide_y * S.y}, guide_col, 1.0f);
    }

    // the position is rewritten every frame even when the mouse is still, so
    // only an actual displacement is worth an undo entry
    if (!pending_undo.empty()) {
        constexpr double move_eps = 1e-6;
        if (std::abs(cx - pending_undo.front().x) > move_eps
         || std::abs(cy - pending_undo.front().y) > move_eps)
            commitUndo();
    }

    lab->writePosAtLabel(cx, cy, true);
    pis.alpha = (std::cos(TimeFrom(time_at_pick) * 5) + 1) * 0.8 + 0.2;
}
