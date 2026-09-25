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
 * It is drawn with ImGui and not with LaTeX, so line positions are exact
 * and a reload does not need pdflatex. Highlighting comes from tree-sitter, see cmake/treesitter.cmake.
 *
 *   auto code = Code::FromFile("newton.py");
 *   show << code->at("code") << code->reveal(START);
 *   show << inNextFrame << code->reveal("loop") << code->focus("loop");
 *
 * A file can hold named regions and points. They are removed from the displayed text.
 *
 *   # slope:begin relax
 *   for v in verts: ...
 *   # slope:end relax
 *   # slope:here done
 */
class Code;
using CodePtr = std::shared_ptr<Code>;

// A language for highlighting. Languages are declared with slope_language() in cmake/treesitter.cmake.
struct CodeLanguage {
    std::string name;

    // True when the language exists.
    bool valid() const { return !name.empty(); }

    // No highlighting.
    static const CodeLanguage& PlainText();
    // Language with this name.
    static const CodeLanguage& ForName(const std::string& name);
    // Links a file extension, written without the dot as in "py", to a language.
    static void Register(const std::string& extension, const CodeLanguage& lang);
    // Language linked to an extension.
    static const CodeLanguage& ForExtension(const std::string& extension);
    // Names of the declared languages.
    static std::vector<std::string> Available();
};

// Registers the declared grammars. It is defined by Grammars.cpp, which cmake generates.
void RegisterDeclaredGrammars();

// Colors and layout of a code block.
// Named colors can be tuned in the Tuner. A fixed Color removes a block from the tuning.
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

    // When null, Options::CodeFont is used, then the monospace font of polyscope.
    ImFont* font       = nullptr;
    // Applied on top of the scale of the slide state.
    float font_scale   = 2.2f;
    // Multiplies the advance of each glyph. 1 keeps the metrics of the font.
    float tracking     = 1.0f;
    float line_spacing = 1.15f;
    // Space around the text, in pixels.
    float padding      = 12.f;
    bool  line_numbers = false;
    // For a block loaded from a file, numbers the lines as in the file instead of from 1.
    // It only changes the display. Reveal and focus still count from the loaded part.
    bool  absolute_line_numbers = false;
    // Opacity of the lines outside the highlighted range. 1 keeps them fully visible and only draws the highlight band.
    float dim_factor   = 0.35f;
};

// Start or end of a code block, used by reveal.
enum CodeAnchor { START, END };

class Code : public TextualPrimitive
{
public:
    Code() {}

    CodeStyle style;

    // Builds a code block from a string. The default language is plain text.
    // For a file, the default language comes from the extension.
    static CodePtr Add(const std::string& source,
                       const CodeLanguage& lang = CodeLanguage::PlainText());
    // Builds a code block from a file, which is reloaded when it changes.
    static CodePtr FromFile(const path& file);
    static CodePtr FromFile(const path& file, const CodeLanguage& lang);
    // Only the lines from first_line to last_line, counted from 1. A last_line of 0 or less means the end of the file.
    static CodePtr FromFile(const path& file, int first_line, int last_line);
    static CodePtr FromFile(const path& file, int first_line, int last_line,
                            const CodeLanguage& lang);
    // Only the part between two marker lines, without the markers.
    static CodePtr FromFile(const path& file,
                            const std::string& begin_marker,
                            const std::string& end_marker);
    static CodePtr FromFile(const path& file,
                            const std::string& begin_marker,
                            const std::string& end_marker,
                            const CodeLanguage& lang);

    // Changes the language and highlights again.
    void setLanguage(const CodeLanguage& lang);

    // Highlights lines on the primitive itself, so on every slide that shows it.
    // Lines are counted from 1 and both ends are included. An empty range removes the highlight.
    void highlight(int first_line, int last_line);
    void highlight(const std::string& region);
    // Removes the highlight.
    void clearHighlight() { hl_first = hl_last = 0; }

    /*
     *   show << typed;
     *   show << typed->reveal(START);
     *   show << inNextFrame << typed->reveal("loop");
     *   show << inNextFrame << typed->focus("loop");
     *
     * A slide with no cue keeps the last one set.
     */

    // Reveal writes the block progressively up to a point.
    // A point is a label, the end of a region, START, END or a line number.
    // The cues below apply from the slide where they are added.
    SlideCue reveal(CodeAnchor where);
    SlideCue reveal(const std::string& label);
    SlideCue reveal(int line);

    // Focus highlights a region, the span between two labels or a line range, and dims the other lines.
    SlideCue focus(const std::string& region);
    SlideCue focus(const std::string& from, const std::string& to);
    SlideCue focus(int first_line, int last_line);
    // Removes the focus.
    SlideCue unfocus();

    // True when the code defines this region.
    bool hasRegion(const std::string& name) const { return regions.contains(name); }
    // True when the code defines this point.
    bool hasPoint(const std::string& name) const { return points.contains(name); }

