#include "content/screen_primitives/text/Algorithm.h"
#include "extern/stb_image.h"
#include <spdlog/spdlog.h>
#include <fmt/core.h>
#include <climits>
#include <set>

namespace slope {

static std::string fnv(const std::string& s)
{
    std::uint64_t h = 1469598103934665603ull;
    for (unsigned char c : s) { h ^= c; h *= 1099511628211ull; }
    return fmt::format("{:016x}", h);
}

static path linesPath(const std::string& key) { return Options::CachePath + key + ".lines"; }

// records each line's baseline with \pdfsavepos, through the line hooks of algorithmicx and algorithm2e
static std::string hooks(const std::string& key)
{
    return R"(\newwrite\slopeAlgOut\immediate\openout\slopeAlgOut=)" + key + R"(.lines
\csname newcount\endcsname\slopeAlgN\global\slopeAlgN=0
\csname newcount\endcsname\slopeAlgLast\global\slopeAlgLast=0
\csname newcount\endcsname\slopeAlgO\global\slopeAlgO=0
\csname newcount\endcsname\slopeAlgV\global\slopeAlgV=0
\def\slopeAlgPos#1{\pdfsavepos\edef\slopeAlgW{\write\slopeAlgOut{#1}}\slopeAlgW}
\def\slopeAlgRecord{\ifnum\slopeAlgN>\slopeAlgLast\global\slopeAlgLast=\slopeAlgN\slopeAlgPos{L \the\slopeAlgN\space\noexpand\the\noexpand\pdflastypos\space\noexpand\number\noexpand\pdfpageheight\space\noexpand\number\noexpand\ht\noexpand\slopebox}\fi}
\def\slopeAlgSet#1{\ifnum#1<\slopeAlgV\global\slopeAlgO=\slopeAlgLast\fi\global\slopeAlgV=#1\relax\global\slopeAlgN=\numexpr\slopeAlgO+#1\relax\slopeAlgRecord}
\expandafter\let\expandafter\slopeAlgStep\csname ALG@step\endcsname
\expandafter\def\csname ALG@step\endcsname{\slopeAlgStep\slopeAlgSet{\value{ALG@line}}}
\expandafter\let\expandafter\slopeAlgPar\csname algocf@everypar\endcsname
\expandafter\def\csname algocf@everypar\endcsname{\slopeAlgPar\slopeAlgSet{\value{AlgoLine}}}
\def\slopemark#1{\immediate\write\slopeAlgOut{M #1 \the\slopeAlgN}}
\csname LinesNumberedHidden\endcsname
)";
}

AlgorithmPtr Algorithm::Add(const TexObject& tex, scalar scale, int width)
{
    auto r = NewPrimitive<Algorithm>();
    all.push_back(r.get());
    r->isFormula = false;
    r->scale = scale;
    r->width = -1;
    r->listing_width = width;
    r->setSource(tex);
    r->full_content = WriteTexFile(r->tex_source, false, -1, false);

    const path png = GetLatexPath(r->full_content);
    if (io::file_exists(png) && io::file_exists(linesPath(r->key)) && !Options::ignore_cache) {
        try {
            r->loadTexture(png);
            r->baseline = ReadBaseline(png);
            return r;
        } catch (const std::exception& e) {
            spdlog::error("[algo] {}", e.what());
        }
    }
    pending.push_back(r);
    return r;
}

// width and preamble move the lines, so both are in the key
void Algorithm::setSource(const TexObject& tex)
{
    const std::string w = listing_width > 0 ? std::to_string(listing_width) : "493.69707";
    key = "alg_" + fnv(Latex::context + "|" + w + "|" + tex);
    content = tex;
    // algorithm2e draws its numbers left of the box, the pad keeps them in; -trim drops it otherwise
    tex_source = hooks(key) + "\\hspace*{2em}\\begin{varwidth}[t]{" + w + "pt}\n"
               + tex + "\n\\end{varwidth}";
}

