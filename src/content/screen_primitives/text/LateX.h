#ifndef LATEX_H
#define LATEX_H

#include "libslope.h"
#include "content/screen_primitives/media/Image.h"
#include "content/authoring/color_tools.h"
#include "content/config/io.h"
#include "content/config/Options.h"
#include "content/screen_primitives/text/LateXMacros.h"
#include <future>
#include "extern/json.hpp"

namespace slope {

// Compiles LaTeX source into an image at the given path.
void GenerateLatex(const path& filename, const TexObject& texcontent);
// Writes the .tex file for a source and returns its path. A formula is set in math mode.
// The width is in pt, and -1 means no width. With white, the text is white.
std::string WriteTexFile(const TexObject& tex, bool formula, int width = -1, bool white = false);
// Preamble shared by every object.
std::string TexPreamble(bool white = false);
// Document body of an object, without the preamble.
std::string TexBody(const TexObject& tex, bool formula, int width = -1);
// Path of the image of a source in the cache.
path GetLatexPath(const TexObject& tex);
// Path of the file that stores the baseline of an image.
path BaselinePath(const path& png);
// Baseline of an image in pixels from the top, or -1 when unknown.
double ReadBaseline(const path& png);
// Last n characters of a file, used to show the end of a compile log.
std::string Tail(const path& p, std::size_t n);

// One image to compile.
struct LatexJob {
    path png;
    std::string body;
    bool white;
};
// Result of a batch. It maps the image of each failed job to the log of its compile.
// A broken preamble makes every job fail at once.
struct LatexBatchResult {
    std::map<path, std::string> failed;
    std::string preamble_error;
};
// Compiles several jobs together.
LatexBatchResult GenerateLatexBatch(const std::vector<LatexJob>& jobs);

struct Latex;
using LatexPtr = std::shared_ptr<Latex>;

// Loads LaTeX objects from a json file. Each key maps to an array with 0 or 1 for text or formula, then the source, then an optional width.
struct LatexLoader {
    using key = std::string;

    static path source_path;
    static json source;
    static std::map<key, LatexPtr> loaded;
    static std::filesystem::file_time_type source_last_modified;
    static bool initialized;
    // Increases at every successful reload. The deck loader watches it.
    static int generation;

    // Sets the json file and reads it.
    static void Init(path P);
    // Builds the object stored under a key.
    static LatexPtr Load(key k);

    // Builds the object and places it at a label with the same name.
    static ScreenPrimitiveInSlide LoadWithAnchor(key k);

    // Reads the json file.
    static void parseJson();

    // Reads the file again and updates the loaded objects.
    static void ReloadContentAndUpdate();

    // Reloads when the file changed.
    static void HotReloadIfModified();

    // Width given in a json entry, or -1.
    static int GetWidth(const json& content) {
        if (!content.is_array())
            throw std::runtime_error("Latex source object is not an array");
        if (content.size() > 2)
            return content[2];
        return -1;
    }
};

// A text or formula compiled with pdflatex and drawn as an image.
struct Latex : public TextualPrimitive {

    // Builds a text object. The width is in pt, and -1 means no width.
    static LatexPtr Add(const TexObject& tex, scalar scale = 1, int width = -1);

    // Builds an object and queues its compilation, done by FlushPending.
    static LatexPtr MakeObject(const TexObject& tex, scalar scale = 1, int width = -1, bool formula = false);

    // The prefix is the preamble shared by all objects.
    // It is a list of parts, each either a string or a file, so file parts such as commands.tex can be watched and reloaded.
    // `context` is the assembled prefix used by WriteTexFile.
    static TexObject context;

    // One part of the prefix.
    struct ContextPart {
        bool is_file;
        // The text, or the path for a file part.
        TexObject value;
        std::filesystem::file_time_type last_modified;
    };
    static std::vector<ContextPart> context_parts;

    // Adds LaTeX source to the prefix. The other prefix functions below are shortcuts for it.
    static void AddToPrefix(const TexObject& tex) {
        context_parts.push_back({false, tex, {}});
        context += tex;
    }
    static void Define(const TexObject& tex) { AddToPrefix(tex); }
    // Declares a math operator with a name and its text.
    static void DeclareMathOperator(const TexObject& name, const TexObject& content);
    // Defines a command, with nb_arg arguments in the second form.
    static void NewCommand(const TexObject& name, const TexObject& content) {
        AddToPrefix("\\newcommand{\\" + name + "}{" + content + "}");
    }
    static void NewCommand(const TexObject& name, const TexObject& content, int nb_arg) {
        AddToPrefix("\\newcommand{\\" + name + "}" + "[" + std::to_string(nb_arg) + "]{" + content + "}");
    }

