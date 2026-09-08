#include "content/screen_primitives/text/Code.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "content/config/Options.h"
#include "content/config/io.h"
#include "polyscope/render/engine.h"
#include <tree_sitter/api.h>

#include <sstream>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <limits>

namespace slope {

namespace {

// the listing is drawn with its own font, so its size comes from that font
// rather than from whatever ImGui is currently set to
ImFont* fontOf(const CodeStyle& style)
{
    if (style.font)
        return style.font;
    if (!Options::CodeFont.empty())
        if (ImFont* f = Code::LoadFont(Options::CodeFont))
            return f;
    if (polyscope::render::engine && polyscope::render::engine->monoFont)
        return polyscope::render::engine->monoFont;
    return ImGui::GetFont();
}

float baseSizeOf(const CodeStyle& style)
{
    ImFont* f = fontOf(style);
    return f && f->LegacySize > 0 ? f->LegacySize : ImGui::GetFontSize();
}

// Draws a run glyph by glyph so the advance is ours, and returns its width.
// A null draw list measures without drawing.
float runOf(ImDrawList* dl, ImFont* font, float fs, ImVec2 pos, ImU32 col,
            const char* b, const char* e, float tracking)
{
    float x = 0;
    while (b < e) {
        unsigned int c = 0;
        const int n = ImTextCharFromUtf8(&c, b, e);
        if (!n)
            break;
        b += n;
        if (!c)
            continue;
        if (dl)
            font->RenderChar(dl, fs, ImVec2(pos.x + x, pos.y), col, ImWchar(c));
        const char* one = b - n;
        x += font->CalcTextSizeA(fs, FLT_MAX, 0.f, one, b).x * tracking;
    }
    return x;
}

std::string trimmed(const std::string& s)
{
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    const auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

// filled by the cmake-generated Grammars.cpp, in declaration order
struct Grammar {
    std::string        name;
    Code::GrammarFn    fn;
};

std::vector<Grammar>& registry()
{
    static std::vector<Grammar> g;
    return g;
}

// on first use, which is also what keeps the generated object in the link
void ensureGrammars()
{
    static const bool once = [] { RegisterDeclaredGrammars(); return true; }();
    (void)once;
}

const Grammar* grammarNamed(const std::string& name)
{
    ensureGrammars();
    for (const auto& g : registry())
        if (g.name == name)
            return &g;
    return nullptr;
}

std::map<std::string, CodeLanguage>& extensionMap()
{
    static std::map<std::string, CodeLanguage> m;
    return m;
}

// cached for the life of the show, compiling one is the slow part
TSQuery* queryFor(const std::string& lang)
{
    static std::map<std::string, TSQuery*> cache;
    if (auto it = cache.find(lang); it != cache.end())
        return it->second;

    TSQuery* q = nullptr;
    if (const Grammar* g = grammarNamed(lang)) {
        const path file = path(Options::QueryPath) / (lang + ".scm");
        std::ifstream in(file);
        if (!in.is_open()) {
            spdlog::error("[code] no highlight query at {}", file.string());
        } else {
            std::stringstream buf; buf << in.rdbuf();
            const std::string text = buf.str();
            uint32_t err_off = 0;
            TSQueryError err = TSQueryErrorNone;
            q = ts_query_new(static_cast<const TSLanguage*>(g->fn()), text.c_str(),
                             uint32_t(text.size()), &err_off, &err);
            if (!q)
                spdlog::error("[code] {} is not a valid query (error {} at byte {})",
                              file.string(), int(err), err_off);
        }
    }
    cache[lang] = q;
    return q;
}

} // namespace

// the capture names the grammars use, mapped onto what a slide draws. A name
// is matched on its first component, so "function.builtin" lands on Function
Code::Tok Code::tokenOfCapture(std::string_view capture)
{
    const auto dot = capture.find('.');
    const std::string_view head = capture.substr(0, dot);
    if (head == "comment")                          return Code::Tok::Comment;
    if (head == "keyword")                          return Code::Tok::Keyword;
    if (head == "type" || head == "constructor")    return Code::Tok::Type;
    if (head == "string" || head == "number"
        || head == "character" || head == "escape") return Code::Tok::Literal;
    if (head == "preproc")                          return Code::Tok::Preproc;
    if (head == "function")                         return Code::Tok::Function;
    if (head == "constant" || head == "boolean")    return Code::Tok::Constant;
    if (head == "operator")                         return Code::Tok::Operator;
    if (head == "variable" || head == "property"
        || head == "label")                         return Code::Tok::Variable;
    return Code::Tok::Plain;
}


void Code::RegisterGrammar(const std::string& name, GrammarFn grammar,
                           const std::vector<std::string>& extensions)
{
    registry().push_back({name, grammar});
    for (const auto& e : extensions)
        extensionMap()[e] = CodeLanguage{name};
}

const CodeLanguage& CodeLanguage::PlainText()
{
    static const CodeLanguage l{""};
    return l;
}

const CodeLanguage& CodeLanguage::ForName(const std::string& name)
{
    static std::map<std::string, CodeLanguage> known;
    if (!grammarNamed(name)) {
        spdlog::warn("[code] no grammar named '{}' in this build", name);
        return PlainText();
    }
    auto it = known.find(name);
    if (it == known.end())
        it = known.emplace(name, CodeLanguage{name}).first;
    return it->second;
}

std::vector<std::string> CodeLanguage::Available()
{
    ensureGrammars();
    std::vector<std::string> names;
    for (const auto& g : registry())
        names.push_back(g.name);
    return names;
}

void CodeLanguage::Register(const std::string& extension, const CodeLanguage& lang)
{
    extensionMap()[extension] = lang;
}

const CodeLanguage& CodeLanguage::ForExtension(const std::string& extension)
{
    ensureGrammars();
    auto& m = extensionMap();
    auto it = m.find(extension);
    return it == m.end() ? PlainText() : it->second;
}

float Code::lineHeight() const
{
    return baseSizeOf(style) * style.font_scale * style.line_spacing;
}

// what the gutter shows for line i, absolute only makes sense from a file
int Code::lineNumberOf(size_t i) const
{
    if (style.absolute_line_numbers && from_file)
        return lines[i].file_line;
    return int(i) + 1;
}

// wide enough for the largest number drawn, plus a space
float Code::gutterWidth(ImFont* font, float fs) const
{
    if (!style.line_numbers || lines.empty())
        return 0.f;
    const std::string widest(std::to_string(lineNumberOf(lines.size()-1)).size() + 1, '0');
    return runOf(nullptr, font, fs, ImVec2(0,0), 0,
                 widest.c_str(), widest.c_str() + widest.size(), style.tracking);
}

void Code::setSource(const std::string& source, int base_line)
{
    lines.clear();
    regions.clear();
    points.clear();
    source_base = base_line;

    // pass 1 : split into lines, pulling out region markers so they never show
    std::vector<std::string> raw;
    std::vector<int> raw_file_line;   // a stripped marker still uses up a number
    {
        int n = base_line - 1;
        std::istringstream in(source);
        std::string l;
        std::map<std::string,int> open;
        while (std::getline(in, l)) {
            ++n;
            if (!l.empty() && l.back() == '\r')
                l.pop_back();
            // the tag is searched anywhere in the line rather than after a
            // fixed "//", so any comment syntax works : #, %, --, ;
            const auto t = trimmed(l);
            constexpr const char* beg_tag = "slope:begin ";
            constexpr const char* end_tag = "slope:end ";
            constexpr const char* here_tag = "slope:here ";
            // the point sits where the marker was
            if (const auto p = t.find(here_tag); p != std::string::npos) {
                points[trimmed(t.substr(p + std::strlen(here_tag)))] = int(raw.size());
                continue;
            }
            if (const auto p = t.find(beg_tag); p != std::string::npos) {
                const auto name = trimmed(t.substr(p + std::strlen(beg_tag)));
                open[name] = int(raw.size()) + 1;
                points[name + ".begin"] = int(raw.size());   // regions are two points
                continue;
            }
            if (const auto p = t.find(end_tag); p != std::string::npos) {
                const auto name = trimmed(t.substr(p + std::strlen(end_tag)));
                auto it = open.find(name);
                if (it != open.end()) {
                    regions[name] = {it->second, int(raw.size())};
                    open.erase(it);
                }
                points[name + ".end"] = int(raw.size());
                continue;
            }
            raw.push_back(l);
            raw_file_line.push_back(n);
        }
        for (const auto& [name, first] : open) {
            regions[name] = {first, int(raw.size())};
            spdlog::warn("[code] region '{}' is never closed", name);
        }
    }

    // pass 2, the spans. A query capture gives byte offsets, which is a span.
    for (size_t i = 0; i < raw.size(); ++i) {
        Line line;
        line.text = raw[i];
        line.file_line = raw_file_line[i];
        lines.push_back(std::move(line));
    }

    // byte offset of each line, to cut a capture into the lines it covers
    std::vector<size_t> line_start(lines.size() + 1, 0);
    for (size_t i = 0; i < lines.size(); ++i)
        line_start[i+1] = line_start[i] + lines[i].text.size() + 1;   // + '\n'

    std::string stripped;
    stripped.reserve(line_start.back());
    for (const auto& l : lines)
        stripped += l.text + "\n";

    TSQuery* query = language.valid() ? queryFor(language.name) : nullptr;
    if (query) {
        TSParser* parser = ts_parser_new();
        ts_parser_set_language(parser,
            static_cast<const TSLanguage*>(grammarNamed(language.name)->fn()));
        TSTree* tree = ts_parser_parse_string(parser, nullptr, stripped.c_str(),
                                              uint32_t(stripped.size()));
        TSQueryCursor* cursor = ts_query_cursor_new();
        ts_query_cursor_exec(cursor, query, ts_tree_root_node(tree));

        TSQueryMatch match;
        while (ts_query_cursor_next_match(cursor, &match)) {
            for (uint16_t c = 0; c < match.capture_count; ++c) {
                const TSQueryCapture& cap = match.captures[c];
                uint32_t len = 0;
                const char* name = ts_query_capture_name_for_id(query, cap.index, &len);
                const Tok tok = tokenOfCapture(std::string_view(name, len));
                if (tok == Tok::Plain)
                    continue;
                size_t begin = ts_node_start_byte(cap.node);
                size_t end   = ts_node_end_byte(cap.node);
                if (end <= begin || end > stripped.size())
                    continue;
                // a capture may straddle lines, a span may not
                auto lit = std::upper_bound(line_start.begin(), line_start.end(), begin);
                size_t li = size_t(lit - line_start.begin()) - 1;
                while (li < lines.size() && begin < end) {
                    const size_t stop = std::min(end, line_start[li] + lines[li].text.size());
                    if (stop > begin)
                        lines[li].spans.push_back({begin - line_start[li],
                                                   stop - line_start[li], tok});
                    begin = line_start[li+1];
                    ++li;
                }
            }
        }
        ts_query_cursor_delete(cursor);
        ts_tree_delete(tree);
        ts_parser_delete(parser);
    }

    // display() walks the spans in order and expects them disjoint
    for (auto& l : lines) {
        auto& sp = l.spans;
        std::stable_sort(sp.begin(), sp.end(),
                         [](const Span& a, const Span& b) { return a.begin < b.begin; });
        std::vector<Span> keep;
        for (const auto& x : sp) {
            if (!keep.empty() && x.begin < keep.back().end) {
                if (x.end <= keep.back().end) {   // the inner one is more specific
                    Span outer = keep.back();
                    keep.pop_back();
                    if (x.begin > outer.begin)
                        keep.push_back({outer.begin, x.begin, outer.tok});
                    keep.push_back(x);
                    if (outer.end > x.end)
                        keep.push_back({x.end, outer.end, outer.tok});
                    continue;
                }
                continue;   // straddles a boundary, dropped rather than overlapped
            }
            keep.push_back(x);
        }
        sp = std::move(keep);
    }

    content = source;
    buildPoints();   // the points moved, so the write budgets follow
}

size_t Code::indentOf(const std::string& text)
{
    size_t i = 0;
    while (i < text.size() && (text[i] == ' ' || text[i] == '\t'))
        ++i;
    return i;
}

int Code::typedChars(const std::string& text)
{
    // the indentation is jumped rather than typed, it would read as a stall
    return int(text.size() - indentOf(text));
}

void Code::buildPoints()
{
    unit_prefix.assign(lines.size() + 1, 0.f);
    for (size_t i = 0; i < lines.size(); ++i)
        unit_prefix[i+1] = unit_prefix[i]
                         + float(typedChars(lines[i].text)) + kNewlineCost;
    written_chars.assign(lines.size(), -1);
    caret_line = -1;
}

int Code::pointOf(const std::string& label) const
{
    if (auto it = points.find(label); it != points.end())
        return it->second;
    // a bare region name means its end
    if (auto it = regions.find(label); it != regions.end())
        return it->second.second;
    spdlog::warn("[code] no label or region named '{}'", label);
    return -1;
}

int Code::revealOn(int slide) const
{
    if (reveal_at.empty())
        return int(lines.size());          // never cued, so whole
    auto it = reveal_at.upper_bound(slide);
    if (it == reveal_at.begin())
        return 0;
    return std::prev(it)->second;
}

bool Code::focusOn(int slide, int& first, int& last) const
{
    if (focus_at.empty()) {                // never cued, fall back on highlight()
        first = hl_first; last = hl_last;
        return hl_first > 0;
    }
    auto it = focus_at.upper_bound(slide);
    if (it == focus_at.begin())
        return false;
    const auto& r = std::prev(it)->second;
    first = r.first; last = r.second;
    return first > 0;
}

// Everything animated here is a function of t.slidePosition(), never of dt.
void Code::updateFromShow(const TimeObject& t)
{
    if (written_chars.size() != lines.size() || unit_prefix.size() != lines.size()+1)
        buildPoints();

    const parameter pos = t.slidePosition();
    const int  i = int(std::floor(pos));
    const float f = float(std::clamp<parameter>(pos - i, 0, 1));

    auto budgetOf = [&](int slide) {
        const int p = std::clamp(revealOn(slide), 0, int(lines.size()));
        return unit_prefix[size_t(p)];
    };
    // one slide change writes whatever lies between the two points
    float budget = std::lerp(budgetOf(i), budgetOf(i+1), f);

    std::fill(written_chars.begin(), written_chars.end(), 0);
    caret_line = -1;
    const bool writing = f > 0.f && f < 1.f && budgetOf(i) != budgetOf(i+1);
    for (size_t l = 0; l < lines.size(); ++l) {
        const int n = typedChars(lines[l].text);
        const float cost = float(n) + kNewlineCost;
        if (budget >= cost) {
            written_chars[l] = -1;         // whole
            budget -= cost;
            continue;
        }
        written_chars[l] = std::clamp(int(budget), 0, n);
        // the first line the budget runs out on is the write head
        if (writing && caret_line < 0)
            caret_line = int(l) + 1;
        budget = 0;
    }

    int fa, la, fb, lb;
    const bool a = focusOn(i, fa, la);
    const bool b = focusOn(i+1, fb, lb);
    if (!a && !b) {
        focus_amt = 0;
    } else if (a && b) {
        band_first = std::lerp(float(fa), float(fb), f);
        band_last  = std::lerp(float(la), float(lb), f);
        focus_amt  = 1;
    } else {
        // one side only, so hold the region and fade the band
        band_first = float(a ? fa : fb);
        band_last  = float(a ? la : lb);
        focus_amt  = a ? 1.f - f : f;
    }
}

CodePtr Code::Add(const std::string& source, const CodeLanguage& lang)
{
    auto rslt = NewPrimitive<Code>();
    rslt->language = lang;
    rslt->setSource(source);
    return rslt;
}

void Code::setLanguage(const CodeLanguage& lang)
{
    if (lang.name == language.name)
        return;
    language = lang;
    setSource(content, source_base); // re-tokenize what is already loaded
}

CodePtr Code::FromFile(const path& file)
{
    // extension decides, unless the caller says otherwise
    auto ext = file.extension().string();
    if (!ext.empty() && ext.front() == '.')
        ext.erase(0, 1);
    return FromFile(file, "", "", CodeLanguage::ForExtension(ext));
}

CodePtr Code::FromFile(const path& file, const CodeLanguage& lang)
{
    return FromFile(file, "", "", lang);
}

CodePtr Code::FromFile(const path& file, int first_line, int last_line)
{
    auto ext = file.extension().string();
    if (!ext.empty() && ext.front() == '.')
        ext.erase(0, 1);
    return FromFile(file, first_line, last_line, CodeLanguage::ForExtension(ext));
}

CodePtr Code::FromFile(const path& file, int first_line, int last_line,
                       const CodeLanguage& lang)
{
    auto rslt = NewPrimitive<Code>();
    rslt->source_file  = formatPath(file);
    rslt->slice_first  = std::max(first_line, 1);
    rslt->slice_last   = last_line;
    rslt->from_file    = true;
    rslt->language     = lang;
    rslt->reloadFromFile();
    file_backed.push_back(rslt.get());
    return rslt;
}

CodePtr Code::FromFile(const path& file, const std::string& begin_marker,
                       const std::string& end_marker)
{
    auto ext = file.extension().string();
    if (!ext.empty() && ext.front() == '.')
        ext.erase(0, 1);
    return FromFile(file, begin_marker, end_marker, CodeLanguage::ForExtension(ext));
}

CodePtr Code::FromFile(const path& file, const std::string& begin_marker,
                       const std::string& end_marker, const CodeLanguage& lang)
{
    auto rslt = NewPrimitive<Code>();
    rslt->source_file   = formatPath(file);
    rslt->begin_marker  = begin_marker;
    rslt->end_marker    = end_marker;
    rslt->from_file     = true;
    rslt->language      = lang;
    rslt->reloadFromFile();
    file_backed.push_back(rslt.get());
    return rslt;
}

void Code::reloadFromFile()
{
    std::ifstream f(source_file);
    if (!f.is_open()) {
        spdlog::error("[code] could not open {}", source_file.string());
        setSource("<missing " + source_file.string() + ">");
        return;
    }
    std::stringstream buffer;
    buffer << f.rdbuf();

    std::string source = buffer.str();
    int base = 1;   // file line the kept text starts on, for the gutter
    if (!begin_marker.empty()) {
        // keep only what lies strictly between the two marker lines
        std::istringstream in(source);
        std::string l, kept;
        bool inside = false, found = false;
        int n = 0, first = 1;
        while (std::getline(in, l)) {
            ++n;
            if (!inside && l.find(begin_marker) != std::string::npos) {
                inside = found = true;
                first = n + 1;
                continue;
            }
            if (inside && !end_marker.empty() && l.find(end_marker) != std::string::npos)
                break;
            if (inside)
                kept += l + "\n";
        }
        if (!found)
            spdlog::warn("[code] marker '{}' not found in {}", begin_marker, source_file.string());
        else {
            source = kept;
            base = first;
        }
    }
    if (slice_first > 0) {
        // a plain line range, for a file nobody wants to decorate
        std::istringstream in(source);
        std::string l, kept;
        int n = 0;
        const int last = slice_last > 0 ? slice_last : std::numeric_limits<int>::max();
        while (std::getline(in, l)) {
            ++n;
            if (n > last)
                break;
            if (n >= slice_first)
                kept += l + "\n";
        }
        if (n < slice_first)
            spdlog::warn("[code] {} has {} lines, asked for {}..{}",
                         source_file.string(), n, slice_first, slice_last);
        source = kept;
        base += slice_first - 1;
    }
    setSource(source, base);

    std::error_code ec;
    auto t = std::filesystem::last_write_time(source_file, ec);
    if (!ec)
        last_modified = t;
}

namespace {

// lowercase, letters and digits only, so "JetBrains Mono" matches
// "JetBrainsMono-Regular.ttf"
std::string fontKey(const std::string& s)
{
    std::string k;
    for (unsigned char c : s)
        if (std::isalnum(c))
            k += char(std::tolower(c));
    return k;
}

// where a system keeps its fonts, plus the project itself, so a deck can ship
// the face it wants next to its slides
std::vector<path> fontDirs()
{
    std::vector<path> dirs;
    if (!Options::ProjectDataPath.empty())
        dirs.push_back(Options::ProjectDataPath);
    const char* home = std::getenv("HOME");
    if (home) {
        dirs.push_back(path(home) / ".local/share/fonts");
        dirs.push_back(path(home) / ".fonts");
        dirs.push_back(path(home) / "Library/Fonts");
    }
    if (const char* win = std::getenv("WINDIR"))
        dirs.push_back(path(win) / "Fonts");
    dirs.push_back("/usr/share/fonts");
    dirs.push_back("/usr/local/share/fonts");
    dirs.push_back("/Library/Fonts");
    dirs.push_back("/System/Library/Fonts");
    return dirs;
}

// The best match contains the name asked for and carries the fewest extra
// characters, with a heavy penalty for a weight or a slant the name did not
// ask for, so "JetBrains Mono" lands on the regular face and not on Bold.
int fontPenalty(const std::string& stem, const std::string& want)
{
    static const char* styles[] = {"bold","italic","oblique","thin","light",
                                   "black","heavy","medium","semi","extra",
                                   "condensed","expanded","mono"};
    int score = int(stem.size() - want.size());
    for (const char* st : styles)
        if (stem.find(st) != std::string::npos && want.find(st) == std::string::npos)
            score += 50;
    if (stem.find("regular") != std::string::npos)
        score -= 10;
    return score;
}

std::string resolveFontFile(const std::string& name)
{
    const std::string want = fontKey(name);
    if (want.empty())
        return "";

    std::string best;
    int best_len = std::numeric_limits<int>::max();
    for (const auto& dir : fontDirs()) {
        std::error_code ec;
        if (!std::filesystem::is_directory(dir, ec))
            continue;
        auto it = std::filesystem::recursive_directory_iterator(
            dir, std::filesystem::directory_options::skip_permission_denied, ec);
        for (const auto& e : it) {
            if (!e.is_regular_file(ec))
                continue;
            const auto ext = fontKey(e.path().extension().string());
            if (ext != "ttf" && ext != "otf")
                continue;
            const std::string stem = fontKey(e.path().stem().string());
            if (stem.find(want) == std::string::npos)
                continue;
            const int score = fontPenalty(stem, want);
            if (score < best_len) {
                best_len = score;
                best = e.path().string();
            }
        }
    }
    return best;
}

} // namespace

// The atlas takes new fonts after startup, the backend re-uploads it. A miss
// is cached too, so a bad name warns once instead of every frame.
ImFont* Code::LoadFont(const path& file, float size)
{
    static std::map<std::string, ImFont*> cache;
    const std::string key = file.string() + "@" + std::to_string(size);
    if (auto it = cache.find(key); it != cache.end())
        return it->second;

    // a path is taken as given, anything else is a family name to look up
    std::error_code ec;
    std::string resolved = formatPath(file);
    if (!std::filesystem::is_regular_file(resolved, ec)) {
        resolved = resolveFontFile(file.string());
        if (resolved.empty())
            spdlog::error("[code] no font matching \"{}\"", file.string());
        else
            spdlog::debug("[code] font \"{}\" -> {}", file.string(), resolved);
    }

    ImFont* font = nullptr;
    if (!resolved.empty() && ImGui::GetCurrentContext()) {
        font = ImGui::GetIO().Fonts->AddFontFromFileTTF(resolved.c_str(), size);
        if (!font)
            spdlog::error("[code] could not load font {}", resolved);
    }
    cache[key] = font;
    return font;
}

std::vector<Code::HighlightRun> Code::HighlightRuns(const std::string& text,
                                                    const CodeLanguage& lang,
                                                    const CodeStyle& style)
{
    std::vector<HighlightRun> out;
    if (text.empty() || !lang.valid())
        return out;

    TSQuery* query = queryFor(lang.name);
    const Grammar* g = grammarNamed(lang.name);
    if (!query || !g)
        return out;

    TSParser* parser = ts_parser_new();
    ts_parser_set_language(parser, static_cast<const TSLanguage*>(g->fn()));
    TSTree* tree = ts_parser_parse_string(parser, nullptr, text.c_str(),
                                          uint32_t(text.size()));
    TSQueryCursor* cursor = ts_query_cursor_new();
    ts_query_cursor_exec(cursor, query, ts_tree_root_node(tree));

    struct Raw { size_t begin, end; Tok tok; };
    std::vector<Raw> raw;
    TSQueryMatch match;
    while (ts_query_cursor_next_match(cursor, &match)) {
        for (uint16_t c = 0; c < match.capture_count; ++c) {
            const TSQueryCapture& cap = match.captures[c];
            uint32_t len = 0;
            const char* name = ts_query_capture_name_for_id(query, cap.index, &len);
            const Tok tok = tokenOfCapture(std::string_view(name, len));
            if (tok == Tok::Plain)
                continue;
            size_t b = ts_node_start_byte(cap.node);
            size_t e = ts_node_end_byte(cap.node);
            if (e > b && e <= text.size())
                raw.push_back({b, e, tok});
        }
    }
    ts_query_cursor_delete(cursor);
    ts_tree_delete(tree);
    ts_parser_delete(parser);

    if (raw.empty())
        return out;

    // paint the widest captures first, then the narrower (more specific) ones
    // on top, so a token inside a larger node keeps its own colour
    std::sort(raw.begin(), raw.end(), [](const Raw& a, const Raw& b) {
        return (a.end - a.begin) > (b.end - b.begin);
    });
    std::vector<signed char> paint(text.size(), -1);
    for (const auto& r : raw)
        for (size_t i = r.begin; i < r.end; ++i)
            paint[i] = static_cast<signed char>(r.tok);

    auto colorOf = [&](Tok tok) -> ImU32 {
        Color col = style.text;
        switch (tok) {
            case Tok::Keyword:  col = style.keyword;  break;
            case Tok::Type:     col = style.type;     break;
            case Tok::Comment:  col = style.comment;  break;
            case Tok::Literal:  col = style.literal;  break;
            case Tok::Preproc:  col = style.preproc;  break;
            case Tok::Function: col = style.function; break;
            case Tok::Constant: col = style.constant; break;
            case Tok::Variable: col = style.variable; break;
            case Tok::Operator: col = style.op;       break;
            case Tok::Plain:    break;
        }
        return ImU32(ImColor(col.getImColor()));
    };

    // coalesce equal neighbours into runs
    for (size_t i = 0; i < paint.size();) {
        if (paint[i] < 0) { ++i; continue; }
        size_t j = i + 1;
        while (j < paint.size() && paint[j] == paint[i])
            ++j;
        out.push_back({i, j, colorOf(static_cast<Tok>(paint[i]))});
        i = j;
    }
    return out;
}

void Code::ClearAllCues()
{
    // file_backed misses the inline ones, so this walks every primitive
    for (const auto& p : Primitive::primitives)
        if (auto c = std::dynamic_pointer_cast<Code>(p))
            c->clearCues();
}

std::vector<path> Code::WatchedFiles()
{
    std::vector<path> out;
    for (auto* c : file_backed) {
        if (c->source_file.empty()) continue;
        std::error_code ec;
        auto v = std::filesystem::weakly_canonical(c->source_file, ec);
        if (ec) v = c->source_file;
        if (std::find(out.begin(), out.end(), v) == out.end())
            out.push_back(v);
    }
    return out;
}

void Code::HotReloadIfModified()
{
    static auto last_refresh = Time::now();
    if (TimeFrom(last_refresh) < 0.2)
        return;
    last_refresh = Time::now();

    for (auto* c : file_backed) {
        std::error_code ec;
        auto t = std::filesystem::last_write_time(c->source_file, ec);
        if (ec || t == c->last_modified)
            continue;
        spdlog::info("[code] reloading {}", c->source_file.string());
        c->reloadFromFile();
    }
}

void Code::highlight(int first_line, int last_line)
{
    hl_first = std::max(1, first_line);
    hl_last  = std::min<int>(last_line, int(lines.size()));
    if (hl_last < hl_first)
        clearHighlight();
}

void Code::highlight(const std::string& region)
{
    auto it = regions.find(region);
    if (it == regions.end()) {
        spdlog::warn("[code] no region named '{}'", region);
        clearHighlight();
        return;
    }
    highlight(it->second.first, it->second.second);
}

// Each cue records what a slide asks for, keyed by the slide being composed.

SlideCue Code::reveal(CodeAnchor where)
{
    const auto id = pid;   // resolved at call time, nothing kept alive here
    const bool all = (where == END);
    return {[id, all](int slide) {
        auto c = Primitive::get<Code>(id);
        c->reveal_at[slide] = all ? int(c->lines.size()) : 0;
    }};
}

// counted from the top of the listing as loaded, not from the file
SlideCue Code::reveal(int line)
{
    const auto id = pid;
    return {[id, line](int slide) {
        auto c = Primitive::get<Code>(id);
        c->reveal_at[slide] = std::clamp(line, 0, int(c->lines.size()));
    }};
}

SlideCue Code::reveal(const std::string& label)
{
    const auto id = pid;
    return {[id, label](int slide) {
        auto c = Primitive::get<Code>(id);
        const int p = c->pointOf(label);
        if (p >= 0)   // an unknown label leaves the slide as it was
            c->reveal_at[slide] = p;
    }};
}

SlideCue Code::focus(const std::string& region)
{
    const auto id = pid;
    return {[id, region](int slide) {
        auto c = Primitive::get<Code>(id);
        auto it = c->regions.find(region);
        if (it == c->regions.end()) {
            spdlog::warn("[code] no region named '{}'", region);
            c->focus_at[slide] = {0, 0};
            return;
        }
        c->focus_at[slide] = it->second;
    }};
}

// the two labels may be given in either order
SlideCue Code::focus(const std::string& from, const std::string& to)
{
    const auto id = pid;
    return {[id, from, to](int slide) {
        auto c = Primitive::get<Code>(id);
        int a = c->pointOf(from), b = c->pointOf(to);
        if (a < 0 || b < 0) { c->focus_at[slide] = {0, 0}; return; }
        if (b < a) std::swap(a, b);
        // a point sits above the line that follows it
        c->focus_at[slide] = {a + 1, b};
    }};
}

SlideCue Code::focus(int first_line, int last_line)
{
    const auto id = pid;
    return {[id, first_line, last_line](int slide) {
        auto c = Primitive::get<Code>(id);
        const int n = int(c->lines.size());
        const int a = std::clamp(first_line, 1, std::max(n, 1));
        const int b = std::clamp(last_line, 1, std::max(n, 1));
        c->focus_at[slide] = b < a ? std::pair<int,int>{0, 0}
                                   : std::pair<int,int>{a, b};
    }};
}

SlideCue Code::unfocus()
{
    const auto id = pid;
    return {[id](int slide) { Primitive::get<Code>(id)->focus_at[slide] = {0, 0}; }};
}

vec2 Code::getSize() const
{
    if (lines.empty())
        return vec2(0, 0);
    auto* font = fontOf(style);
    const float fs = baseSizeOf(style) * style.font_scale;

    float w = 0;
    for (const auto& l : lines) {
        const char* b = l.text.c_str();
        w = std::max(w, runOf(nullptr, font, fs, ImVec2(0,0), 0,
                              b, b + l.text.size(), style.tracking));
    }
    w += gutterWidth(font, fs);

    const float h = lineHeight() * float(lines.size());
    return vec2(w + 2*style.padding, h + 2*style.padding);
}

void Code::display(const StateInSlide& sis, float global_alpha)
{
    if (lines.empty())
        return;

    auto* dl   = ImGui::GetWindowDrawList();
    auto* font = fontOf(style);
    const float fsize = baseSizeOf(style);
    const float scale = float(sis.getScale());
    const float fs    = fsize * style.font_scale * scale;
    const float lh    = fsize * style.font_scale * style.line_spacing * scale;
    const float pad   = style.padding * scale;

    // getSize() is unscaled; the slide state's scale is applied here so the
    // wheel-zoom in the drag editor works on code like any other primitive
    const vec2 base = getSize();
    const ImVec2 size(float(base(0)) * scale, float(base(1)) * scale);

    const auto P = sis.getAbsolutePosition();
    const ImVec2 origin(P.x - size.x*0.5f, P.y - size.y*0.5f);

    auto withAlpha = [&](const Color& c, float a) {
        ImVec4 v = c.getImColor();
        v.w *= a * global_alpha;
        return ImU32(ImColor(v));
    };

    if (style.background.getValue().w > 0.f)
        dl->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y),
                          withAlpha(style.background, 1.f), 6.f * scale);

