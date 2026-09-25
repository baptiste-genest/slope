#include "Plot.h"
#include "content/config/io.h"
#include <algorithm>
#include <fstream>

namespace slope {

// Ink only where the line is, so it composites over whatever is under it.
static const char* kPlotSource = R"(
#include <plot.glsl>
uniform sampler2D samples;
uniform vec2  span;
uniform vec4  color;
uniform float line_width, reveal;

void main() {
    vec2 px = iPixelXY(), p = iWorld();
    float a = color.a
            * stroke(sdGraph(p, dataAt(samples, span, p.x),
                                dataSlope(samples, span, p.x), px), line_width)
            * inSpan(p.x, span, px.x)
            * revealMask(p.x, span, reveal, px.x);
    fragColor = vec4(color.rgb, a);
}
)";

namespace {

// linear interpolation of a sorted point set, clamped at both ends
Plot::Fn interpolator(std::vector<vec2> pts)
{
    std::sort(pts.begin(), pts.end(), [](const vec2& a, const vec2& b){ return a(0) < b(0); });
    return [pts](scalar x) -> scalar {
        if (pts.empty()) return 0;
        if (x <= pts.front()(0)) return pts.front()(1);
        if (x >= pts.back()(0))  return pts.back()(1);
        const auto it = std::lower_bound(pts.begin(), pts.end(), x,
                                         [](const vec2& a, scalar v){ return a(0) < v; });
        const vec2 b = *it, a = *(it - 1);
        const scalar d = b(0) - a(0);
        return d > 0 ? a(1) + (b(1) - a(1)) * (x - a(0)) / d : a(1);
    };
}

vec2 bounds(const std::vector<vec2>& pts)
{
    if (pts.empty()) return vec2(0, 1);
    const auto [lo, hi] = std::minmax_element(pts.begin(), pts.end(),
                              [](const vec2& a, const vec2& b){ return a(0) < b(0); });
    return (*hi)(0) > (*lo)(0) ? vec2((*lo)(0), (*hi)(0)) : vec2((*lo)(0), (*lo)(0) + 1);
}

} // namespace

const std::vector<std::string>& Plot::settingNames()
{
    static const std::vector<std::string> all = {"color", "width", "reveal"};
    return all;
}

BoardPtr Plot::owner() const
{
    if (auto p = cached.lock()) return p;
    auto p = Board::find(board);
    if (p) cached = p;
    else {
        static std::set<std::string> said;
        if (said.insert(board).second)
            spdlog::error("plot \"{}\" : no board named \"{}\"", name, board);
    }
    return p;
}

PlotPtr Plot::make(const std::string& name, const BoardRef& board)
{
    auto c = NewPrimitive<Plot>();
    c->name = name;
    c->board = board.name;
    if (const path f = shaderFileFor(name, kPlotSource); !f.empty())
        c->setFragmentFile(f);
    else
        c->setFragmentSource(kPlotSource);
    c->registerLive();
    c->settings.owner = name;

    c->default_ink = nextInk(c->board);

    // the board's referential, read live, so a moving view carries the plot
    Plot* self = c.get();
    c->bindViewRect([self] {
        auto p = self->owner();
        const vec2 x = p ? p->xview() : vec2(-1, 1), y = p ? p->yview() : vec2(-1, 1);
        return std::make_pair(vec2(x(0), y(0)), vec2(x(1), y(1)));
    });
    c->bind("color", [self] { return self->settings.ink("color", self->default_ink); });
    c->bind("line_width", [self] { return self->settings.num("width", 3, 0, 12); });
    // whichever is shorter, the setting or its own arrival
    c->bind("reveal", [self] {
        return std::min(scalar(self->appeared), self->settings.num("reveal", 1, 0, 1));
    });
    c->bind("span", [self] { return self->span; });
    registerLegendEntry(c->board, std::weak_ptr<Legendable>(c));
    return c;
}

PlotPtr Plot::Add(const std::string& name, BoardRef board, const Fn& f)
{
    auto c = make(name, board);
    c->f = f;
    return c;
}

PlotPtr Plot::Add(const std::string& name, BoardRef board,
                    const std::vector<vec2>& points)
{
    auto c = make(name, board);
    c->raw = points;
    c->span = c->data_span = bounds(points);
    c->own_span = true;
    return c;
}

PlotPtr Plot::Add(const std::string& name, BoardRef board,
                    const std::vector<scalar>& values, const vec2& span)
{
    std::vector<vec2> pts;
    for (std::size_t i = 0; i < values.size(); i++)
        pts.push_back(vec2(span(0) + (span(1) - span(0)) * scalar(i)
                                   / std::max<scalar>(1, scalar(values.size() - 1)),
                           values[i]));
    return Add(name, board.name, pts);
}

PlotPtr Plot::Add(const std::string& name, BoardRef board, const path& csv)
{
    auto c = make(name, board);
    c->file = csv;
    c->own_span = true;
    return c;
}

