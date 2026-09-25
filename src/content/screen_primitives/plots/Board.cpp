#include "Board.h"
#include <algorithm>
#include <fstream>
#include <limits>
#include "content/screen_primitives/text/LateX.h"

namespace slope {

// The whole frame in one pass, rectangle, grid, axes and border.
static const char* kFrameSource = R"(
#include <plot.glsl>
uniform vec4  background, grid_color, axis_color;
uniform vec2  grid_step;
uniform vec2  axis_at;       // where x = 0 and y = 0 fall, far off a log axis
uniform float line_width;
uniform vec3  show;          // grid, axes, frame, 1 when drawn

void main() {
    vec2 px = iPixelXY(), p = iWorld();
    vec2 lo = iViewCenter - iViewHalf, hi = iViewCenter + iViewHalf;
    Ink k = inkClear();
    inkOver(k, axis_color.rgb, axis_color.a *
            max(show.y * axesMask(p - axis_at, px, 1.5*line_width),
                show.z * frameMask(p, lo, hi, px, line_width)));
    inkOver(k, grid_color.rgb, grid_color.a * show.x * gridMask(p, grid_step, px, 0.7*line_width));
    inkOver(k, background.rgb, background.a);
    fragColor = inkResolve(k);
}
)";

// the furniture colours, read through the namespace
static const std::vector<std::string> kXSides = {"bottom", "top", "none"};
static const std::vector<std::string> kYSides = {"left", "right", "none"};
static const std::vector<std::string> kFonts  = {"latex", "text"};
static const std::vector<std::string> kScales = {"linear", "log"};

static const RGBA kGridInk(0.45f, 0.48f, 0.55f, 0.28f);
static const RGBA kAxisInk(0.35f, 0.38f, 0.45f, 0.85f);

// two columns of numbers. A line holding no pair of them is skipped.
path shaderFileFor(const std::string& name, const std::string& fallback)
{
    if (Options::ProjectViewsPath.empty())
        return {};
    std::error_code ec;
    std::filesystem::create_directories(Options::ProjectViewsPath, ec);
    const path file = std::filesystem::absolute(Options::ProjectViewsPath + name + ".glsl");
    if (std::filesystem::exists(file, ec))
        return file;
    std::ofstream out(file);
    if (!out) {
        spdlog::error("[board] could not write \"{}\"", file.string());
        return {};
    }
    // the program alone, without the newline its raw literal opens on
    out << fallback.substr(fallback.find_first_not_of("\n"));
    spdlog::info("[board] wrote {}", file.string());
    return file;
}

// what a legend lists
// by board name, so a plot declared before its frame still lands in the box
static std::map<std::string, std::vector<std::weak_ptr<Legendable>>>& legendLists()
{
    static std::map<std::string, std::vector<std::weak_ptr<Legendable>>> l;
    return l;
}

void registerLegendEntry(const std::string& board, const std::weak_ptr<Legendable>& e)
{
    legendLists()[board].push_back(e);
}

std::vector<std::shared_ptr<Legendable>> legendEntries(const std::string& board)
{
    auto& list = legendLists()[board];
    std::vector<std::shared_ptr<Legendable>> out;
    // the walk prunes what has gone
    auto it = list.begin();
    while (it != list.end()) {
        if (auto e = it->lock()) { out.push_back(e); ++it; }
        else it = list.erase(it);
    }
    return out;
}

RGBA nextInk(const std::string& board)
{
    static const std::vector<RGBA> palette = {
        RGBA(0.35f, 0.62f, 0.98f, 1.f), RGBA(0.98f, 0.55f, 0.30f, 1.f),
        RGBA(0.38f, 0.80f, 0.52f, 1.f), RGBA(0.88f, 0.45f, 0.72f, 1.f),
        RGBA(0.95f, 0.82f, 0.32f, 1.f), RGBA(0.60f, 0.55f, 0.92f, 1.f),
    };
    static std::map<std::string, std::size_t> handed_out;
    return palette[handed_out[board]++ % palette.size()];
}

std::vector<vec2> readCsvPoints(const path& file)
{
    std::vector<vec2> out;
    std::ifstream in(formatPath(file));
    if (!in)
        throw std::runtime_error("plot : cannot read \"" + formatPath(file) + "\"");
    std::string line;
    while (std::getline(in, line)) {
        for (char& c : line)
            if (c == ',' || c == ';' || c == '\t') c = ' ';
        std::istringstream ss(line);
        scalar a, b;
        if (ss >> a >> b) out.push_back(vec2(a, b));
    }
    return out;
}

