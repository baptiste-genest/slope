#ifndef LATEX_H
#define LATEX_H

#include "../libslope.h"
#include "Image.h"
#include "io.h"
#include "Options.h"
#include "LateXMacros.h"

namespace slope {


void GenerateLatex(const path& filename,const TexObject& tex,bool formula);
path GetLatexPath(const TexObject& tex,bool formula);

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

};


struct Latex : public TextualPrimitive {

    static LatexPtr Add(const TexObject& tex,scalar scale = 1);

    static TexObject context;
    static void Define(const TexObject& tex) {context += tex;}
    static void AddToPrefix(const TexObject& tex) {context += tex;}
    static void DeclareMathOperator(const TexObject& name,const TexObject& content);
    static void NewCommand(const TexObject& name,const TexObject& content) {context += "\\newcommand{\\"+name+"}{"+content+"}";}
    static void NewCommand(const TexObject& name,const TexObject& content,int nb_arg) {
        context += "\\newcommand{\\"+name+"}" + "[" + std::to_string(nb_arg) + "]{"+content+"}";
    }

    static void UsePackage(std::string pkg,std::string options = "") {
        if (options != "")
            context += "\\usepackage["+options+"]{"+pkg+"}\n";
        else
            context += "\\usepackage{"+pkg+"}\n";
    }

    static void AddFileToPrefix(const path& p);


    bool isFormula;
    scalar scale;
    ImageData data;

    void updateContent(const TexObject& tex,scalar hr);


    // Primitive interface
public:
    Latex() {}
    ~Latex() {}

    static scalar getNormalizationFactor() {
        return 800./slope::Options::PDFtoPNGDensity*0.45;
    }

    void display(const StateInSlide& sis) const{
        anchor->updatePos(sis.getPosition());
        DisplayImage(data,sis,scale*getNormalizationFactor()*sis.getScale());
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
    static LatexPtr Add(const TexObject& tex,scalar scale = 1.);
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