PlotPtr Plot::FromSnippet(const std::string& name, BoardRef board,
                            const std::string& section)
{
    auto c = make(name, board);
    c->live = true;
    // resolved on every sampling, so an edit takes effect at once
    c->f = [section](scalar x) { return Snippet::fn<scalar(scalar)>(section)(x); };
    return c;
}

// Just under the rectangle, where a value that a log axis cannot show is sent, so the
// curve leaves the picture and does not stop. Not to its real depth,
// which for an underflowed value is hundreds of decades down and reads, in the
// linearisation the shader draws with, as a line straight through the frame.
static float offScale(const vec2& yview)
{
    return float(yview(0) - 0.25 * (yview(1) - yview(0)));
}

// kSamples values over `span`, uploaded as the one row dataAt() reads. The
// sampling is even in the referential the board draws in, so a log axis takes
// its samples per decade.
void Plot::refresh()
{
    auto p = owner();
    const bool lx = p && p->xlog(), ly = p && p->ylog();

    if (!file.empty()) {
        std::error_code ec;
        const auto now = std::filesystem::last_write_time(formatPath(file), ec);
        if (!sampled || now != stamp) {
            stamp = now;
            raw = readCsvPoints(file);
            data_span = bounds(raw);
            sampled = false;
        }
    }
    // its own interval, or the board's, since a formula has no domain and a
    // zoom must resample it rather than stretch what is there
    const vec2 want = own_span ? (lx ? decadeSpan(data_span) : data_span)
                               : (p ? p->xview() : span);
    // a log axis that moved takes the floor of the samples with it
    const vec2 wanted_y = ly && p ? p->yview() : view_y;
    if (lx != log_x || ly != log_y || (want - span).norm() > 1e-9 * (1 + span.norm())
        || (wanted_y - view_y).norm() > 1e-9 * (1 + view_y.norm())) {
        span = want;
        view_y = wanted_y;
        log_x = lx;
        log_y = ly;
        sampled = false;
    }
    if (sampled && !live) return;

    // Measurements are joined where they are drawn, so two of them are a
    // straight line on the slide whatever the axes do. What a point cannot be
    // shown on is left out of the interpolation rather than clamped.
    if (own_span) {
        std::vector<vec2> in_view;
        in_view.reserve(raw.size());
        for (const vec2& q : raw) {
            if ((log_x && q(0) <= 0) || (log_y && q(1) <= 0)) continue;
            in_view.push_back(vec2(log_x ? std::log10(q(0)) : q(0),
                                   log_y ? std::log10(q(1)) : q(1)));
        }
        f = in_view.empty() ? Fn() : interpolator(in_view);
    }

    const float floor_v = offScale(view_y);
    samples.resize(kSamples);
    for (int i = 0; i < kSamples; i++) {
        const scalar u = span(0) + (span(1) - span(0)) * i / scalar(kSamples - 1);
        float v;
        if (own_span)
            v = f ? float(f(u)) : floor_v;        // already in the referential
        else {
            const scalar y = f ? f(log_x ? std::pow(scalar(10), u) : u) : 0;
            v = log_y ? (y > 0 ? float(std::log10(y)) : floor_v) : float(y);
        }
        samples[std::size_t(i)] = log_y ? std::max(v, floor_v) : v;
    }
    setTexture("samples", samples, kSamples, 1);
    sampled = true;
}

// Drawn on by the transition that brings it in, erased by the one that takes
// it away.
bool Plot::prepare(const StateInSlide& sis, float u, StateInSlide& on)
{
    auto p = owner();
    if (!p) return false;
    if (!sized) {
        setResolution(p->bufferWidth(), p->bufferHeight());
        sized = true;
    }
    appeared = u;
    refresh();
    on = p->frame(sis);
    return true;
}

void Plot::draw(const TimeObject& t, const StateInSlide& sis)
{ last_frame = t.absolute_frame_number;
  StateInSlide on; if (prepare(sis, 1, on)) Shader::draw(t, on); }

void Plot::playIntro(const TimeObject& t, const StateInSlide& sis)
{ last_frame = t.absolute_frame_number;
  StateInSlide on; if (prepare(sis, float(t.transition_parameter), on)) Shader::playIntro(t, on); }

void Plot::playOutro(const TimeObject& t, const StateInSlide& sis)
{ last_frame = t.absolute_frame_number;
  StateInSlide on; if (prepare(sis, float(1 - t.transition_parameter), on)) Shader::playOutro(t, on); }

// A straight run of the line at its own width and ink. An edited program
// (dashes, a halo) is not shown as such, that wants a second render target.
void Plot::legendSwatch(ImDrawList* dl, const ImVec2& a, const ImVec2& b,
                        scalar scale, scalar alpha) const
{
    const RGBA ink = settings.ink("color", default_ink);
    const float w = float(settings.num("width", 3, 0, 12) * scale);
    dl->AddLine(a, b, ImGui::GetColorU32(ImVec4(ink.Value.x, ink.Value.y, ink.Value.z,
                                                float(ink.Value.w * alpha))), w);
}

}
