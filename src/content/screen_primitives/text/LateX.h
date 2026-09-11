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


void GenerateLatex(const path& filename,const TexObject& texcontent);
std::string WriteTexFile(const TexObject& tex,bool formula, int width = -1,bool white = false);
std::string TexPreamble(bool white = false);
std::string TexBody(const TexObject& tex,bool formula,int width = -1);
path GetLatexPath(const TexObject& tex);
path BaselinePath(const path& png);
double ReadBaseline(const path& png);
std::string Tail(const path& p,std::size_t n);

struct LatexJob {
    path png;
    std::string body;
    bool white;
};
// png -> the log of its failed compile; a broken preamble fails them all at once
struct LatexBatchResult {
    std::map<path,std::string> failed;
    std::string preamble_error;
};
LatexBatchResult GenerateLatexBatch(const std::vector<LatexJob>& jobs);

struct Latex;
using LatexPtr = std::shared_ptr<Latex>;


struct LatexLoader {
    using key = std::string;

    static path source_path;
    static json source;
    static std::map<key,LatexPtr> loaded;
    static std::filesystem::file_time_type source_last_modified;
    static bool initialized;
    // bumped on every successful reload, watched by the deck loader
    static int generation;

    static void Init(path P);
    static LatexPtr Load(key k);

    static ScreenPrimitiveInSlide LoadWithAnchor(key k);

    static void parseJson();

    static void ReloadContentAndUpdate();

    static void HotReloadIfModified();

    static int GetWidth(const json& content) {
        if (!content.is_array())
            throw std::runtime_error("Latex source object is not an array");
        if (content.size() > 2)
            return content[2];
        return -1;
    }

};


struct Latex : public TextualPrimitive {

    static LatexPtr Add(const TexObject& tex,scalar scale = 1,int width = -1);

    static LatexPtr MakeObject(const TexObject& tex,scalar scale = 1,int width = -1,bool formula = false);

    // the prefix (shared preamble) is kept as an ordered list of parts,
    // either literal strings or file references, so that file-backed parts
    // (e.g. commands.tex) can be watched and hot-reloaded at runtime;
    // `context` is the assembled result used by WriteTexFile
    static TexObject context;

    struct ContextPart {
        bool is_file;
        TexObject value; // literal content, or path for file parts
        std::filesystem::file_time_type last_modified;
    };
    static std::vector<ContextPart> context_parts;

    static void AddToPrefix(const TexObject& tex) {
        context_parts.push_back({false,tex,{}});
        context += tex;
    }
    static void Define(const TexObject& tex) {AddToPrefix(tex);}
    static void DeclareMathOperator(const TexObject& name,const TexObject& content);
    static void NewCommand(const TexObject& name,const TexObject& content) {
        AddToPrefix("\\newcommand{\\"+name+"}{"+content+"}");
    }
    static void NewCommand(const TexObject& name,const TexObject& content,int nb_arg) {
        AddToPrefix("\\newcommand{\\"+name+"}" + "[" + std::to_string(nb_arg) + "]{"+content+"}");
    }

    static void UsePackage(std::string pkg,std::string options = "") {
        if (options != "")
            AddToPrefix("\\usepackage["+options+"]{"+pkg+"}\n");
        else
            AddToPrefix("\\usepackage{"+pkg+"}\n");
    }

    static void AddFileToPrefix(const path& p);

    // the deck's inline "preamble:", added after the file-backed prefix parts
    static TexObject deck_prefix;
    static void SetDeckPrefix(const TexObject& tex);

    // watches the file-backed prefix parts; when one changed on disk,
    // rebuilds the context and re-renders every latex primitive
    static void HotReloadPrefixIfModified();

    static void rebuildContext();
    static void RegenerateAll();
    // asked while a batch runs, RegenerateAll waits for PumpBatch instead of blocking
    static bool regenerate_again;

    // the latex source json and the file-backed prefix parts, for the file editor
    static std::vector<path> WatchedFiles();
    // where errors of tex with no origin of its own are reported, the deck
    static path default_origin;
    // hands every compile_error to ReloadErrors, grouped by origin
    static void PublishErrors();

    // compiles every primitive queued by MakeObject in one pdflatex run
    static void FlushPending();
    static std::vector<LatexPtr> pending;

    // adopts the images of a finished background batch ; called every frame
    static void PumpBatch();
    static std::future<LatexBatchResult> batch_future;
    static std::vector<LatexPtr> batch_targets;


    bool isFormula;
    scalar scale;
    int width = -1;
    ImageData data;
    TexObject tex_source; // the exact tex passed in (content may differ, cf Title)
    std::string full_content;
    path origin;                // the file the tex was written in, if any
    std::string compile_error;  // of the last compile of full_content, "" if it built

    // opt-in. setColor renders the glyphs white and multiplies them by `color`
    // at draw time, which would turn any \textcolor of the source black
    bool tintable = false;
    Color color = Color(1.f,1.f,1.f,1.f);

    // distance in image pixels from the top of the png to the tex baseline,
    // -1 when unknown. Formulas are placed on it instead of on the centre of
    // their ink, so that e and e^{a^{b^{c}}} sit on the same line
    double baseline = -1;
    bool alignOnBaseline = true;

