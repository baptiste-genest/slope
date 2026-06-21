#ifndef SLIDESHOW_H
#define SLIDESHOW_H

#include "Slide.h"
#include "PrompterModule.h"
#include "screenshot.h"
#include "../content/screen_primitives/Text.h"
#include "../content/screen_primitives/plots/Plot.h"
#include "CLI.h"

#include "WindowManager.h"
#include "DragEditor.h"
#include "TimeTracker.h"
#include "CameraExporter.h"
#include "HUD.h"

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


    void prompt();

    void handleTransition();

    void init(std::string project_name,int argc,char** argv);

    std::string getSlideTitle(int slide_nb);

    void goToSlide(int slide_nb);

    bool display_slide_number = true;

    int getRelativeSlideNumber(Primitive* p);

    void run();

    DragEditor drag_editor;
    TimeTracker time_tracker;
    CameraExporter camera_exporter;
    HUD hud;

private:
    TimeTypeSec transitionTime = 0.5;

    void initializeSlides();
    void computeFirstSlideNumbers();

    void exportPDF();

    void loadSlides();

    bool transition_done = false;
    bool backward = false;
    bool locked = true;

    bool pause_active = false;
    TimeStamp pause_start;

    bool halt_slope_display = false;

    WindowManager wm;

    void transformEditor();

    void handleInputs();

    void handleGuizmos();

    void displayPopUps();

    static void ImGuiWindowConfig();

    int visited_slide = -1;

    TimeStamp from_action,from_begin;
    size_t current_slide = 0;
    ImGuiWindowFlags window_flags = 0;



    InputManager input_manager;
    void addKeyboardInputs();

    bool help_wanted = false;
};


}

#endif // SLIDESHOW_H
