#include "content/screen_primitives/text/Algorithm.h"
#include <spdlog/spdlog.h>
#include <fmt/core.h>

namespace slope {

static std::string fnv(const std::string& s)
{
    std::uint64_t h = 1469598103934665603ull;
    for (unsigned char c : s) { h ^= c; h *= 1099511628211ull; }
    return fmt::format("{:016x}", h);
}

static path linesPath(const std::string& key) { return Options::CachePath + key + ".lines"; }

// A \pdfsavepos at the start of each line lands on its baseline; positions are
// only known at shipout, hence \write. algorithmicx lines go through the list
// label \ALG@step, algorithm2e lines through \algocf@everypar. algorithm2e only
// counts lines when numbered, so numbering is hidden rather than off.
static std::string hooks(const std::string& key)
{
    return R"(\newwrite\slopeAlgOut\immediate\openout\slopeAlgOut=)" + key + R"(.lines
\csname newcount\endcsname\slopeAlgN\global\slopeAlgN=0
\csname newcount\endcsname\slopeAlgLast\global\slopeAlgLast=0
\def\slopeAlgPos#1{\pdfsavepos\edef\slopeAlgW{\write\slopeAlgOut{#1}}\slopeAlgW}
\def\slopeAlgRecord{\ifnum\slopeAlgN>\slopeAlgLast\global\slopeAlgLast=\slopeAlgN\slopeAlgPos{L \the\slopeAlgN\space\noexpand\the\noexpand\pdflastypos\space\noexpand\number\noexpand\pdfpageheight\space\noexpand\number\noexpand\ht\noexpand\slopebox}\fi}
\expandafter\let\expandafter\slopeAlgStep\csname ALG@step\endcsname
\expandafter\def\csname ALG@step\endcsname{\slopeAlgStep\global\slopeAlgN=\value{ALG@line}\slopeAlgRecord}
\expandafter\let\expandafter\slopeAlgPar\csname algocf@everypar\endcsname
\expandafter\def\csname algocf@everypar\endcsname{\slopeAlgPar\global\slopeAlgN=\value{AlgoLine}\slopeAlgRecord}
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

void Algorithm::setSource(const TexObject& tex)
{
    key = "alg_" + fnv(tex);
    content = tex;
    // algorithm2e draws its numbers left of the box, the pad keeps them in; -trim drops it otherwise
    const std::string w = listing_width > 0 ? std::to_string(listing_width) : "493.69707";
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

// cues keep the line numbers they resolved to, a moved \slopemark needs a deck reload
void Algorithm::HotReloadIfModified()
{
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
        a->ensureRendered();
        spdlog::info("[algo] reloaded {}", a->source_file.string());
    }
}

// "L n ypos pageheight boxheight" in sp, "M name n"
void Algorithm::parseLines()
{
    parsed_for = full_content;
    baseline_px.clear();
    marks.clear();
    std::ifstream f(linesPath(key));
    if (!f || baseline < 0) {
        spdlog::warn("[algo] no line positions for {}, focus disabled", key);
        return;
    }
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
    if (ys.empty())
        return;
    baseline_px.assign(ys.rbegin()->first, 0.);
    for (auto [n, y] : ys)
        if (n >= 1) baseline_px[n-1] = y;

    std::vector<double> gaps;
    for (int i = 1; i < count(); ++i)
        gaps.push_back(baseline_px[i] - baseline_px[i-1]);
    std::sort(gaps.begin(), gaps.end());
    pitch_px = gaps.empty() ? 12 * 65536. * px_per_sp : gaps[gaps.size()/2];
}

double Algorithm::yOf(double l) const
{
    l = std::clamp(l, 1., double(count()));
    const int i = int(std::floor(l));
    if (i >= count())
        return baseline_px.back();
    return std::lerp(baseline_px[i-1], baseline_px[i], l - i);
}

int Algorithm::markOf(const std::string& label) const
{
    if (auto it = marks.find(label); it != marks.end())
        return it->second;
    spdlog::warn("[algo] no \\slopemark{{{}}}", label);
    return -1;
}

int Algorithm::revealOn(int slide) const
{
    if (reveal_at.empty())
        return count();
    auto it = reveal_at.upper_bound(slide);
    return it == reveal_at.begin() ? 0 : std::prev(it)->second;
}

bool Algorithm::focusOn(int slide, int& first, int& last) const
{
    auto it = focus_at.upper_bound(slide);
    if (it == focus_at.begin())
        return false;
    first = std::prev(it)->second.first;
    last  = std::prev(it)->second.second;
    return first > 0;
}

// cues resolve lines at compose time, so the positions must be read by then
#define ALGO_CUE(...) \
    const auto id = pid; \
    return {[=](int slide) { \
        auto c = Primitive::get<Algorithm>(id); \
        FlushPending(); \
        if (c->parsed_for != c->full_content) c->parseLines(); \
        __VA_ARGS__ \
    }};

SlideCue Algorithm::reveal(CodeAnchor where)
{
    ALGO_CUE(c->reveal_at[slide] = where == END ? c->count() : 0;)
}

SlideCue Algorithm::reveal(int line)
{
    ALGO_CUE(c->reveal_at[slide] = std::clamp(line, 0, c->count());)
}

SlideCue Algorithm::reveal(const std::string& label)
{
    ALGO_CUE(if (int n = c->markOf(label); n >= 0) c->reveal_at[slide] = n;)
}

SlideCue Algorithm::focus(const std::string& label)
{
    ALGO_CUE(int n = c->markOf(label);
             c->focus_at[slide] = n > 0 ? std::pair{n, n} : std::pair{0, 0};)
}

SlideCue Algorithm::focus(const std::string& from, const std::string& to)
{
    ALGO_CUE(int a = c->markOf(from), b = c->markOf(to);
             if (b < a) std::swap(a, b);
             c->focus_at[slide] = a > 0 ? std::pair{a, b} : std::pair{0, 0};)
}

SlideCue Algorithm::focus(int first_line, int last_line)
{
    ALGO_CUE(const int n = std::max(c->count(), 1);
             const int a = std::clamp(first_line, 1, n), b = std::clamp(last_line, 1, n);
             c->focus_at[slide] = b < a ? std::pair{0, 0} : std::pair{a, b};)
}

SlideCue Algorithm::unfocus()
{
    ALGO_CUE(c->focus_at[slide] = {0, 0};)
}

#undef ALGO_CUE

void Algorithm::draw(const TimeObject& t, const StateInSlide& sis)
{
    FlushPending();
    if (data.width == -1)
        return;
    if (parsed_for != full_content)
        parseLines();
    // the strips below are axis aligned, a plane or a rotation draws it whole
    if (sis.hasPlane() || std::abs(sis.getAngle()) > 0.001 || baseline_px.empty()) {
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
    const float W = float(data.width * sx);
    const auto P = sis.getAbsolutePosition();
    const ImVec2 o(P.x - W * 0.5f, float(P.y - H * sy * 0.5));

    const double asc = 0.72 * pitch_px, desc = 0.28 * pitch_px;
    const parameter pos = t.slidePosition();
    const int i = int(std::floor(pos));
    const double f = std::clamp<double>(pos - i, 0, 1);

    auto clipOf = [&](int slide) {
        const int r = revealOn(slide);
        if (r >= count()) return H;
        return r <= 0 ? yOf(1) - asc : yOf(r) + desc;
    };
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
        dl->AddImage(tex, ImVec2(o.x, float(o.y + y0 * sy)), ImVec2(o.x + W, float(o.y + y1 * sy)),
                     ImVec2(0, float(y0 / H)), ImVec2(1, float(y1 / H)),
                     ImColor(1.f, 1.f, 1.f, a * alpha));
    };

    if (amt < 0.001) {
        strip(0, H, 1);
        return;
    }
    const double top = yOf(bf) - asc, bot = yOf(bl) + desc;
    ImVec4 hc = highlight.getImColor();
    hc.w *= float(amt) * alpha;
    const double btop = std::min(top, clip), bbot = std::min(bot, clip);
    if (bbot > btop)
        dl->AddRectFilled(ImVec2(o.x, float(o.y + btop * sy)), ImVec2(o.x + W, float(o.y + bbot * sy)),
                          ImColor(hc));
    const float dim = float(1 - amt * (1 - dim_factor));
    strip(0, top, dim);
    strip(top, bot, 1);
    strip(bot, H, dim);
}

}