namespace {

// 1, 2 or 5 times a power of ten, the spacings that read as round numbers
scalar niceStep(scalar raw)
{
    if (!(raw > 0)) return 1;
    const scalar mag = std::pow(scalar(10), std::floor(std::log10(raw)));
    const scalar f = raw / mag;
    return (f <= 1 ? 1 : f <= 2 ? 2 : f <= 5 ? 5 : 10) * mag;
}

// the fewest decimals that keep two consecutive ticks apart
int decimals(scalar step)
{
    if (!(step > 0)) return 0;
    for (int d = 0; d < 4; d++) {
        const scalar q = std::pow(scalar(10), d);
        if (std::abs(step * q - std::round(step * q)) < 1e-2 * step * q)
            return d;
    }
    return 4;
}

std::string decimalText(scalar v, scalar step)
{
    if (std::abs(v) < step * 1e-6) v = 0;
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.*f", decimals(step), double(v));
    return buf;
}

// what one axis will ever label, however small a step is asked for
constexpr int kMaxTicks = 60;

// the multiples of `step` inside [lo, hi]
std::vector<scalar> tickValues(scalar lo, scalar hi, scalar step)
{
    std::vector<scalar> out;
    // walking backwards would never reach the far end
    if (!(step > 0) || !(hi > lo))
        return out;
    for (long i = long(std::ceil(lo / step));
         i * step <= hi + step * 1e-6 && int(out.size()) < kMaxTicks; i++)
        out.push_back(i * step);
    return out;
}

// A log axis is positive. A range that reaches zero is raised to six decades under its top,
// and one that is not a range at all is opened to one decade.
vec2 positiveRange(vec2 r)
{
    if (!(r(1) > 0)) r(1) = 1;
    if (!(r(0) > 0)) r(0) = r(1) * scalar(1e-6);
    if (!(r(1) > r(0))) r(1) = r(0) * 10;
    return r;
}

// The decades of [lo, hi], given as log10. Over a short range the halves of a
// decade are labelled too, so a range inside one still reads.
std::vector<scalar> decadeValues(scalar lo, scalar hi)
{
    std::vector<scalar> out;
    if (!(hi > lo)) return out;
    // 1, 2 and 5 while there is room for them, only powers of ten past that
    static const scalar mantissas[] = {1, 2, 5};
    const int many = (hi - lo) < 3 ? 3 : 1;
    for (long e = long(std::floor(lo)); scalar(e) <= hi && int(out.size()) < kMaxTicks; e++)
        for (int m = 0; m < many; m++) {
            const scalar v = scalar(e) + std::log10(mantissas[m]);
            if (v >= lo - 1e-9 && v <= hi + 1e-9) out.push_back(v);
        }
    return out;
}

// One axis uses a single notation. It uses plain numbers while all of them are short,
// and powers of ten as soon as a single label needs them.
bool wantsPowers(const std::vector<scalar>& decades)
{
    for (scalar v : decades) {
        const scalar e = std::floor(v + 1e-9);
        if (e < -3 || e > 3) return true;
    }
    return false;
}

// A decade as a reader writes it : 0.01, 2, 500, or 10^-5 once the axis has
// gone past what a plain number says clearly.
std::string decadeText(scalar log_value, bool latex, bool powers)
{
    const long e = long(std::floor(log_value + 1e-9));
    const long m = std::lround(std::pow(scalar(10), log_value - scalar(e)));
    if (!powers) {
        char buf[64];
        std::snprintf(buf, sizeof buf, "%.*f", int(std::max<long>(0, -e)),
                      double(m) * std::pow(10.0, double(e)));
        return buf;
    }
    const std::string p = latex ? "10^{" + std::to_string(e) + "}"
                                : "1e" + std::to_string(e);
    if (m == 1) return p;
    return latex ? std::to_string(m) + "\\cdot " + p
                 : std::to_string(m) + "e" + std::to_string(e);
}

} // namespace

vec2 decadeSpan(const vec2& data)
{
    const vec2 r = positiveRange(data);
    return vec2(std::log10(r(0)), std::log10(r(1)));
}

