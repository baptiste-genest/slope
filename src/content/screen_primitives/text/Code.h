#ifndef CODE_H
#define CODE_H

#include "libslope.h"
#include "content/screen_primitives/ScreenPrimitive.h"
#include "content/authoring/color_tools.h"
#include <filesystem>

namespace slope {

/*
 * Source code as a screen primitive.
 *
 * Drawn with ImGui rather than through the LaTeX pipeline, so line positions
 * are known exactly and a reload costs no pdflatex round-trip. Highlighting
 * comes from tree-sitter, see cmake/treesitter.cmake.
 *
 *   auto code = Code::FromFile("newton.py");
 *   show << code->at("listing") << code->reveal(START);
 *   show << inNextFrame << code->reveal("loop") << code->focus("loop");
 *
 * A file may carry named regions and points, stripped from what is shown:
 *
 *   # slope:begin relax
 *   for v in verts: ...
 *   # slope:end relax
 *   # slope:here done
 */
class Code;
using CodePtr = std::shared_ptr<Code>;

// Languages are declared with slope_language() in cmake/treesitter.cmake.
struct CodeLanguage {
    std::string name;

    bool valid() const { return !name.empty(); }

    static const CodeLanguage& PlainText();
    static const CodeLanguage& ForName(const std::string& name);
    // extension without the dot, e.g. "py"
    static void Register(const std::string& extension, const CodeLanguage& lang);
    static const CodeLanguage& ForExtension(const std::string& extension);
    static std::vector<std::string> Available();
};

// defined by the cmake-generated Grammars.cpp
void RegisterDeclaredGrammars();

// Named colours are live in the Tuner. A literal Color opts one listing out.
struct CodeStyle {
    Color text        = Color("code/text",        ColorType(0.10f, 0.10f, 0.12f, 1.f));
    Color keyword     = Color("code/keyword",     ColorType(0.60f, 0.15f, 0.55f, 1.f));
    Color type        = Color("code/type",        ColorType(0.15f, 0.35f, 0.70f, 1.f));
    Color comment     = Color("code/comment",     ColorType(0.45f, 0.50f, 0.45f, 1.f));
    Color literal     = Color("code/literal",     ColorType(0.65f, 0.30f, 0.10f, 1.f));
    Color preproc     = Color("code/preproc",     ColorType(0.35f, 0.45f, 0.35f, 1.f));
    Color function    = Color("code/function",    ColorType(0.20f, 0.35f, 0.60f, 1.f));
    Color constant    = Color("code/constant",    ColorType(0.55f, 0.35f, 0.10f, 1.f));
    Color variable    = Color("code/variable",    ColorType(0.10f, 0.10f, 0.12f, 1.f));
    Color op          = Color("code/operator",    ColorType(0.35f, 0.35f, 0.40f, 1.f));
    Color line_number = Color("code/line_number", ColorType(0.65f, 0.65f, 0.68f, 1.f));
    Color highlight   = Color("code/highlight",   ColorType(1.00f, 0.85f, 0.30f, 0.35f));
    Color background  = Color("code/background",  ColorType(0.00f, 0.00f, 0.00f, 0.00f));

    // null falls back on Options::CodeFont, then on polyscope's monospace font
    ImFont* font       = nullptr;
    float font_scale   = 2.2f;  // on top of the slide state's scale
    // multiplies each glyph's advance, 1 keeps the font's own metrics
    float tracking     = 1.0f;
    float line_spacing = 1.15f;
    float padding      = 12.f;  // pixels, around the text block
    bool  line_numbers = false;
    // numbers a file-backed listing by its lines in the file rather than from
    // 1. Display only, reveal and focus stay relative to the loaded portion
    bool  absolute_line_numbers = false;
    // lines outside the highlighted range are dimmed to this factor; 1 keeps
    // them fully opaque and only draws the highlight band
    float dim_factor   = 0.35f;
};

enum CodeAnchor { START, END };

class Code : public TextualPrimitive
{
public:
    Code() {}

    CodeStyle style;

    // language defaults to plain text for inline sources, and to whatever the
    // file extension maps to for file-backed ones
    static CodePtr Add(const std::string& source,
                       const CodeLanguage& lang = CodeLanguage::PlainText());
    static CodePtr FromFile(const path& file);
    static CodePtr FromFile(const path& file, const CodeLanguage& lang);
    // lines first..last of the file, 1-based, last <= 0 for "to the end"
    static CodePtr FromFile(const path& file, int first_line, int last_line);
    static CodePtr FromFile(const path& file, int first_line, int last_line,
                            const CodeLanguage& lang);
    // only the part between the two marker lines, markers excluded
    static CodePtr FromFile(const path& file,
                            const std::string& begin_marker,
                            const std::string& end_marker);
    static CodePtr FromFile(const path& file,
                            const std::string& begin_marker,
                            const std::string& end_marker,
                            const CodeLanguage& lang);

    void setLanguage(const CodeLanguage& lang);

