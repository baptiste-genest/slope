#include "content/screen_primitives/shapes/Shape2D.h"
#include "content/screen_primitives/layout/Anchor.h"
#include "content/screen_primitives/layout/Canvas.h"
#include "math/kernels.h"
#include "polyscope/view.h"

namespace slope {

static ImU32 withAlpha(const RGBA& c, float alpha) {
    ImVec4 v = c.Value;
    v.w *= alpha;
    return ImGui::ColorConvertFloat4ToU32(v);
}

static ImU32 withAlpha(const Color& c, float alpha) {
    return withAlpha(c.getImColor(), alpha);
}

static float pixelThickness(float thickness) {
    return thickness * float(Canvas::pixelScale());
}

// draws the arc-length prefix [0,t] of a pixel-space polyline and returns
// the tip position and direction (for arrowheads)
static std::pair<ImVec2, ImVec2> strokePolylinePrefix(
    const std::vector<ImVec2>& px, parameter t, ImU32 col, float th, bool closed) {
    auto* dl = ImGui::GetWindowDrawList();
    if (px.size() < 2)
        return {};

    if (t >= 1) {
        // the closed flag adds the closing segment, a duplicated first point
        // would degenerate the join at the starting corner
        int n = px.size();
        if (closed && n > 2 && px.front().x == px.back().x && px.front().y == px.back().y)
            n--;
        if (th > 0)
            dl->AddPolyline(px.data(), n, col,
                            closed ? ImDrawFlags_Closed : ImDrawFlags_None, th);
        ImVec2 tip = px.back();
        ImVec2 prev = px[px.size() - 2];
        return {tip, ImVec2(tip.x - prev.x, tip.y - prev.y)};
    }

    scalar total = 0;
    std::vector<scalar> cum(px.size(), 0);
    for (size_t i = 1; i < px.size(); i++) {
        total += std::hypot(px[i].x - px[i - 1].x, px[i].y - px[i - 1].y);
        cum[i] = total;
    }
    scalar target = t * total;

    std::vector<ImVec2> prefix;
    prefix.push_back(px[0]);
    for (size_t i = 1; i < px.size(); i++) {
        if (cum[i] < target) {
            prefix.push_back(px[i]);
            continue;
        }
        scalar seg = cum[i] - cum[i - 1];
        scalar u = seg > 0 ? (target - cum[i - 1]) / seg : 0;
        prefix.push_back(ImVec2(px[i - 1].x + u * (px[i].x - px[i - 1].x),
                                px[i - 1].y + u * (px[i].y - px[i - 1].y)));
        break;
    }
    if (prefix.size() >= 2 && th > 0)
        dl->AddPolyline(prefix.data(), prefix.size(), col, ImDrawFlags_None, th);

    ImVec2 tip = prefix.back();
    ImVec2 prev = prefix.size() >= 2 ? prefix[prefix.size() - 2] : tip;
    return {tip, ImVec2(tip.x - prev.x, tip.y - prev.y)};
}

// ---------------------------------------------------------------- restyles

static void merge(StyleChange& into, const StyleChange& c) {
    if (c.color) into.color = c.color;
    if (c.fill_color) into.fill_color = c.fill_color;
    if (c.thickness) into.thickness = c.thickness;
    if (c.filled) into.filled = c.filled;
}

ShapeStyle StyleTrack::on(const ShapeStyle& base, int slide) const {
    ShapeStyle s = base;
    for (auto it = changes.begin(); it != changes.end() && it->first <= slide; ++it) {
        const StyleChange& c = it->second;
        if (c.color) s.color = *c.color;
        if (c.fill_color) s.fill_color = *c.fill_color;
        if (c.thickness) s.thickness = *c.thickness;
        if (c.filled) s.filled = *c.filled;
    }
    return s;
}

ShapeStyle StyleTrack::at(const ShapeStyle& base, parameter slide_position) const {
    if (changes.empty())
        return base;
    const int i = int(std::floor(slide_position));
    const float f = float(smoothstep(std::clamp<parameter>(slide_position - i, 0, 1)));
    const ShapeStyle a = on(base, i), b = on(base, i + 1);
    // the ends are returned as they are, so a named color stays tunable
    if (f <= 0.f) return a;
    if (f >= 1.f) return b;
    auto mix = [f](const ColorType& x, const ColorType& y) { return Color(x + (y - x) * f); };
    // an unfilled end is a transparent fill, so the fill fades in or out
    auto fill = [](const ShapeStyle& x) {
        ColorType c = x.fill_color.getValue();
        if (!x.filled) c.w = 0;
        return c;
    };
    ShapeStyle s;
    s.color = mix(a.color.getValue(), b.color.getValue());
    s.fill_color = mix(fill(a), fill(b));
    s.thickness = std::lerp(a.thickness, b.thickness, f);
    s.filled = a.filled || b.filled;
    return s;
}

template <class T>
static SlideCue restyleCue(PrimitiveID id, const StyleChange& change) {
    return {[id, change](int slide) { merge(Primitive::get<T>(id)->restyles.changes[slide], change); }};
}

SlideCue Shape2D::restyle(const StyleChange& change) { return restyleCue<Shape2D>(pid, change); }
SlideCue Box2D::restyle(const StyleChange& change) { return restyleCue<Box2D>(pid, change); }
SlideCue Arrow2D::restyle(const StyleChange& change) { return restyleCue<Arrow2D>(pid, change); }

void ClearAllRestyles() {
    for (const auto& p : Primitive::primitives) {
        if (auto s = std::dynamic_pointer_cast<Shape2D>(p))
            s->restyles.changes.clear();
        else if (auto b = std::dynamic_pointer_cast<Box2D>(p))
            b->restyles.changes.clear();
        else if (auto a = std::dynamic_pointer_cast<Arrow2D>(p))
            a->restyles.changes.clear();
    }
}

// ---------------------------------------------------------------- Shape2D

Shape2DPtr Shape2D::Add(const std::vector<vec2>& pts, bool closed) {
    auto s = NewPrimitive<Shape2D>();
    // recenter so the shape is positioned by its anchor
    vec2 lo = pts[0], hi = pts[0];
    for (const auto& p : pts) {
        lo = lo.cwiseMin(p);
        hi = hi.cwiseMax(p);
    }
    vec2 c = (lo + hi) * 0.5;
    for (const auto& p : pts)
        s->points.push_back(p - c);
    s->closed = closed;
    s->updateAnchor(c);
    return s;
}

Shape2DPtr Shape2D::Line(const vec2& a, const vec2& b) {
    return Add({a, b});
}

Shape2DPtr Shape2D::Bezier(const vec2& a, const vec2& control, const vec2& b, int N) {
    std::vector<vec2> pts(N);
    for (int i = 0; i < N; i++) {
        scalar u = scalar(i) / (N - 1);
        pts[i] = (1 - u) * (1 - u) * a + 2 * (1 - u) * u * control + u * u * b;
    }
    return Add(pts);
}

Shape2DPtr Shape2D::Circle(const vec2& center, scalar radius, int N) {
    std::vector<vec2> pts(N);
    for (int i = 0; i < N; i++) {
        scalar th = 2 * M_PI * i / N;
        pts[i] = center + radius * vec2(cos(th), sin(th));
    }
    return Add(pts, true);
}

Shape2DPtr Shape2D::Rect(const vec2& center, const vec2& size) {
    vec2 h = size * 0.5;
    return Add({center + vec2(-h(0), -h(1)), center + vec2(h(0), -h(1)),
                center + vec2(h(0), h(1)), center + vec2(-h(0), h(1))},
               true);
}

std::vector<ImVec2> Shape2D::toPixels(const StateInSlide& sis) const {
    vec2 pos = sis.getPosition();
    scalar s = sis.getScale();
    std::vector<ImVec2> px;
    px.reserve(points.size() + 1);
    for (const auto& p : points)
        px.push_back(Canvas::toScreen(pos + s * p));
    if (closed && !px.empty())
        px.push_back(px.front());
    return px;
}

vec2 Shape2D::getSize() const {
    if (points.empty())
        return vec2(0, 0);
    vec2 lo = points[0], hi = points[0];
    for (const auto& p : points) {
        lo = lo.cwiseMin(p);
        hi = hi.cwiseMax(p);
    }
    return vec2((hi(0) - lo(0)) * Options::ScreenResolutionWidth,
                (hi(1) - lo(1)) * Options::ScreenResolutionHeight);
}

void Shape2D::draw(const TimeObject& t, const StateInSlide& sis) {
    const ShapeStyle style = restyles.at(this->style, t.slidePosition());
    auto px = toPixels(sis);
    if (style.filled && px.size() >= 3)
        ImGui::GetWindowDrawList()->AddConvexPolyFilled(
            px.data(), px.size(), withAlpha(style.fill_color, sis.getAlpha()));
    strokePolylinePrefix(px, 1, withAlpha(style.color, sis.getAlpha()),
                         pixelThickness(style.thickness), closed);
}

void Shape2D::playIntro(const TimeObject& t, const StateInSlide& sis) {
    const ShapeStyle style = restyles.at(this->style, t.slidePosition());
    parameter u = t.transition_parameter;
    auto px = toPixels(sis);
    if (style.filled && u >= 1 && px.size() >= 3)
        ImGui::GetWindowDrawList()->AddConvexPolyFilled(
            px.data(), px.size(), withAlpha(style.fill_color, sis.getAlpha()));
    strokePolylinePrefix(px, u, withAlpha(style.color, sis.getAlpha()),
                         pixelThickness(style.thickness), false);
}

void Shape2D::playOutro(const TimeObject& t, const StateInSlide& sis) {
    draw(t, sis);
}

// ------------------------------------------------------------------ Box2D

Box2DPtr Box2D::Add(const std::vector<ScreenPrimitivePtr>& targets) {
    auto b = NewPrimitive<Box2D>();
    b->setTargets(targets);
    return b;
}

void Box2D::setTargets(const std::vector<ScreenPrimitivePtr>& t) {
    targets = t;
}

bool Box2D::bounds(vec2& lo, vec2& hi) const {
    bool first = true;
    for (const auto& target : targets) {
        if (!target)
            continue;
        vec2 tlo, thi;
        target->getBoundingBox(tlo, thi);
        if (first) {
            lo = tlo;
            hi = thi;
            first = false;
        } else {
            lo = lo.cwiseMin(tlo);
            hi = hi.cwiseMax(thi);
        }
    }
    if (first)
        return false;
    lo(0) -= pad_left.value_or(padding(0));
    lo(1) -= pad_top.value_or(padding(1));
    hi(0) += pad_right.value_or(padding(0));
    hi(1) += pad_bot.value_or(padding(1));
    return true;
}

void Box2D::drawBox(const TimeObject& time, parameter t, float alpha) {
    vec2 lo, hi;
    if (!bounds(lo, hi))
        return;
    ShapeStyle base = this->style;
    if (use_background_fill)
        base.fill_color = Color(polyscope::view::bgColor[0], polyscope::view::bgColor[1],
                                polyscope::view::bgColor[2], 1.f);
    const ShapeStyle style = restyles.at(base, time.slidePosition());
    updateAnchor((lo + hi) * 0.5); // so arrows can attach to the box
    std::vector<ImVec2> px = {
        Canvas::toScreen(lo), Canvas::toScreen(vec2(hi(0), lo(1))),
        Canvas::toScreen(hi), Canvas::toScreen(vec2(lo(0), hi(1))),
        Canvas::toScreen(lo)};
    if (style.filled) {
        float fill_alpha = alpha * std::min<float>(1.f, t); // fades in with the intro
        ImGui::GetWindowDrawList()->AddConvexPolyFilled(px.data(), 4,
                                                        withAlpha(style.fill_color, fill_alpha));
    }
    strokePolylinePrefix(px, t, withAlpha(style.color, alpha),
                         pixelThickness(style.thickness), t >= 1);
}

void Box2D::getBoundingBox(vec2& lo, vec2& hi) const {
    if (!bounds(lo, hi))
        ScreenPrimitive::getBoundingBox(lo, hi);
}

vec2 Box2D::getSize() const {
    vec2 lo, hi;
    if (!bounds(lo, hi))
        return vec2(0, 0);
    return vec2((hi(0) - lo(0)) * Options::ScreenResolutionWidth,
                (hi(1) - lo(1)) * Options::ScreenResolutionHeight);
}

void Box2D::draw(const TimeObject& t, const StateInSlide& sis) {
    drawBox(t, 1, sis.getAlpha());
}

void Box2D::playIntro(const TimeObject& t, const StateInSlide& sis) {
    drawBox(t, t.transition_parameter, sis.getAlpha());
}

void Box2D::playOutro(const TimeObject& t, const StateInSlide& sis) {
    drawBox(t, 1, sis.getAlpha());
}

// ---------------------------------------------------------------- Arrow2D

vec2 Arrow2D::Endpoint::center() const {
    if (prim) {
        vec2 lo, hi;
        prim->getBoundingBox(lo, hi);
        return (lo + hi) * 0.5;
    }
    if (anchor)
        return anchor->getPos();
    if (follow)
        return follow();
    return fixed;
}

Arrow2DPtr Arrow2D::Add(const vec2& a, const vec2& b) {
    Endpoint ea;
    ea.fixed = a;
    Endpoint eb;
    eb.fixed = b;
    return Add(ea, eb);
}

Arrow2DPtr Arrow2D::Add(const Endpoint& a, const Endpoint& b) {
    auto arrow = NewPrimitive<Arrow2D>();
    arrow->from = a;
    arrow->to = b;
    return arrow;
}

Arrow2D::Endpoint Arrow2D::Attach(ScreenPrimitivePtr p) {
    Endpoint e;
    e.prim = p;
    return e;
}

Arrow2D::Endpoint Arrow2D::AttachLabel(const std::string& label) {
    Endpoint e;
    e.anchor = LabelAnchor::Add(label);
    return e;
}

vec2 Arrow2D::attachPoint(const Endpoint& e, const vec2& other, scalar margin) {
    vec2 c = e.center();
    vec2 d = other - c;
    if (!e.prim || d.norm() < 1e-9)
        return c;
    vec2 lo, hi;
    e.prim->getBoundingBox(lo, hi);
    vec2 h = (hi - lo) * 0.5 + vec2(margin, margin);
    scalar t = 1;
    if (std::abs(d(0)) > 1e-9) t = std::min(t, h(0) / std::abs(d(0)));
    if (std::abs(d(1)) > 1e-9) t = std::min(t, h(1) / std::abs(d(1)));
    return c + d * t;
}

vec2 Arrow2D::controlPoint(const vec2& a, const vec2& b) const {
    vec2 mid = (a + b) * 0.5;
    if (bend == 0)
        return mid;
    const vec2 aspect(Options::ScreenResolutionWidth, Options::ScreenResolutionHeight);
    vec2 d_px = (b - a).cwiseProduct(aspect);
    if (d_px.norm() < 1e-9)
        return mid;
    vec2 perp_px = vec2(-d_px(1), d_px(0)).normalized() * (bend * d_px.norm());
    return mid + perp_px.cwiseQuotient(aspect);
}

void Arrow2D::drawArrow(const TimeObject& time, parameter t, float alpha) const {
    const ShapeStyle style = restyles.at(this->style, time.slidePosition());
    vec2 ca = from.center(), cb = to.center();
    vec2 a = attachPoint(from, cb, margin) + from.offset;
    vec2 b = attachPoint(to, ca, margin) + to.offset;

    if ((b - a).norm() < 1e-9)
        return;
    vec2 control = controlPoint(a, b);

    auto W = Canvas::size();
    constexpr int N = 48;
    std::vector<ImVec2> px(N);
    for (int i = 0; i < N; i++) {
        scalar u = scalar(i) / (N - 1);
        vec2 p = (1 - u) * (1 - u) * a + 2 * (1 - u) * u * control + u * u * b;
        px[i] = Canvas::toScreen(p);
    }

    ImU32 col = withAlpha(style.color, alpha);
    float th = pixelThickness(style.thickness);
    if (head <= 0) {
        strokePolylinePrefix(px, t, col, th, false);
        return;
    }

    auto [tip, dir] = strokePolylinePrefix(px, t, col, 0, false);
    scalar total = 0;
    for (int i = 1; i < N; i++)
        total += std::hypot(px[i].x - px[i - 1].x, px[i].y - px[i - 1].y);
    float hs = std::min<float>(std::max<float>(head * W.y, 2 * th), total);

    parameter body = total > 0 ? t - 0.8 * hs / total : 0;
    if (body > 0) {
        ImVec2 end = strokePolylinePrefix(px, body, col, th, false).first;
        if (std::hypot(tip.x - end.x, tip.y - end.y) > 1e-6f)
            dir = ImVec2(tip.x - end.x, tip.y - end.y);
    }
    float len = std::hypot(dir.x, dir.y);
    if (len < 1e-6f)
        return;
    ImVec2 u = ImVec2(dir.x / len, dir.y / len);
    ImVec2 n = ImVec2(-u.y, u.x);
    ImVec2 base = ImVec2(tip.x - hs * u.x, tip.y - hs * u.y);
    ImGui::GetWindowDrawList()->AddTriangleFilled(
        ImVec2(tip.x + 0.35f * hs * u.x, tip.y + 0.35f * hs * u.y),
        ImVec2(base.x + 0.5f * hs * n.x, base.y + 0.5f * hs * n.y),
        ImVec2(base.x - 0.5f * hs * n.x, base.y - 0.5f * hs * n.y),
        col);
}

void Arrow2D::getBoundingBox(vec2& lo, vec2& hi) const {
    vec2 a = from.center(), b = to.center();
    lo = a.cwiseMin(b);
    hi = a.cwiseMax(b);
    if (bend != 0) {
        // the curve stays in the hull of {a, control, b}
        if ((b - a).norm() > 1e-9) {
            vec2 control = controlPoint(a, b);
            lo = lo.cwiseMin(control);
            hi = hi.cwiseMax(control);
        }
    }
}

vec2 Arrow2D::getSize() const {
    vec2 lo, hi;
    getBoundingBox(lo, hi);
    return vec2((hi(0) - lo(0)) * Options::ScreenResolutionWidth,
                (hi(1) - lo(1)) * Options::ScreenResolutionHeight);
}

void Arrow2D::draw(const TimeObject& t, const StateInSlide& sis) {
    drawArrow(t, 1, sis.getAlpha());
}

void Arrow2D::playIntro(const TimeObject& t, const StateInSlide& sis) {
    drawArrow(t, t.transition_parameter, sis.getAlpha());
}

void Arrow2D::playOutro(const TimeObject& t, const StateInSlide& sis) {
    drawArrow(t, 1, sis.getAlpha());
}

} // namespace slope
