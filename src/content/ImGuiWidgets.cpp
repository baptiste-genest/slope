#include "ImGuiWidgets.h"

void slope::Sliders::draw(const TimeObject &time, const StateInSlide &sis)
{
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.WantCaptureMouse = true;
    //io.WantCaptureKeyboard = true;
    ImGui::Begin(label.c_str());
    if (isInteger){
        for (int i = 0;i<nb_sliders;i++)
            ImGui::SliderInt(std::to_string(i).c_str(),&int_vals[i],bounds.first,bounds.second);
    }
    else
        for (int i = 0;i<nb_sliders;i++)
            ImGui::SliderFloat(std::to_string(i).c_str(),&float_vals[i],bounds.first,bounds.second);
    ImGui::End();
}


namespace slope {
Sliders::SlidersPtr AddIntSliders(int nb, const std::string &label,int default_val, const std::pair<float, float> &bounds)
{
    std::vector<int> ints(nb,default_val);
    return NewPrimitive<Sliders>(ints,label,bounds);
}

Sliders::SlidersPtr AddFloatSliders(int nb, const std::string &label,float default_val, const std::pair<float, float> &bounds)
{
    std::vector<float> floats(nb,default_val);
    return NewPrimitive<Sliders>(floats,label,bounds);
}

Sliders::Sliders(const std::vector<int> &int_vals, const std::string &label, const std::pair<float, float> &bounds) : int_vals(int_vals),
    isInteger(true),
    label(label),
    bounds(bounds)
{
    nb_sliders = int_vals.size();
}

Sliders::Sliders(const std::vector<float> &float_vals, const std::string &label, const std::pair<float, float> &bounds) :
    float_vals(float_vals),
    isInteger(false),
    label(label),
    bounds(bounds)
{
    nb_sliders = float_vals.size();
}

void ImGuiWidgets::draw(const TimeObject &time, const StateInSlide &sis) {
    ImGui::Begin(label.c_str());
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.WantCaptureMouse = true;
    io.WantCaptureKeyboard = true;
    ImGui::SetWindowFocus();
    func(time);
    ImGui::End();
}

}
