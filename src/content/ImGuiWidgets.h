#ifndef IMGUIWIDGETS_H
#define IMGUIWIDGETS_H

#include "primitive.h"

namespace slope {

class ImGuiWidgets : public Primitive
{
    public:
    using WidgetPtr = std::shared_ptr<ImGuiWidgets>;
    using callback = std::function<void(TimeObject t)>;

private:

    callback func;
    std::string label;

    // Primitive interface
public:

    ImGuiWidgets(const callback& f,const std::string& title) : func(f),label(title)  {
    }

    virtual void draw(const TimeObject &time, const StateInSlide &sis) override;

    virtual void playIntro(const TimeObject &t, const StateInSlide &sis) override {}
    virtual void playOutro(const TimeObject &t, const StateInSlide &sis) override {}

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


    Sliders(const std::vector<int> &int_vals, const std::string &label, const std::pair<float, float> &bounds);
    Sliders(const std::vector<float> &float_vals, const std::string &label, const std::pair<float, float> &bounds);
    virtual void draw(const TimeObject &time, const StateInSlide &sis) override;
    virtual void playIntro(const TimeObject &t, const StateInSlide &sis) override {}
    virtual void playOutro(const TimeObject &t, const StateInSlide &sis) override {}

    float getVal(int i) const {
        if (isInteger)
            return int_vals[i];
        else
            return float_vals[i];
    }

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

Sliders::SlidersPtr AddIntSliders(int nb, const std::string &label,int default_val, const std::pair<float, float> &bounds);
Sliders::SlidersPtr AddFloatSliders(int nb, const std::string &label,float default_val, const std::pair<float, float> &bounds);


}

#endif // IMGUIWIDGETS_H
