#include "Plot.h"

slope::Plot::Plot()
{
    static int count = 0;
    id = count++;
}

slope::PlotPtr slope::Plot::Add()
{
    auto rslt = slope::NewPrimitive<Plot>();
    return rslt;
}

slope::PlotPtr slope::Plot::Add(const PlotModule &m)
{
    auto rslt = slope::NewPrimitive<Plot>();
    rslt->modules.push_back(m);
    return rslt;
}

void slope::Plot::draw(const TimeObject &t, const StateInSlide &sis)
{
    display(t,sis);
}

void slope::Plot::playIntro(const TimeObject &t, const StateInSlide &sis)
{
    display(t,transition.intro(t,sis));
}

void slope::Plot::playOutro(const TimeObject &t, const StateInSlide &sis)
{
    display(t,transition.outro(t,sis));
}

void slope::Plot::display(const TimeObject &t, const StateInSlide &sis)
{
    int flags = 0;
    flags |= ImGuiWindowFlags_NoTitleBar;
    flags |= ImGuiWindowFlags_NoMove;
    flags |= ImGuiWindowFlags_NoResize;
    flags |= ImGuiWindowFlags_NoBackground;
    flags |= ImGuiWindowFlags_NoScrollbar;

    auto ap = sis.getAbsolutePosition();
    ImGui::SetNextWindowPos(ap);
    ImGui::SetNextWindowSize(ImVec2(500,300));

    ImGui::GetStyle().Alpha = sis.alpha;
    ImPlot::GetStyle().MinorAlpha = 0;

    ImGui::Begin(("test" + std::to_string(id)).c_str(),0,flags);

    ImPlot::StyleColorsLight();


    ImPlot::BeginPlot(("test" + std::to_string(id)).c_str(),ImVec2(-1,0),ImPlotFlags_CanvasOnly);

    //remove axes
    ImPlot::SetupAxes("x","y",ImPlotAxisFlags_NoDecorations,ImPlotAxisFlags_NoDecorations);

    for (auto& m : modules)
        m(t);
    ImPlot::EndPlot();
    ImGui::End();
}

slope::PlotModule slope::PlotLines(const scalars &X, const scalars &Y)
{
    return [X,Y] (TimeObject) {
        ImPlot::PlotLine("data",X.data(),Y.data(),X.size());
    };
}
