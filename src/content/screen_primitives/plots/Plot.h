#ifndef PLOT_H
#define PLOT_H

#include "../ScreenPrimitive.h"
#include "implot.h"
//#include "../../extern/implot.h"

namespace slope {

class Plot;
using PlotPtr = std::shared_ptr<Plot>;

using PlotModule = std::function<void(TimeObject)>;

class Plot : public ScreenPrimitive
{
public:
    Plot();

    static PlotPtr Add();
    static PlotPtr Add(const PlotModule& m);

    void addModule(const PlotModule& M);

protected:

    int id;

    void draw(const TimeObject &time, const StateInSlide &sis);
    void playIntro(const TimeObject &t, const StateInSlide &sis);
    void playOutro(const TimeObject &t, const StateInSlide &sis);

    void display(const TimeObject& t,const StateInSlide& sis);

    using Modules = std::vector<PlotModule>;

    Modules modules;

    // ScreenPrimitive interface
public:
    vec2 getSize() const {return vec2(500,300);}
};

PlotModule PlotLines(const scalars& X,const scalars& Y);


}

#endif // PLOT_H
