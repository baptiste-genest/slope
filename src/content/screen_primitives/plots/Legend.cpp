#include "Legend.h"
#include "polyscope/view.h"
#include <algorithm>

namespace slope {

namespace {

static const std::vector<std::string> kCorners = {"top_right", "top_left",
                                                  "bottom_right", "bottom_left"};

// A caption is a fixed string, so it is compiled once and shared.
LatexPtr captionLatex(const std::string& body) {
    static std::map<std::string, LatexPtr> made;
    auto it = made.find(body);
    if (it == made.end())
        it = made.emplace(body, Latex::Add(body, 1)).first;
    return it->second;
}

} // namespace

const std::vector<std::string>& Legend::settingNames() {
    static const std::vector<std::string> all = {
        "corner",
        "text_size",
        "swatch",
        "padding",
        "background",
        "border",
        "text",
    };
    return all;
}

LegendPtr Legend::Add(BoardRef board) {
    auto l = NewPrimitive<Legend>();
    l->name = nameFor(board.name);
    l->board = board.name;
    l->settings.owner = l->name;
    return l;
}

BoardPtr Legend::owner() const {
    if (auto b = cached.lock()) return b;
    auto b = Board::find(board);
    if (b)
        cached = b;
    else {
        static std::set<std::string> said;
        if (said.insert(board).second)
            spdlog::error("legend : no board named \"{}\"", board);
    }
    return b;
}

vec2 Legend::getSize() const {
    auto b = owner();
    return b ? b->getSize() : vec2(1, 1);
}

// Over the board's image, like a scatter.
void Legend::paint(const TimeObject& t, const StateInSlide& sis, float appeared) {
    auto b = owner();
    if (!b) return;

    const ImVec2 W = ImGui::GetWindowSize();
    if (W.x <= 0 || W.y <= 0) return;

    // only what this slide is showing, in declaration order
    std::vector<std::shared_ptr<Legendable>> rows;
    for (const auto& e : legendEntries(board))
        if (e->legendDrawnAt(t.absolute_frame_number))
            rows.push_back(e);
    if (rows.empty()) return;

    const StateInSlide on = b->frame(sis);
    const scalar sc = on.getScale();
    const scalar alpha = on.getAlpha() * std::clamp(scalar(appeared), scalar(0), scalar(1));
    const float pad = float(settings.num("padding", 12, 0, 40) * sc);
    const float swat = float(settings.num("swatch", 46, 10, 140) * sc);
    const scalar size = settings.num("text_size", 0.4, 0.1, 1.5);

    // the box is as wide as the caption that needs the most
    std::vector<LatexPtr> texs;
    std::vector<vec2> sizes;
    float text_w = 0, height = pad;
    for (const auto& e : rows) {
        LatexPtr tex = captionLatex(e->legendCaption());
        tex->scale = size;
        const vec2 wh = tex->getSize() * sc;
        texs.push_back(tex);
        sizes.push_back(wh);
        text_w = std::max(text_w, float(wh(0)));
        height += float(wh(1)) + pad;
    }
    const float width = pad + swat + pad + text_w + pad;

    // the rectangle it sits in a corner of, in the referential it is drawn in
    const vec2 x = b->xview(), y = b->yview();
    const vec2 lo = b->worldToScreen(vec2(x(0), y(0)));
    const vec2 hi = b->worldToScreen(vec2(x(1), y(1)));
    const float x0 = float(std::min(lo(0), hi(0))) * W.x, x1 = float(std::max(lo(0), hi(0))) * W.x;
    const float y0 = float(std::min(lo(1), hi(1))) * W.y, y1 = float(std::max(lo(1), hi(1))) * W.y;

    // not "at", which is the deck's placement key
    const std::string at = settings.choice("corner", kCorners, "top_right");
    const bool right = at == "top_right" || at == "bottom_right";
    const bool bottom = at == "bottom_left" || at == "bottom_right";
    const ImVec2 bmin(right ? x1 - pad - width : x0 + pad,
                      bottom ? y1 - pad - height : y0 + pad);
    const ImVec2 bmax(bmin.x + width, bmin.y + height);

    auto ink = [&](const RGBA& c, scalar a) {
        return ImGui::GetColorU32(ImVec4(c.Value.x, c.Value.y, c.Value.z,
                                         float(c.Value.w * a)));
    };
    auto* dl = ImGui::GetWindowDrawList();

    const auto& bg = polyscope::view::bgColor;
    const RGBA back = settings.ink("background", RGBA(bg[0], bg[1], bg[2], 0.85f));
    const RGBA edge = settings.ink("border", RGBA(0.35f, 0.38f, 0.45f, 0.5f));
    dl->AddRectFilled(bmin, bmax, ink(back, alpha), 4.f * float(sc));
    dl->AddRect(bmin, bmax, ink(edge, alpha), 4.f * float(sc), 0, float(sc));

    const RGBA text_ink = settings.ink("text", RGBA(0.15f, 0.16f, 0.20f, 1.f));
    if (anchors.size() < rows.size())
        anchors.resize(rows.size());

    float cy = bmin.y + pad;
    for (std::size_t i = 0; i < rows.size(); i++) {
        // as far in as what it names, so the box arrives with the curves
        const scalar a = alpha * std::clamp(rows[i]->legendAlpha(), scalar(0), scalar(1));
        const float mid = cy + float(sizes[i](1)) * 0.5f;
        rows[i]->legendSwatch(dl, ImVec2(bmin.x + pad, mid),
                              ImVec2(bmin.x + pad + swat, mid), sc, a);

        if (!anchors[i]) anchors[i] = AbsoluteAnchor::Add(vec2(-1, -1));
        const ImVec2 c(bmin.x + pad + swat + pad + float(sizes[i](0)) * 0.5f, mid);
        anchors[i]->updatePos(vec2(c.x / W.x, c.y / W.y));
        texs[i]->setColor(Color(text_ink.Value.x, text_ink.Value.y, text_ink.Value.z));
        StateInSlide s;
        s.anchor = anchors[i];
        s.scale = sc;
        s.alpha = a;
        texs[i]->draw(t, s);

        cy += float(sizes[i](1)) + pad;
    }
}

void Legend::draw(const TimeObject& t, const StateInSlide& sis) { paint(t, sis, 1); }

void Legend::playIntro(const TimeObject& t, const StateInSlide& sis) { paint(t, sis, float(t.transition_parameter)); }

void Legend::playOutro(const TimeObject& t, const StateInSlide& sis) { paint(t, sis, float(1 - t.transition_parameter)); }

} // namespace slope