    const float gutter = gutterWidth(font, fs);
    const size_t digits = lines.empty() ? 0
                        : std::to_string(lineNumberOf(lines.size()-1)).size();

    // band_first/band_last are the first/last lit lines as (possibly
    // fractional) 1-based coordinates, interpolated by updateFromShow
    const bool has_band = focus_amt > 0.001f && band_last >= band_first;

    // smooth membership of line `n` in the band, ramping over one line at each
    // edge so a moving band dims/undims lines gradually instead of popping
    auto membership = [&](int n) -> float {
        if (n <= band_first) return std::clamp(1.f - (band_first - n), 0.f, 1.f);
        if (n >= band_last)  return std::clamp(1.f - (n - band_last),  0.f, 1.f);
        return 1.f;
    };

    auto writtenOf = [&](int line_no) -> int {
        if (written_chars.size() != lines.size())
            return -1;
        return written_chars[size_t(line_no-1)];
    };
    // the band must not light lines that are not written yet
    const bool band_written = band_last < band_first
                            || writtenOf(std::clamp(int(band_last), 1, int(lines.size()))) != 0;

    if (has_band && band_written) {
        // one rect spanning the whole animated range, opacity following focus
        const float top = origin.y + pad + lh * (band_first - 1.f);
        const float bot = origin.y + pad + lh * band_last;
        dl->AddRectFilled(ImVec2(origin.x + pad*0.4f, top),
                          ImVec2(origin.x + size.x - pad*0.4f, bot),
                          withAlpha(style.highlight, focus_amt));
    }