static std::string readAll(const path& p)
{
    std::ifstream f(p);
    if (!f)
        throw std::runtime_error("[algo] cannot read " + p.string());
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

AlgorithmPtr Algorithm::FromFile(const path& file, scalar scale, int width)
{
    const path p = formatPath(file);
    auto r = Add(readAll(p), scale, width);
    std::error_code ec;
    r->source_file = std::filesystem::weakly_canonical(p, ec);
    if (ec) r->source_file = p;
    r->origin = r->source_file;
    r->last_modified = std::filesystem::last_write_time(r->source_file, ec);
    return r;
}

void Algorithm::ClearAllCues()
{
    for (auto* a : all)
        a->clearCues();
}

std::vector<path> Algorithm::WatchedFiles()
{
    std::vector<path> out;
    for (auto* a : all)
        if (!a->source_file.empty()
            && std::find(out.begin(), out.end(), a->source_file) == out.end())
            out.push_back(a->source_file);
    return out;
}

void Algorithm::HotReloadIfModified()
{
    bool changed = false;
    for (auto* a : all) {
        if (a->source_file.empty())
            continue;
        std::error_code ec;
        const auto t = std::filesystem::last_write_time(a->source_file, ec);
        if (ec || t == a->last_modified)
            continue;
        a->last_modified = t;
        try {
            a->setSource(readAll(a->source_file));
        } catch (const std::exception& e) {
            spdlog::error("{}", e.what());
            continue;
        }
        changed = true;
        spdlog::info("[algo] reloading {}", a->source_file.string());
    }
    // compiled off the render thread, like every other latex reload
    if (changed)
        Latex::RegenerateAll();
}

// "L n ypos pageheight boxheight" in sp, "M name n"
void Algorithm::parseLines()
{
    baseline_px.clear();
    edge_px.clear();
    marks.clear();
    std::ifstream f(linesPath(key));
    if (!f || baseline < 0) {
        spdlog::warn("[algo] no line positions for {}, focus disabled", key);
        return;
    }
    parsed_for = full_content;
    const double px_per_sp = Options::PDFtoPNGDensity / 72.27 / 65536.;
    std::map<int, double> ys;
    std::string tag;
    while (f >> tag) {
        if (tag == "L") {
            int n; double y, page, ht;
            f >> n >> y >> page >> ht;
            // the preview page is the box plus a 5pt border on each side
            const double first = page - 5 * 65536. - ht;
            ys[n] = baseline + (first - y) * px_per_sp;
        } else if (tag == "M") {
            std::string name; int n;
            f >> name >> n;
            marks[name] = n;
        }
    }
    if (ys.empty()) {
        spdlog::warn("[algo] no line recorded for {}, focus and reveal need pdflatex "
                     "and algorithmicx or algorithm2e", key);
        return;
    }
    baseline_px.assign(ys.rbegin()->first, 0.);
    for (auto [n, y] : ys)
        if (n >= 1) baseline_px[n-1] = y;

    std::vector<double> gaps;
    for (int i = 1; i < count(); ++i)
        gaps.push_back(baseline_px[i] - baseline_px[i-1]);
    std::sort(gaps.begin(), gaps.end());
    pitch_px = gaps.empty() ? 12 * 65536. * px_per_sp : gaps[gaps.size()/2];

    cutLines(GetLatexPath(full_content));
    warnUnknownMarks();
}

// cut in the blank rows above each line's ink, so wrapped rows and tall math stay with their line
void Algorithm::cutLines(const path& png)
{
    const int n = count();
    const double asc = 0.72 * pitch_px;
    edge_px.assign(n + 1, double(data.height));
    edge_px[0] = std::max(0., baseline_px[0] - asc);
    for (int k = 1; k < n; ++k)
        edge_px[k] = std::clamp(baseline_px[k] - asc, baseline_px[k-1], baseline_px[k]);

    int w, h;
    unsigned char* px = stbi_load(png.string().c_str(), &w, &h, nullptr, 4);
    if (!px)
        return;
    std::vector<char> ink(h, 0);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w && !ink[y]; ++x)
            ink[y] = px[(std::size_t(y) * w + x) * 4 + 3] > 16;
    stbi_image_free(px);

    // from inside the x-height of the line on `base`, up to the blank run above it
    auto blankAbove = [&](double base, double stop_y, int& top, int& bottom) {
        const int stop = std::clamp(int(std::ceil(stop_y)), 0, h - 1);
        int y = std::clamp(int(base - 0.25 * pitch_px), stop, h - 1);
        while (y > stop && ink[y]) --y;
        if (ink[y])
            return false;
        bottom = y;
        while (y > stop && !ink[y-1]) --y;
        top = y;
        return true;
    };
    int top, bottom;
    if (blankAbove(baseline_px[0], 0, top, bottom))
        edge_px[0] = std::max(double(top), bottom + 1 - 0.15 * pitch_px);
    for (int k = 1; k < n; ++k)
        if (blankAbove(baseline_px[k], baseline_px[k-1], top, bottom))
            edge_px[k] = 0.5 * (top + bottom + 1);
}