    // Loads a package, with optional options.
    static void UsePackage(std::string pkg, std::string options = "") {
        if (options != "")
            AddToPrefix("\\usepackage[" + options + "]{" + pkg + "}\n");
        else
            AddToPrefix("\\usepackage{" + pkg + "}\n");
    }

    // Adds a file to the prefix, which is reloaded when the file changes.
    static void AddFileToPrefix(const path& p);
    // Removes a file from the prefix.
    static void RemoveFileFromPrefix(const path& p);

    // The "preamble:" of the deck, added after the file parts of the prefix.
    static TexObject deck_prefix;
    // Sets the preamble of the deck.
    static void SetDeckPrefix(const TexObject& tex);

    // Watches the file parts of the prefix.
    // When one changed, it builds the context again and renders every LaTeX object again.
    static void HotReloadPrefixIfModified();

    // Assembles the context from the parts of the prefix.
    static void rebuildContext();
    // Renders every LaTeX object again.
    static void RegenerateAll();
    // Set when a regeneration is asked during a batch. It then waits for PumpBatch instead of blocking.
    static bool regenerate_again;

    // The json source and the file parts of the prefix, for the file editor.
    static std::vector<path> WatchedFiles();
    // Origin used to report errors of source that has no file of its own. It is the deck.
    static path default_origin;
    // Gives every compile_error to ReloadErrors, grouped by origin.
    static void PublishErrors();

    // Compiles every object queued by MakeObject in a single pdflatex run.
    static void FlushPending();
    static std::vector<LatexPtr> pending;

    // Takes the images of a finished background batch. Called every frame.
    static void PumpBatch();
    static std::future<LatexBatchResult> batch_future;
    static std::vector<LatexPtr> batch_targets;

    bool isFormula;
    scalar scale;
    int width = -1;
    ImageData data;
    // The source exactly as given. The content can differ, as for Title.
    TexObject tex_source;
    // Source with the prefix, as compiled.
    std::string full_content;
    // The file where the source was written, if any.
    path origin;
    // Error of the last compile of full_content, empty when it succeeded.
    std::string compile_error;

    // Off by default. setColor renders the glyphs in white and multiplies them by `color` when drawing,
    // so any \textcolor of the source would become black.
    bool tintable = false;
    Color color = Color(1.f, 1.f, 1.f, 1.f);

    // Distance in pixels from the top of the png to the baseline of the text, or -1 when unknown.
    // A formula is placed by its baseline and not by the center of its ink,
    // so that e and e^{a^{b^{c}}} sit on the same line.
    double baseline = -1;
    bool alignOnBaseline = true;

    // Fraction of the png held by the texture, for each axis.
    double tex_sx = 1, tex_sy = 1;
    // The png shown on screen. Reloading the texture reads it again, even after a failed compile.
    path texture_png;
    // Set when a reload of the texture failed. No more reloads are tried until the next loadTexture.
    bool texels_failed = false;

    // Reduction that ensureTexelsFor is waiting for, and since when.
    double settling_need = -1;
    TimeStamp settling_since = Time::now();

    // Vertical shift in image pixels that puts the baseline where the center would be.
    double baselineOffset() const {
        if (baseline < 0 || !isFormula || !alignOnBaseline)
            return 0;
        return data.height * 0.5 - baseline;
    }

    // Sets a color and turns tintable on.
    void setColor(const Color& c) {
        color = c;
        tintable = true;
        ensureRendered();
    }

    // Replaces the source with a json entry, as described in LatexLoader.
    void updateContent(json content);

    // Renders the current source with the current context, using the cache when possible.
    // A LaTeX failure leaves the object without an image and does not throw.
    void ensureRendered();

    // Renders again.
    void regenerate() { ensureRendered(); }

    // Builds the source again from the current context, before a reload of the prefix compiles it.
    virtual void refreshSource() {}

    // Primitive interface
public:
    Latex() {}
    ~Latex() {}

    // Factor between png pixels and screen pixels for a full HD window.
    static scalar getNormalizationFactor() {
        return 800. / slope::Options::PDFtoPNGDensity * 0.45;
    }