    for (size_t i = 0; i < lines.size(); ++i) {
        const int line_no = int(i) + 1;
        // dim unfocused lines, scaled by how focused we are (so unfocus fades
        // the dimming back to full brightness)
        const float m = has_band ? membership(line_no) : 1.f;
        const float a = 1.f - focus_amt * (1.f - m) * (1.f - style.dim_factor);

        const float y = origin.y + pad + lh * float(i);
        const float x = origin.x + pad + gutter;

        if (style.line_numbers) {
            // right aligned by padding, so 9 and 10 share their last column
            auto num = std::to_string(lineNumberOf(i));
            if (num.size() < digits)
                num.insert(0, digits - num.size(), ' ');
            runOf(dl, font, fs, ImVec2(origin.x + pad, y),
                  withAlpha(style.line_number, a),
                  num.c_str(), num.c_str() + num.size(), style.tracking);
        }

        // spans are sparse : whatever they do not cover is plain text
        const auto& text = lines[i].text;
        // -1 is whole, otherwise characters past the indentation
        const int written = writtenOf(line_no);
        const size_t limit = written < 0 ? text.size()
                                         : std::min(text.size(), indentOf(text) + size_t(written));
        if (written == 0) {
            // the head is at the start of the line, the caret is all there is
            if (line_no == caret_line) {
                const float w = std::max(1.f, fs * 0.06f);
                dl->AddRectFilled(ImVec2(x + w, y), ImVec2(x + 2*w, y + fs),
                                  withAlpha(style.text, a * 0.75f));
            }
            continue;
        }

        size_t cursor = 0;
        float pen = x;
        auto emit = [&](size_t b, size_t e, Tok tok) {
            e = std::min(e, limit);
            if (b >= e) return;
            const char* p0 = text.c_str() + b;
            const char* p1 = text.c_str() + e;
            Color col = style.text;
            switch (tok) {
                case Tok::Keyword:  col = style.keyword;  break;
                case Tok::Type:     col = style.type;     break;
                case Tok::Comment:  col = style.comment;  break;
                case Tok::Literal:  col = style.literal;  break;
                case Tok::Preproc:  col = style.preproc;  break;
                case Tok::Function: col = style.function; break;
                case Tok::Constant: col = style.constant; break;
                case Tok::Variable: col = style.variable; break;
                case Tok::Operator: col = style.op;       break;
                case Tok::Plain:    break;
            }
            pen += runOf(dl, font, fs, ImVec2(pen, y), withAlpha(col, a),
                         p0, p1, style.tracking);
        };
        for (const auto& s : lines[i].spans) {
            emit(cursor, s.begin, Tok::Plain);
            emit(s.begin, s.end, s.tok);
            cursor = s.end;
        }
        emit(cursor, text.size(), Tok::Plain);

        // pen is left just past the last glyph drawn, so it is the write head
        if (line_no == caret_line) {
            const float w = std::max(1.f, fs * 0.06f);
            dl->AddRectFilled(ImVec2(pen + w, y), ImVec2(pen + 2*w, y + fs),
                              withAlpha(style.text, a * 0.75f));
        }
    }
}

// A listing kept across a slide change never calls playIntro, only draw.
void Code::draw(const TimeObject& t, const StateInSlide& sis)
{
    updateFromShow(t);
    display(sis, float(sis.alpha));
}

void Code::playIntro(const TimeObject& t, const StateInSlide& sis)
{
    updateFromShow(t);
    display(sis, float(sis.alpha));
}

void Code::playOutro(const TimeObject& t, const StateInSlide& sis)
{
    updateFromShow(t);
    display(sis, float(sis.alpha));
}

}
