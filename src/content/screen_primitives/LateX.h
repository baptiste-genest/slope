#ifndef LATEX_H
#define LATEX_H

#include "../../libslope.h"
#include "Image.h"
#include "../color_tools.h"
#include "../io.h"
#include "../Options.h"
#include "LateXMacros.h"
#include <future>

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
void GenerateLatexBatch(const std::vector<LatexJob>& jobs);

struct Latex;
using LatexPtr = std::shared_ptr<Latex>;


struct LatexLoader {
    using key = std::string;

    static path source_path;
    static json source;
    static std::map<key,LatexPtr> loaded;
    static std::filesystem::file_time_type source_last_modified;
    static bool initialized;

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

    // watches the file-backed prefix parts; when one changed on disk,
    // rebuilds the context and re-renders every latex primitive
    static void HotReloadPrefixIfModified();

    static void rebuildContext();
    static void RegenerateAll();

    // compiles every primitive queued by MakeObject in one pdflatex run
    static void FlushPending();
    static std::vector<LatexPtr> pending;

    // adopts the images of a finished background batch ; called every frame
    static void PumpBatch();
    static std::future<void> batch_future;
    static std::vector<LatexPtr> batch_targets;


    bool isFormula;
    scalar scale;
    int width = -1;
    ImageData data;
    TexObject tex_source; // the exact tex passed in (content may differ, cf Title)
    std::string full_content;

    // opt-in : setColor renders the glyphs white and multiplies them by `color`
    // at draw time, so recoloring costs no recompilation. It stays off by
    // default because the multiply would turn any \textcolor of the source black
    bool tintable = false;
    Color color = Color(1.f,1.f,1.f,1.f);

    // distance in image pixels from the top of the png to the tex baseline,
    // -1 when unknown. Formulas are placed on it instead of on the centre of
    // their ink, so that e and e^{a^{b^{c}}} sit on the same line
    double baseline = -1;
    bool alignOnBaseline = true;

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


    // Primitive interface
public:
    Latex() {}
    ~Latex() {}

    static scalar getNormalizationFactor() {
        return 800./slope::Options::PDFtoPNGDensity*0.45;
    }

    void display(const StateInSlide& sis) const{
        FlushPending();
        if (data.width == -1)
            return;
        anchor->updatePos(sis.getPosition());
        scalar s = scale*getNormalizationFactor()*sis.getScale();
        DisplayImage(data,sis,s,
                     tintable ? color.getImColor() : RGBA(1.f,1.f,1.f,1.f),
                     baselineOffset()*s);
    }

    virtual void draw(const TimeObject &time, const StateInSlide &sis) override {
        display(sis);
    }
    virtual void playIntro(const TimeObject &t, const StateInSlide &sis) override {
        display(transition.intro(t,sis));
    }
    virtual void playOutro(const TimeObject &t, const StateInSlide &sis) override {
        display(transition.outro(t,sis));
    }

    // ScreenPrimitive interface
public:
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

inline LatexPtr Title(TexObject s,bool center = true) {
    auto old = s;
    if (center)
        s = tex::center(s);
    auto rslt = Latex::Add(s,Options::TitleScale);
    rslt->exclusive = true;
    rslt->content = old;
    return rslt;
}



}

#endif // LATEX_H
