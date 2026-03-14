#ifndef SLIDERS_H
#define SLIDERS_H

#include "../primitive.h"

namespace slope {


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

    virtual bool isScreenSpace() const override {return false;}

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
};

Sliders::SlidersPtr AddIntSliders(int nb, const std::string &label,int default_val, const std::pair<float, float> &bounds);
Sliders::SlidersPtr AddFloatSliders(int nb, const std::string &label,float default_val, const std::pair<float, float> &bounds);


}

#endif // SLIDERS_H
