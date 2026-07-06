#include "DragEditor.h"
#include "../content/screen_primitives/Anchor.h"

slope::PrimitivePtr slope::DragEditor::getPrimitiveUnderMouse(const Slide& s, scalar x, scalar y) const
{
    auto S = ImGui::GetWindowSize();
    for (auto& pis : s.getScreenPrimitives()) {
        if (!pis.second.anchor->isPersistent())
            continue;
        auto p = pis.second.getPosition();
        auto prim_size = pis.first->getSize() * pis.second.getScale();
        if (std::abs(p(0) - x) < prim_size(0)/2/S.x && std::abs(p(1) - y) < prim_size(1)/2/S.y)
            return pis.first;
    }
    return nullptr;
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
        } else {
            double dx = x - drag_last_x, dy = y - drag_last_y;
            drag_travel += std::abs(dx) + std::abs(dy);
            if (dx != 0 || dy != 0)
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

void slope::DragEditor::handle(Slide& cs, WindowManager& wm)
{
    if (wm.isOtherOpen(WindowType::DragAndDrop))
        return;

    auto io = ImGui::GetIO();
    bool ctrl  = ImGui::IsKeyDown(ImGuiKey_LeftCtrl);
    bool shift = ImGui::IsKeyDown(ImGuiKey_LeftShift);
    bool click = io.MouseClicked[0];

    if (ctrl && shift && click && selected_primitive == nullptr) {
        auto S = ImGui::GetWindowSize();
        auto x = double(io.MousePos.x) / S.x;
        auto y = double(io.MousePos.y) / S.y;
        auto prim = getPrimitiveUnderMouse(cs, x, y);
        if (prim != nullptr)
            toggleMember(cs, wm, prim);
        else
            clearGroup(cs, wm);
        return;
    }
    if (!group.empty() && selected_primitive == nullptr) {
        handleGroup(cs, ctrl, shift, wm);
        return;
    }

    if (ctrl && !shift && click && selected_primitive == nullptr) {
        auto S = ImGui::GetWindowSize();
        auto x = double(io.MousePos.x) / S.x;
        auto y = double(io.MousePos.y) / S.y;
        selected_primitive = getPrimitiveUnderMouse(cs, x, y);
        if (selected_primitive != nullptr) {
            wm.Toggle(WindowType::DragAndDrop);
            auto& pis = cs[selected_primitive];
            LabelAnchorPtr lab = std::dynamic_pointer_cast<LabelAnchor>(pis.anchor);
            if (lab != nullptr) {
                x_offset     = lab->getPos()(0) - x;
                y_offset     = lab->getPos()(1) - y;
                original_alpha = pis.alpha;
                time_at_pick = Time::now();
            }
        }
    }

    if (!ctrl && click && selected_primitive != nullptr) {
        cs[selected_primitive].alpha = original_alpha;
        selected_primitive = nullptr;
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

    constexpr scalar zoom = 1.1;
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

    lab->writePosAtLabel(cx, cy, true);
    pis.alpha = (std::cos(TimeFrom(time_at_pick) * 5) + 1) * 0.8 + 0.2;
}