// the shared namespace
// A snippet section answers first, then a stated value, then a parameter.

scalar Settings::num(const std::string& key, scalar def, scalar lo, scalar hi) const
{
    if (Snippet::get(full(key)).valid())
        return Snippet::get(full(key), 1).num();
    if (auto it = stated.find(key); it != stated.end())
        return it->second.get<scalar>();
    return *Params::Add(full(key), def, lo, hi);
}

scalar Settings::atLeast(const std::string& key, scalar def, scalar floor) const
{
    if (Snippet::get(full(key)).valid())
        return std::max(Snippet::get(full(key), 1).num(), floor);
    if (auto it = stated.find(key); it != stated.end())
        return std::max(it->second.get<scalar>(), floor);
    return *Params::AddAtLeast(full(key), def, floor);
}

vec2 Settings::rect(const std::string& key, const vec2& def) const
{
    if (Snippet::get(full(key)).valid())
        return Snippet::get(full(key), 2).v2();
    if (auto it = stated.find(key); it != stated.end() && it->second.size() == 2)
        return vec2(it->second[0].get<scalar>(), it->second[1].get<scalar>());
    return *Params::AddVec2(full(key), def);
}

RGBA Settings::ink(const std::string& key, const RGBA& def) const
{
    if (Snippet::get(full(key)).valid())
        return Snippet::get(full(key), 4).rgba();
    if (auto it = stated.find(key); it != stated.end() && it->second.size() >= 3) {
        const json& c = it->second;
        return RGBA(c[0].get<float>(), c[1].get<float>(), c[2].get<float>(),
                    c.size() > 3 ? c[3].get<float>() : 1.f);
    }
    // captured once, so a live default (the slide bg) cannot rewrite an untouched param every frame
    const RGBA& first = first_ink.try_emplace(key, def).first->second;
    return *Params::AddColor(full(key), first);
}

bool Settings::flag(const std::string& key, bool def) const
{
    if (auto it = stated.find(key); it != stated.end())
        return it->second.get<bool>();
    return *Params::AddBool(full(key), def);
}

std::string Settings::choice(const std::string& key,
                             const std::vector<std::string>& options,
                             const std::string& def) const
{
    if (auto it = stated.find(key); it != stated.end()) {
        const std::string want = it->second.get<std::string>();
        if (std::find(options.begin(), options.end(), want) == options.end()) {
            std::string all;
            for (const auto& o : options) all += (all.empty() ? "" : " | ") + o;
            throw std::runtime_error("\"" + full(key) + "\" is one of " + all +
                                     ", not \"" + want + "\"");
        }
        return want;
    }
    // through the handle, which stamps the read so the Tuner shows it
    return Params::AddEnum(full(key), options, def).choice();
}

// the board

// the boards by name, weakly, so one that goes away takes its entry with it
static std::map<std::string, std::weak_ptr<Board>>& registry()
{
    static std::map<std::string, std::weak_ptr<Board>> r;
    return r;
}

const std::vector<std::string>& Board::settingNames()
{
    static const std::vector<std::string> all = {
        "xrange", "yrange", "xscale", "yscale", "xstep", "ystep",
        "tick_font", "tick_size", "xticks", "yticks",
        "show_grid", "show_axes", "show_frame",
        "background", "grid", "axis", "line_width",
    };
    return all;
}

BoardPtr Board::find(const std::string& name)
{
    const auto it = registry().find(name);
    return it == registry().end() ? nullptr : it->second.lock();
}

