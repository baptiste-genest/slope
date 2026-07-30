#ifndef SLIDESHOW_H
#define SLIDESHOW_H

#include "Slide.h"
#include "PrompterModule.h"
#include "screenshot.h"
#include "../content/screen_primitives/Text.h"
#include "../content/screen_primitives/plots/Plot.h"
#include "CLI.h"

#include "GLFW/glfw3.h"
#include "WindowManager.h"
#include "PlaybackState.h"
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

    void init(std::string project_name,int argc,char** argv);

    void goToSlide(int slide_nb);

    bool display_slide_number = true;

    void run();

    bool helpWanted() const {return help_wanted;}

    // rebuilds the slide structure at runtime : disables what is currently
    // shown (plus the given stale primitives), clears the slides and re-runs
    // the composer, then restores the playback position. Primitives
    // themselves are untouched, so a composer reusing them keeps their state.
    void recompose(const std::function<void(SlideManager&)>& composer,
                   const std::set<PrimitivePtr>& stale = {});

    // called once per frame at the end of play(), e.g. to watch external
    // sources (deck manifest, generated data...) and recompose on change
    std::function<void()> onFrame;

private:

    DragEditor drag_editor;
    TimeTracker time_tracker;
    CameraExporter camera_exporter;
    HUD hud;

    void setInnerTime();
    void prompt();
    void handleTransition();

    inline TimeObject getTimeObject() const {
        TimeObject T;
        T.from_begin = TimeFrom(from_begin);
        T.from_action = TimeFrom(state.from_action);
        T.absolute_frame_number = state.current;
        T.delta_time = TimeFrom(last_frame);
        last_frame = Time::now();
        TimeObject::keyframes = &keyframes;
        return T;
    }

    std::string getSlideTitle(int slide_nb);
    TimeTypeSec transitionTime = 0.5;

    void initializeSlides();

    void exportPDF();

    void loadSlides();

    bool halt_slope_display = false;
    PlaybackState state;

    WindowManager wm;
    TimeStamp from_begin;
    mutable TimeStamp last_frame = Time::now();
    ImGuiWindowFlags window_flags = 0;

    void renderSlide(TimeTypeSec t, Slide& CS, TimeObject& T);

    void transformEditor();

    void handleInputs();

    void handleGuizmos();

    void displayPopUps();

    static void ImGuiWindowConfig();
    static void onWindowClose(GLFWwindow* w);



    InputManager input_manager;
    void addKeyboardInputs();

    bool help_wanted = false;
};


}

#endif // SLIDESHOW_H
