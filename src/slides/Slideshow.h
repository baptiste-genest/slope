#ifndef SLIDESHOW_H
#define SLIDESHOW_H

#include "Slide.h"
#include "PrompterModule.h"
#include "screenshot.h"
#include "../content/screen_primitives/Text.h"
#include "../content/screen_primitives/plots/Plot.h"
#include "CLI.h"

#include "WindowManager.h"

namespace slope {

class Slideshow : public PrompterModule
{
public:
    
    Slideshow() {}

    void nextFrame();

    void previousFrame();

    void forceNextFrame();

    void play();

    void setTransitionTimeSecond(TimeTypeSec s) {
        transitionTime = s;
    }

    void setInnerTime();


    inline TimeObject getTimeObject() const {
        TimeObject T;
        T.from_begin = TimeFrom(from_begin);
        T.from_action = TimeFrom(from_action);
        T.absolute_frame_number = current_slide;
        return T;
    }

    void handleDragAndDrop();

    void prompt();

    void handleTransition();

    void init(std::string project_name,int argc,char** argv);

    std::string getSlideTitle(int slide_nb);

    void goToSlide(int slide_nb);

    bool display_slide_number = true;

    int getRelativeSlideNumber(Primitive* p);

    void run();

private:
    TimeTypeSec transitionTime = 0.5;

    void saveCamera(std::string file);

    void slideMenu();

    void initializeSlides();
    void computeFirstSlideNumbers();

    void exportPDF();

    void loadSlides();
    std::vector<int> slide_numbers;
    std::vector<PrimitiveInSlide> slide_number_display;

    PrimitivePtr selected_primitive = nullptr;
    PrimitivePtr getPrimitiveUnderMouse(scalar x,scalar y) const;

    void displaySlideNumber();

    bool transition_done = false;
    bool backward = false;
    bool locked = true;

    bool halt_slope_display = false;

    WindowManager wm;

    void TransformEditor();

    void handleInputs();

    void handleGuizmos();

    void displayPopUps();

    static void ImGuiWindowConfig();

    int visited_slide = -1;
    int nb_distinct_slides = 0;

    TimeStamp from_action,from_begin;
    size_t current_slide = 0;
    ImGuiWindowFlags window_flags = 0;

    bool help_wanted = false;
};


}

#endif // SLIDESHOW_H