void Algorithm::warnUnknownMarks()
{
    std::set<std::string> missing;
    auto check = [&](const LineRef& r) {
        if (!r.label.empty() && !marks.count(r.label))
            missing.insert(r.label);
    };
    for (const auto& [s, r] : reveal_at)
        check(r);
    for (const auto& [s, f] : focus_at) {
        check(f.first);
        check(f.second);
    }
    for (const auto& m : missing)
        spdlog::warn("[algo] no \\slopemark{{{}}}", m);
}

double Algorithm::edgeOf(double k) const
{
    k = std::clamp(k, 0., double(count()));
    const int i = int(std::floor(k));
    if (i >= count())
        return edge_px.back();
    return std::lerp(edge_px[i], edge_px[i+1], k - i);
}

// -1 for an unknown mark
int Algorithm::resolve(const LineRef& ref) const
{
    if (ref.label.empty())
        return std::clamp(ref.line, 0, count());
    auto it = marks.find(ref.label);
    return it == marks.end() ? -1 : std::clamp(it->second, 0, count());
}

int Algorithm::revealOn(int slide) const
{
    auto it = reveal_at.upper_bound(slide);
    if (it == reveal_at.begin())
        return count();
    const int r = resolve(std::prev(it)->second);
    return r < 0 ? count() : r;
}

bool Algorithm::focusOn(int slide, int& first, int& last) const
{
    auto it = focus_at.upper_bound(slide);
    if (it == focus_at.begin())
        return false;
    first = resolve(std::prev(it)->second.first);
    last  = resolve(std::prev(it)->second.second);
    if (first <= 0 || last <= 0)
        return false;
    if (last < first)
        std::swap(first, last);
    return true;
}

#define ALGO_CUE(...) \
    const auto id = pid; \
    return {[=](int slide) { \
        auto c = Primitive::get<Algorithm>(id); \
        __VA_ARGS__ \
    }};

SlideCue Algorithm::reveal(CodeAnchor where)
{
    ALGO_CUE(c->reveal_at[slide] = {"", where == END ? INT_MAX : 0};)
}

SlideCue Algorithm::reveal(int line)
{
    ALGO_CUE(c->reveal_at[slide] = {"", line};)
}

SlideCue Algorithm::reveal(const std::string& label)
{
    ALGO_CUE(c->reveal_at[slide] = {label, 0};)
}

SlideCue Algorithm::focus(const std::string& label)
{
    ALGO_CUE(c->focus_at[slide] = {{label, 0}, {label, 0}};)
}

SlideCue Algorithm::focus(const std::string& from, const std::string& to)
{
    ALGO_CUE(c->focus_at[slide] = {{from, 0}, {to, 0}};)
}