BoardPtr Board::Add(const std::string& name, std::optional<vec2> x, std::optional<vec2> y,
                    int w, int h)
{
    auto p = NewPrimitive<Board>();
    Board* self = p.get();
    registry()[name] = p;
    p->name = name;
    if (const path f = shaderFileFor(name, kFrameSource); !f.empty())
        p->setFragmentFile(f);
    else
        p->setFragmentSource(kFrameSource);
    p->registerLive();
    p->setResolution(w, h);

    p->settings.owner = name;
    // a range given here is fixed, everything else declares itself on read
    if (x) p->settings.set("xrange", json::array({(*x)(0), (*x)(1)}));
    if (y) p->settings.set("yrange", json::array({(*y)(0), (*y)(1)}));

    p->bind("background", [self] {
        return self->settings.ink("background", RGBA(1.f, 1.f, 1.f, 1.f));
    });
    p->bind("grid_color", [self] { return self->settings.ink("grid", kGridInk); });
    p->bind("axis_color", [self] { return self->settings.ink("axis", kAxisInk); });

    // the frame reads its own namespace, like everything else
    p->bindViewRect([self] { return std::make_pair(
        vec2(self->xview()(0), self->yview()(0)),
        vec2(self->xview()(1), self->yview()(1))); });
    // a log axis has no zero, so its line is sent out of the rectangle
    p->bind("axis_at", [self] {
        const scalar away = 1e9;
        return vec2(self->xlog() ? away : 0, self->ylog() ? away : 0);
    });
    p->bind("grid_step",  [self] { return self->step(); });
    p->bind("line_width", [self] { return self->settings.num("line_width", 1.8, 0, 6); });
    p->bind("show", [self] {
        return vec(self->settings.flag("show_grid", true) ? 1 : 0,
                   self->settings.flag("show_axes", true) ? 1 : 0,
                   self->settings.flag("show_frame", true) ? 1 : 0);
    });
    return p;
}

vec2 Board::xrange() const { return settings.rect("xrange", vec2(-1, 1)); }
vec2 Board::yrange() const { return settings.rect("yrange", vec2(-1, 1)); }

bool Board::xlog() const { return settings.choice("xscale", kScales, "linear") == "log"; }
bool Board::ylog() const { return settings.choice("yscale", kScales, "linear") == "log"; }

// a decade of a log axis is a unit of the referential, so everything drawn
// there keeps its widths in pixels and its grid evenly spaced
vec2 Board::toView(const vec2& p) const
{
    return vec2(xlog() ? std::log10(std::max(p(0), std::numeric_limits<scalar>::min())) : p(0),
                ylog() ? std::log10(std::max(p(1), std::numeric_limits<scalar>::min())) : p(1));
}

vec2 Board::xview() const { return xlog() ? decadeSpan(xrange()) : xrange(); }
vec2 Board::yview() const { return ylog() ? decadeSpan(yrange()) : yrange(); }

void Board::setRange(const vec2& x, const vec2& y)
{
    Params::write(name + "/xrange", x);
    Params::write(name + "/yrange", y);
}

vec2 Board::step() const
{
    const vec2 x = xview(), y = yview();
    // 0 asks for round numbers chosen from the range
    const scalar asked_x = settings.atLeast("xstep", 0);
    const scalar asked_y = settings.atLeast("ystep", 0);
    // a decade is the whole spacing of a log axis, xstep says nothing there
    return vec2(xlog() ? 1 : asked_x > 0 ? asked_x : niceStep((x(1) - x(0)) / 7),
                ylog() ? 1 : asked_y > 0 ? asked_y : niceStep((y(1) - y(0)) / 5));
}

// placing things on the board

ScreenPrimitiveInSlide Board::label(const ScreenPrimitivePtr& p, const vec2& at,
                                   const vec2& offset)
{
    return p->at(tracker(toView(at), offset));
}

StateInSlide Board::frame(const StateInSlide& own) const
{
    StateInSlide s;
    s.alpha  = own.getAlpha();
    s.anchor = anchor;          // the board's own, mirroring where it was drawn
    s.scale  = drawn_scale;
    return s;
}

// The Latex of a number, made once and shared. Compiling one costs a
// pdflatex run, so it is made only when Latex is the font asked for.
static LatexPtr tickLatex(const std::string& body)
{
    static std::map<std::string, LatexPtr> made;
    auto it = made.find(body);
    if (it == made.end())
        it = made.emplace(body, Latex::Add(body, 1)).first;
    return it->second;
}