    // fraction of the png the texture actually holds, one per axis
    double tex_sx = 1, tex_sy = 1;
    // the png on screen, which refills re-read even after a failed compile
    path texture_png;
    // a refill that failed stops asking until the next loadTexture
    bool texels_failed = false;

    // the shrink ensureTexelsFor is waiting on, and since when
    double settling_need = -1;
    TimeStamp settling_since = Time::now();

    double baselineOffset() const {
        if (baseline < 0 || !isFormula || !alignOnBaseline)
            return 0;
        return data.height*0.5 - baseline;
    }

    void setColor(const Color& c) {
        color = c;
        tintable = true;
        ensureRendered();
    }

    void updateContent(json content);

    // renders whatever the current tex_source/context produce, reusing the
    // on-disk cache when possible ; a latex failure leaves the primitive
    // without an image rather than propagating
    void ensureRendered();

    void regenerate() {ensureRendered();}

    // rebuilds tex_source from the current context, before a prefix reload compiles it
    virtual void refreshSource() {}


    // Primitive interface
public:
    Latex() {}
    ~Latex() {}

    static scalar getNormalizationFactor() {
        return 800./slope::Options::PDFtoPNGDensity*0.45;
    }

    // the factor the png is drawn at, which is what the texture is stored at so
    // that the sampler never has to minify glyphs
    std::pair<double,double> drawScale() const {
        double sx = scale*getNormalizationFactor(), sy = sx;
        if (Options::ScreenResolutionWidth != 1920 || Options::ScreenResolutionHeight != 1080){
            sx *= Options::ScreenResolutionWidth/1920.;
            sy *= Options::ScreenResolutionHeight/1080.;
        }
        return {sx,sy};
    }

    // refills from the png when the draw size and the texel count drift apart
    void ensureTexelsFor(double sx,double sy);

    // refills the texture from the png, reduced k times more than now
    void reloadTexels(double k);

    // reloadTexels if it is safe now, otherwise on the next render pass
    void requestTexels(double k);

    // reloadTexels deletes the live texture, so it is only safe while the
    // frame's draw list holds no reference to it yet. True only inside
    // Slideshow::renderSlide; anywhere else the reload waits for the next one.
    static bool in_render_pass;
    // the reload ensureTexelsFor asked for out of pass, composed
    double deferred_texels = 0;

    // uploads the png at the size it will be drawn at
    void loadTexture(const path& png);

    void display(const StateInSlide& sis) {
        FlushPending();
        if (data.width == -1)
            return;
        // taken before anything references the texture this frame
        if (deferred_texels > 0 && in_render_pass) {
            const double k = deferred_texels;
            deferred_texels = 0;
            reloadTexels(k);
        }
        anchor->updatePos(sis.getPosition());
        scalar s = scale*getNormalizationFactor()*sis.getScale();
        if (sis.hasPlane()){
            // the warp decides the on screen size, not the slide scale
            scalar pw,ph;
            // data.width is the png's own size, so this is the fraction of it
            // the screen asks for, the same currency drawScale uses below
            if (PlaneScreenExtent(sis,data,s,pw,ph) && data.width > 0 && data.height > 0)
                ensureTexelsFor(pw/data.width,ph/data.height);
        }
        else {
            auto [dx,dy] = drawScale();
            ensureTexelsFor(dx*sis.getScale(),dy*sis.getScale());
        }
        DisplayImage(data,sis,s,
                     tintable ? color.getImColor() : RGBA(1.f,1.f,1.f,1.f),
                     getDrawOffset()(1)*sis.getScale());
    }

    virtual void draw(const TimeObject &time, const StateInSlide &sis) override {
        display(sis);
    }
    virtual void playIntro(const TimeObject &t, const StateInSlide &sis) override {
        display(sis);
    }
    virtual void playOutro(const TimeObject &t, const StateInSlide &sis) override {
        display(sis);
    }

    // ScreenPrimitive interface
public:
    bool canRotate() const override {return true;}

    // a formula is drawn on its baseline instead of on the centre of its ink,
    // so that e and e^{a^{b^{c}}} placed at the same label sit on the same line
    virtual vec2 getDrawOffset() const override {
        FlushPending();
        return vec2(0,baselineOffset()*scale*getNormalizationFactor());
    }

    virtual vec2 getSize() const override {
        FlushPending();
        bool notfullHD = (Options::ScreenResolutionWidth != 1920) ||(Options::ScreenResolutionHeight != 1080);
        if (notfullHD){
            double sx =  Options::ScreenResolutionWidth/1920.;
            double sy =  Options::ScreenResolutionHeight/1080.;
            return vec2(sx*data.width,sy*data.height)*scale*getNormalizationFactor();
        }
        return vec2(data.width,data.height)*scale*getNormalizationFactor();
    }
};

struct Formula : public Latex {
    static LatexPtr Add(const TexObject& tex,scalar scale = 1.,int width = -1);
};

inline LatexPtr Title(TexObject s,bool center = true,scalar scale = -1) {
    auto old = s;
    if (center)
        s = tex::center(s);
    auto rslt = Latex::Add(s,scale < 0 ? Options::TitleScale : scale);
    rslt->exclusive = true;
    rslt->content = old;
    return rslt;
}



}

#endif // LATEX_H
