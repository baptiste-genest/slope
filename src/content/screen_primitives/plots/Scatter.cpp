#include "Scatter.h"
#include "content/config/io.h"
#include <algorithm>

namespace slope {

ScatterPtr Scatter::make(const std::string& name, const BoardRef& board)
{
    auto s = NewPrimitive<Scatter>();
    s->name = name;
    s->board = board.name;
    s->settings.owner = name;
    s->default_ink = nextInk(s->board);
    registerLegendEntry(s->board, std::weak_ptr<Legendable>(s));
    return s;
}

ScatterPtr Scatter::Add(const std::string& name, BoardRef board,
                        const std::vector<vec2>& points)
{
    auto s = make(name, board);
    s->points = points;
    return s;
}

ScatterPtr Scatter::Add(const std::string& name, BoardRef board, const path& csv)
{
    auto s = make(name, board);
    s->file = csv;
    return s;
}

ScatterPtr Scatter::Add(const std::string& name, BoardRef board,
                        const std::vector<scalar>& values, const vec2& span)
{
    std::vector<vec2> pts;
    for (std::size_t i = 0; i < values.size(); i++)
        pts.push_back(vec2(span(0) + (span(1) - span(0)) * scalar(i)
                                   / std::max<scalar>(1, scalar(values.size() - 1)),
                           values[i]));
    return Add(name, board, pts);
}

const std::vector<std::string>& Scatter::settingNames()
{
    static const std::vector<std::string> all = {"color", "size", "reveal"};
    return all;
}

BoardPtr Scatter::owner() const
{
    if (auto b = cached.lock()) return b;
    auto b = Board::find(board);
    if (b) cached = b;
    else {
        static std::set<std::string> said;
        if (said.insert(board).second)
            spdlog::error("scatter \"{}\" : no board named \"{}\"", name, board);
    }
    return b;
}

void Scatter::refresh()
{
    if (file.empty()) return;
    std::error_code ec;
    const auto now = std::filesystem::last_write_time(formatPath(file), ec);
    if (read_once && now == stamp) return;
    stamp = now;
    read_once = true;
    points = readCsvPoints(file);
}

vec2 Scatter::getSize() const
{
    auto b = owner();
    return b ? b->getSize() : vec2(1, 1);
}

// Marks are drawn over the board's image. A fragment shader would have to
// search for them, where here their places are known.
void Scatter::paint(const TimeObject& t, const StateInSlide& sis, float appeared)
{
    this->appeared = appeared;
    auto b = owner();
    if (!b) return;
    refresh();
    if (points.empty()) return;

    const ImVec2 W = ImGui::GetWindowSize();
    if (W.x <= 0 || W.y <= 0) return;

    const StateInSlide on = b->frame(sis);
    const RGBA ink = settings.ink("color", default_ink);
    const float r = float(settings.num("size", 5, 1, 20) * on.getScale());
    const ImU32 col = ImGui::GetColorU32(ImVec4(ink.Value.x, ink.Value.y, ink.Value.z,
                                                float(ink.Value.w * on.getAlpha())));
    // whichever is shorter, the setting or its own arrival
    const scalar reveal = std::min(scalar(appeared), settings.num("reveal", 1, 0, 1));
    // in the board's referential, so a log axis carries the marks with it
    const vec2 x = b->xview(), y = b->yview();
    const scalar edge = x(0) + (x(1) - x(0)) * std::clamp(reveal, scalar(0), scalar(1));
    const bool lx = b->xlog(), ly = b->ylog();

    auto* dl = ImGui::GetWindowDrawList();
    for (const vec2& q : points) {
        // a log axis has nowhere to put a point at or under zero
        if ((lx && q(0) <= 0) || (ly && q(1) <= 0)) continue;
        const vec2 p = b->toView(q);
        if (p(0) < x(0) || p(0) > x(1) || p(1) < y(0) || p(1) > y(1) || p(0) > edge)
            continue;
        const vec2 s = b->worldToScreen(p);
        dl->AddCircleFilled(ImVec2(float(s(0)) * W.x, float(s(1)) * W.y), r, col);
    }
}

void Scatter::draw(const TimeObject& t, const StateInSlide& sis)
{ last_frame = t.absolute_frame_number; paint(t, sis, 1); }

void Scatter::playIntro(const TimeObject& t, const StateInSlide& sis)
{ last_frame = t.absolute_frame_number; paint(t, sis, float(t.transition_parameter)); }

void Scatter::playOutro(const TimeObject& t, const StateInSlide& sis)
{ last_frame = t.absolute_frame_number; paint(t, sis, float(1 - t.transition_parameter)); }

// three marks along the swatch, at the size they are drawn on the board
void Scatter::legendSwatch(ImDrawList* dl, const ImVec2& a, const ImVec2& b,
                           scalar scale, scalar alpha) const
{
    const RGBA ink = settings.ink("color", default_ink);
    const float r = float(settings.num("size", 5, 1, 20) * scale);
    const ImU32 col = ImGui::GetColorU32(ImVec4(ink.Value.x, ink.Value.y, ink.Value.z,
                                                float(ink.Value.w * alpha)));
    for (int i = 0; i < 3; i++)
        dl->AddCircleFilled(ImVec2(a.x + (b.x - a.x) * (0.5f * i), 0.5f * (a.y + b.y)), r, col);
}

}