SlideCue Algorithm::focus(int first_line, int last_line)
{
    ALGO_CUE(c->focus_at[slide] = {{"", first_line}, {"", last_line}};)
}

SlideCue Algorithm::unfocus()
{
    ALGO_CUE(c->focus_at[slide] = {};)
}

#undef ALGO_CUE

void Algorithm::draw(const TimeObject& t, const StateInSlide& sis)
{
    FlushPending();
    if (data.width == -1)
        return;
    // the lines of an image still compiling, or that failed, are not written yet
    if (parsed_for != full_content && !batch_future.valid()
        && io::file_exists(GetLatexPath(full_content)))
        parseLines();
    if (sis.hasPlane() || baseline_px.empty()) {
        if (sis.hasPlane() && !warned_plane && (!reveal_at.empty() || !focus_at.empty())) {
            warned_plane = true;
            spdlog::warn("[algo] reveal and focus are ignored on a plane");
        }
        display(sis);
        return;
    }
    if (deferred_texels > 0 && in_render_pass) {
        const double k = deferred_texels;
        deferred_texels = 0;
        reloadTexels(k);
    }
    anchor->updatePos(sis.getPosition());
    auto [dx, dy] = drawScale();
    const double sx = dx * sis.getScale(), sy = dy * sis.getScale();
    ensureTexelsFor(sx, sy);

    const double H = data.height;
    const double W = data.width * sx;
    const auto P = sis.getAbsolutePosition();
    const double ca = std::cos(sis.getAngle()), sa = std::sin(sis.getAngle());
    // x in screen pixels from the left edge, y in png pixels from the top
    auto at = [&](double x, double y) {
        const double lx = x - W * 0.5, ly = (y - H * 0.5) * sy;
        return ImVec2(float(P.x + lx * ca - ly * sa), float(P.y + lx * sa + ly * ca));
    };

    const parameter pos = t.slidePosition();
    const int i = int(std::floor(pos));
    const double f = std::clamp<double>(pos - i, 0, 1);

    auto clipOf = [&](int slide) { return edge_px[std::clamp(revealOn(slide), 0, count())]; };
    const double clip = std::lerp(clipOf(i), clipOf(i+1), f);

    int fa, la, fb, lb;
    const bool a = focusOn(i, fa, la), b = focusOn(i+1, fb, lb);
    double bf = 0, bl = 0, amt = 0;
    if (a && b)  { bf = std::lerp(fa, fb, f); bl = std::lerp(la, lb, f); amt = 1; }
    else if (a || b) { bf = a ? fa : fb; bl = a ? la : lb; amt = a ? 1 - f : f; }

    const float alpha = float(sis.getAlpha());
    auto* dl = ImGui::GetWindowDrawList();
    const auto tex = (ImTextureID)(intptr_t)data.texture;
    auto strip = [&](double y0, double y1, float a) {
        y0 = std::clamp(y0, 0., clip);
        y1 = std::clamp(y1, 0., clip);
        if (y1 <= y0) return;
        const float v0 = float(y0 / H), v1 = float(y1 / H);
        dl->AddImageQuad(tex, at(0, y0), at(W, y0), at(W, y1), at(0, y1),
                         ImVec2(0, v0), ImVec2(1, v0), ImVec2(1, v1), ImVec2(0, v1),
                         ImColor(1.f, 1.f, 1.f, a * alpha));
    };

    if (amt < 0.001) {
        strip(0, H, 1);
        return;
    }
    const double top = edgeOf(bf - 1), bot = edgeOf(bl);
    ImVec4 hc = highlight.getImColor();
    hc.w *= float(amt) * alpha;
    const double btop = std::min(top, clip), bbot = std::min(bot, clip);
    if (bbot > btop)
        dl->AddQuadFilled(at(0, btop), at(W, btop), at(W, bbot), at(0, bbot), ImColor(hc));
    const float dim = float(1 - amt * (1 - dim_factor));
    strip(0, top, dim);
    strip(top, bot, 1);
    strip(bot, H, dim);
}

}