    // Sets the highlight on the primitive itself, i.e. on every slide showing
    // it. 1-based, inclusive; an empty range means "no highlight".
    void highlight(int first_line, int last_line);
    void highlight(const std::string& region);
    void clearHighlight() { hl_first = hl_last = 0; }

    /*
     *   show << typed;
     *   show << typed->reveal(START);
     *   show << inNextFrame << typed->reveal("loop");
     *   show << inNextFrame << typed->focus("loop");
     *
     * A slide with no cue keeps the last one set.
     */

    // a label, the end of a region, START / END, or a line of the listing
    SlideCue reveal(CodeAnchor where);
    SlideCue reveal(const std::string& label);
    SlideCue reveal(int line);

    // a region, a span between two labels, or a line range
    SlideCue focus(const std::string& region);
    SlideCue focus(const std::string& from, const std::string& to);
    SlideCue focus(int first_line, int last_line);
    SlideCue unfocus();

    bool hasRegion(const std::string& name) const { return regions.contains(name); }
    bool hasPoint(const std::string& name) const { return points.contains(name); }

    // re-reads any file-backed Code whose source changed on disk
    static void HotReloadIfModified();

    // every source file currently watched for hot reload. Absolute, de-duplicated.
    static std::vector<path> WatchedFiles();

    // loads a ttf into the atlas once and hands back the same font after that
    static ImFont* LoadFont(const path& file, float size = 18.f);

    // Tree-sitter highlight of an arbitrary buffer, for callers that draw their
    // own text (the in-app file editor). Runs are disjoint, sorted, and cover
    // only the coloured spans; the gaps between them are default text. `color`
    // is a packed ImU32 resolved from the live CodeStyle palette.
    struct HighlightRun { size_t begin, end; ImU32 color; };
    static std::vector<HighlightRun> HighlightRuns(const std::string& text,
                                                   const CodeLanguage& lang,
                                                   const CodeStyle& style = CodeStyle());

    // a deck recomposes the whole show on every reload, so the cues of the
    // previous composition have to go with it
    void clearCues() { reveal_at.clear(); focus_at.clear(); }
    static void ClearAllCues();

    // opaque so this header stays clear of tree-sitter
    using GrammarFn = const void* (*)();
    static void RegisterGrammar(const std::string& name, GrammarFn grammar,
                                const std::vector<std::string>& extensions);

    vec2 getSize() const override;

    void draw(const TimeObject& t, const StateInSlide& sis) override;
    void playIntro(const TimeObject& t, const StateInSlide& sis) override;
    void playOutro(const TimeObject& t, const StateInSlide& sis) override;

private:
    enum class Tok { Plain, Keyword, Type, Comment, Literal, Preproc,
                     Function, Constant, Variable, Operator };
    static Tok tokenOfCapture(std::string_view capture);
    struct Span { size_t begin, end; Tok tok; };
    struct Line { std::string text; std::vector<Span> spans; int file_line; };

    std::vector<Line> lines;
    std::map<std::string, std::pair<int,int>> regions; // name -> 1-based [first,last]
    CodeLanguage language;

    int hl_first = 0, hl_last = 0; // target highlight, 0 means no highlight

    // label -> number of lines before it. A region also leaves X.begin, X.end
    std::map<std::string, int> points;

    // filled when a slide is composed
    std::map<int, int> reveal_at;                    // slide -> point
    std::map<int, std::pair<int,int>> focus_at;      // slide -> lit line range

    // typed characters of lines 1..p, so a point becomes a write budget
    std::vector<float> unit_prefix;

    // characters written past the indentation, -1 for a whole line
    std::vector<int> written_chars;
    int caret_line = -1;

    // a line break costs this much budget, so it reads as a beat
    static constexpr float kNewlineCost = 6.f;

    void buildPoints();
    static int typedChars(const std::string& text);
    static size_t indentOf(const std::string& text);
    // -1 if unknown. A region resolves to its end
    int pointOf(const std::string& label) const;
    int revealOn(int slide) const;
    bool focusOn(int slide, int& first, int& last) const;
    void updateFromShow(const TimeObject& t);

    // interpolated between the cues of the two slides, never accumulated
    float band_first = 0, band_last = 0;
    float focus_amt = 0;                 // 0 unfocused .. 1 focused, drives dim+band

    // file backing, for hot reload
    path source_file;
    std::string begin_marker, end_marker;
    int slice_first = 0, slice_last = 0;   // 1-based line range, 0 for unset
    int source_base = 1;                   // file line of the first line loaded
    std::filesystem::file_time_type last_modified;
    bool from_file = false;

    // base_line is the file line the source starts on, for the gutter
    void setSource(const std::string& source, int base_line = 1);
    int lineNumberOf(size_t i) const;
    float gutterWidth(ImFont* font, float fs) const;
    void reloadFromFile();
    void display(const StateInSlide& sis, float global_alpha);
    float lineHeight() const;

    inline static std::vector<Code*> file_backed;
};

}

#endif // CODE_H