    // Factors along x and y at which the png is drawn.
    // The texture is stored at this size, so the sampler never has to shrink glyphs.
    std::pair<double, double> drawScale() const {
        double sx = scale * getNormalizationFactor(), sy = sx;
        if (Options::ScreenResolutionWidth != 1920 || Options::ScreenResolutionHeight != 1080) {
            sx *= Options::ScreenResolutionWidth / 1920.;
            sy *= Options::ScreenResolutionHeight / 1080.;
        }
        return {sx, sy};
    }

    // Reads the texture again from the png when the drawing size and the number of texels no longer match.
    void ensureTexelsFor(double sx, double sy);

    // Reads the texture again from the png, reduced k times more than now.
    void reloadTexels(double k);

    // Calls reloadTexels now if it is safe, or else at the next render pass.
    void requestTexels(double k);

    // reloadTexels deletes the current texture, so it is safe only while the draw list of the frame does not use it yet.
    // This is true only inside Slideshow::renderSlide. Anywhere else the reload waits for the next one.
    static bool in_render_pass;
    // Reduction asked by ensureTexelsFor outside a render pass, waiting to be done.
    double deferred_texels = 0;

    // Uploads the png at the size it will be drawn.
    void loadTexture(const path& png);

    // Draws the image with the state of the slide.
    void display(const StateInSlide& sis) {
        FlushPending();
        if (data.width == -1)
            return;
        // Done before anything uses the texture in this frame.
        if (deferred_texels > 0 && in_render_pass) {
            const double k = deferred_texels;
            deferred_texels = 0;
            reloadTexels(k);
        }
        anchor->updatePos(sis.getPosition());
        scalar s = scale * getNormalizationFactor() * sis.getScale();
        if (sis.hasPlane()) {
            // The warp sets the size on screen, not the scale of the slide.
            scalar pw, ph;
            // data.width is the size of the png, so this is the fraction of it that the screen needs, as in drawScale below.
            if (PlaneScreenExtent(sis, data, s, pw, ph) && data.width > 0 && data.height > 0)
                ensureTexelsFor(pw / data.width, ph / data.height);
        } else {
            auto [dx, dy] = drawScale();
            ensureTexelsFor(dx * sis.getScale(), dy * sis.getScale());
        }
        DisplayImage(data, sis, s,
                     tintable ? color.getImColor() : RGBA(1.f, 1.f, 1.f, 1.f),
                     getDrawOffset()(1) * sis.getScale());
    }

    virtual void draw(const TimeObject& time, const StateInSlide& sis) override {
        display(sis);
    }
    virtual void playIntro(const TimeObject& t, const StateInSlide& sis) override {
        display(sis);
    }
    virtual void playOutro(const TimeObject& t, const StateInSlide& sis) override {
        display(sis);
    }

    // ScreenPrimitive interface
public:
    bool canRotate() const override { return true; }

    // A formula is drawn on its baseline and not on the center of its ink,
    // so that e and e^{a^{b^{c}}} placed at the same label sit on the same line.
    virtual vec2 getDrawOffset() const override {
        FlushPending();
        return vec2(0, baselineOffset() * scale * getNormalizationFactor());
    }

    // Size in pixels.
    virtual vec2 getSize() const override {
        FlushPending();
        bool notfullHD = (Options::ScreenResolutionWidth != 1920) || (Options::ScreenResolutionHeight != 1080);
        if (notfullHD) {
            double sx = Options::ScreenResolutionWidth / 1920.;
            double sy = Options::ScreenResolutionHeight / 1080.;
            return vec2(sx * data.width, sy * data.height) * scale * getNormalizationFactor();
        }
        return vec2(data.width, data.height) * scale * getNormalizationFactor();
    }
};

// A LaTeX formula, set in math mode.
struct Formula : public Latex {
    static LatexPtr Add(const TexObject& tex, scalar scale = 1., int width = -1);
};

// Builds a title, centered by default. A negative scale uses Options::TitleScale.
// The title replaces the previous title of a slide.
inline LatexPtr Title(TexObject s, bool center = true, scalar scale = -1) {
    auto old = s;
    if (center)
        s = tex::center(s);
    auto rslt = Latex::Add(s, scale < 0 ? Options::TitleScale : scale);
    rslt->exclusive = true;
    rslt->content = old;
    return rslt;
}

} // namespace slope

#endif // LATEX_H
