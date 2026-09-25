#ifndef IMGUIWIDGETS_H
#define IMGUIWIDGETS_H

#include "content/core/primitive.h"

namespace slope {

// Runs a function that draws ImGui widgets while the slide is shown.
class ImGuiWidgets : public Primitive
{
    public:
    using WidgetPtr = std::shared_ptr<ImGuiWidgets>;
    // Function called at every frame with the time.
    using callback = std::function<void(TimeObject t)>;

private:

    callback func;
    std::string label;

    // Primitive interface
public:

    // Widgets drawn by f in a window with this title.
    ImGuiWidgets(const callback& f,const std::string& title) : func(f),label(title)  {
    }

    virtual void draw(const TimeObject &time, const StateInSlide &sis) override;

    virtual void playIntro(const TimeObject &t, const StateInSlide &sis) override {}
    virtual void playOutro(const TimeObject &t, const StateInSlide &sis) override {}

    // Builds the primitive, from a function that takes the time or a function that takes nothing.
    static ImGuiWidgets::WidgetPtr Add(const callback& f,const std::string& title="") {
        return NewPrimitive<ImGuiWidgets>(f,title);
    }
    static ImGuiWidgets::WidgetPtr Add(const std::function<void()>& f,const std::string& title="") {
        return NewPrimitive<ImGuiWidgets>([f](TimeObject){f();},title);
    }

    // Primitive interface
public:
    bool isScreenSpace() const override {return false;}
};

// A window with several sliders that share a range, all integers or all floats.
class Sliders : public Primitive
{
public:

private:
    std::vector<int> int_vals;
    std::vector<float> float_vals;
    bool isInteger;
    std::string label;
    int nb_sliders;
    std::pair<float,float> bounds;

    // Primitive interface
public:
    using SlidersPtr = std::shared_ptr<Sliders>;


    // Sliders with initial values, a label and a range given as (min, max).
    Sliders(const std::vector<int> &int_vals, const std::string &label, const std::pair<float, float> &bounds);
    Sliders(const std::vector<float> &float_vals, const std::string &label, const std::pair<float, float> &bounds);
    virtual void draw(const TimeObject &time, const StateInSlide &sis) override;
    virtual void playIntro(const TimeObject &t, const StateInSlide &sis) override {}
    virtual void playOutro(const TimeObject &t, const StateInSlide &sis) override {}

    // Value of slider i.
    float getVal(int i) const {
        if (isInteger)
            return int_vals[i];
        else
            return float_vals[i];
    }

    // Sets the value of slider i.
    void setVal(int index,float v) {
        if (isInteger)
            int_vals[index] = v;
        else
            float_vals[index] = v;

    }
    // Primitive interface
public:
    bool isScreenSpace() const override {return false;}
};

// Builds nb integer sliders that all start at default_val.
Sliders::SlidersPtr AddIntSliders(int nb, const std::string &label,int default_val, const std::pair<float, float> &bounds);
// Builds nb float sliders that all start at default_val.
Sliders::SlidersPtr AddFloatSliders(int nb, const std::string &label,float default_val, const std::pair<float, float> &bounds);


}

#endif // IMGUIWIDGETS_H