    // Reads again the code blocks whose file changed.
    static void HotReloadIfModified();

    // Absolute paths of the source files that are watched, with no duplicate.
    static std::vector<path> WatchedFiles();

    // Loads a ttf file into the font atlas the first time, and returns the same font on later calls.
    static ImFont* LoadFont(const path& file, float size = 18.f);

    // Highlights any text with tree-sitter, for code that draws its own text, such as the file editor.
    // The runs do not overlap, are sorted and cover only the colored spans.
    // The text between two runs has the default color.
    // The color is a packed ImU32 taken from the current CodeStyle.
    struct HighlightRun { size_t begin, end; ImU32 color; };
    static std::vector<HighlightRun> HighlightRuns(const std::string& text,
                                                   const CodeLanguage& lang,
                                                   const CodeStyle& style = CodeStyle());

    // Removes the cues of this block. A deck builds the whole show again at every reload,
    // so the cues of the previous build must be removed.
    void clearCues() { reveal_at.clear(); focus_at.clear(); }
    // Removes the cues of every block.
    static void ClearAllCues();

    // Function returning the tree-sitter grammar. Its type is opaque so this header does not include tree-sitter.
    using GrammarFn = const void* (*)();
    // Declares a language with its grammar and the file extensions it applies to.
    static void RegisterGrammar(const std::string& name, GrammarFn grammar,
                                const std::vector<std::string>& extensions);

    // Size in pixels.
    vec2 getSize() const override;

    void draw(const TimeObject& t, const StateInSlide& sis) override;
    void playIntro(const TimeObject& t, const StateInSlide& sis) override;
    void playOutro(const TimeObject& t, const StateInSlide& sis) override;

private:
    // Kind of a colored token.
    enum class Tok { Plain, Keyword, Type, Comment, Literal, Preproc,
                     Function, Constant, Variable, Operator };
    // Token kind for a tree-sitter capture name.
    static Tok tokenOfCapture(std::string_view capture);
    // A range of characters in a line with its token kind.
    struct Span { size_t begin, end; Tok tok; };
    struct Line { std::string text; std::vector<Span> spans; int file_line; };

    std::vector<Line> lines;
    // Named regions, each as the first and last line counted from 1.
    std::map<std::string, std::pair<int,int>> regions;
    CodeLanguage language;

    // Target highlight. 0 means no highlight.
    int hl_first = 0, hl_last = 0;

    // Number of lines before each label. A region also defines X.begin and X.end.
    std::map<std::string, int> points;

    // Set when a slide is composed.
    // Point to reveal on each slide.
    std::map<int, int> reveal_at;
    // Line range to focus on each slide.
    std::map<int, std::pair<int,int>> focus_at;

    // Number of typed characters in lines 1 to p, which turns a point into an amount of typing.
    std::vector<float> unit_prefix;

    // Characters written after the indentation of each line, or -1 for a whole line.
    std::vector<int> written_chars;
    int caret_line = -1;

    // Typing cost of a line break, so it reads as a pause.
    static constexpr float kNewlineCost = 6.f;

    // Fills points and unit_prefix.
    void buildPoints();
    // Number of characters typed for a line.
    static int typedChars(const std::string& text);
    // Number of indentation characters of a line.
    static size_t indentOf(const std::string& text);
    // Number of lines before a label, or -1 if unknown. A region gives its end.
    int pointOf(const std::string& label) const;
    // Point revealed on a slide.
    int revealOn(int slide) const;
    // Gives the focused lines of a slide. Returns false when there is no focus.
    bool focusOn(int slide, int& first, int& last) const;
    // Updates the reveal and focus for the current time.
    void updateFromShow(const TimeObject& t);

    // Current band, computed between the cues of the two slides and never accumulated.
    float band_first = 0, band_last = 0;
    // From 0 for unfocused to 1 for focused. It sets the dimming and the band.
    float focus_amt = 0;

    // Data used to read the file again.
    path source_file;
    std::string begin_marker, end_marker;
    // Line range counted from 1, 0 when unset.
    int slice_first = 0, slice_last = 0;
    // Line of the file that holds the first loaded line.
    int source_base = 1;
    std::filesystem::file_time_type last_modified;
    bool from_file = false;

    // Sets the source text. base_line is the line of the file where it starts, used for the line numbers.
    void setSource(const std::string& source, int base_line = 1);
    // Number shown for line i.
    int lineNumberOf(size_t i) const;
    // Width of the column of line numbers.
    float gutterWidth(ImFont* font, float fs) const;
    // Reads the file again.
    void reloadFromFile();
    // Draws the block with opacity global_alpha.
    void display(const StateInSlide& sis, float global_alpha);
    // Height of a line in pixels.
    float lineHeight() const;

    // Every block that reads from a file.
    inline static std::vector<Code*> file_backed;
};

}

#endif // CODE_H