void Board::syncTicks()
{
    const vec2 x = xview(), y = yview(), s = step();
    // a moving range would pay a compile a frame, which is what text is for
    const bool latex = settings.choice("tick_font", kFonts, "latex") == "latex";

    std::vector<Tick> next;
    auto add = [&](scalar v, bool horizontal, const std::string& text) {
        Tick tk{v, horizontal, text, nullptr, nullptr};
        if (latex)
            tk.tex = tickLatex("$" + tk.text + "$");
        next.push_back(std::move(tk));
    };
    // the value is where the label sits, which on a log axis is its decade
    auto axis = [&](const vec2& r, scalar spacing, bool log, bool horizontal) {
        if (log) {
            const std::vector<scalar> decades = decadeValues(r(0), r(1));
            const bool powers = wantsPowers(decades);
            for (scalar v : decades)
                add(v, horizontal, decadeText(v, latex, powers));
        }
        else
            for (scalar v : tickValues(r(0), r(1), spacing))
                add(v, horizontal, decimalText(v, spacing));
    };
    if (settings.choice("xticks", kXSides, "bottom") != "none")
        axis(x, s(0), xlog(), true);
    if (settings.choice("yticks", kYSides, "left") != "none")
        axis(y, s(1), ylog(), false);

    // the anchors outlive the rebuild, no allocation a frame
    for (std::size_t i = 0; i < next.size(); i++)
        next[i].anchor = i < tick_labels.size() ? tick_labels[i].anchor
                                                : AbsoluteAnchor::Add(vec2(-1, -1));
    tick_labels.swap(next);
}

void Board::drawTicks(const TimeObject& t, const StateInSlide& sis)
{
    ImVec2 pmin, pmax;
    screenRect(sis, pmin, pmax);
    const ImVec2 W = ImGui::GetWindowSize();
    if (W.x <= 0 || W.y <= 0) return;

    const scalar sc = sis.getScale();
    const scalar size = settings.num("tick_size", 0.4, 0.1, 1.5);
    const float pad = float(8 * sc);            // px between a label and the frame
    // drawn text takes this ink, a Latex label carries its own
    const RGBA ink = settings.ink("axis", kAxisInk);
    const ImU32 col = ImGui::GetColorU32(ImVec4(ink.Value.x, ink.Value.y, ink.Value.z,
                                                float(ink.Value.w * sis.getAlpha())));
    ImFont* font = ImGui::GetFont();
    const float font_px = float(44 * size * sc);

    const bool latex = settings.choice("tick_font", kFonts, "latex") == "latex";
    const std::string xside = settings.choice("xticks", kXSides, "bottom");
    const std::string yside = settings.choice("yticks", kYSides, "left");

    for (const Tick& tk : tick_labels) {
        const std::string& side = tk.horizontal ? xside : yside;
        if (side == "none") continue;

        vec2 wh;                                  // the label's size, in pixels
        if (tk.tex) {
            tk.tex->scale = size;
            wh = tk.tex->getSize() * sc;
        } else {
            const ImVec2 m = font->CalcTextSizeA(font_px, FLT_MAX, 0.f, tk.text.c_str());
            wh = vec2(m.x, m.y);
        }

        const vec2 on = worldToScreen(tk.horizontal ? vec2(tk.value, 0)
                                                    : vec2(0, tk.value));
        // hung off the edge, since the axis may be out of the range
        const ImVec2 c = tk.horizontal
            ? ImVec2(float(on(0)) * W.x, side == "top" ? pmin.y - pad - float(wh(1)) * 0.5f
                                                      : pmax.y + pad + float(wh(1)) * 0.5f)
            : ImVec2(side == "right" ? pmax.x + pad + float(wh(0)) * 0.5f
                                     : pmin.x - pad - float(wh(0)) * 0.5f,
                     float(on(1)) * W.y);

        if (tk.tex) {
            tk.anchor->updatePos(vec2(c.x / W.x, c.y / W.y));
            StateInSlide s;
            s.anchor = tk.anchor;
            s.scale  = sc;
            s.alpha  = sis.getAlpha();
            tk.tex->draw(t, s);
        } else {
            ImGui::GetWindowDrawList()->AddText(
                font, font_px, ImVec2(c.x - float(wh(0)) * 0.5f, c.y - float(wh(1)) * 0.5f),
                col, tk.text.c_str());
        }
    }
}

void Board::draw(const TimeObject& t, const StateInSlide& sis)
{ Shader::draw(t, sis); syncTicks(); drawTicks(t, sis); }

void Board::playIntro(const TimeObject& t, const StateInSlide& sis)
{ Shader::playIntro(t, sis); syncTicks(); drawTicks(t, sis); }

void Board::playOutro(const TimeObject& t, const StateInSlide& sis)
{ Shader::playOutro(t, sis); syncTicks(); drawTicks(t, sis); }

}
