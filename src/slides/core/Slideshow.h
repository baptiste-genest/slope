#ifndef SLIDESHOW_H
#define SLIDESHOW_H

#include "slides/core/Slide.h"
#include "slides/prompter/PrompterModule.h"
#include "slides/capture/screenshot.h"
#include "content/screen_primitives/text/Text.h"
#include "slides/ui/CLI.h"

// must come before GLFW/glfw3.h, their constant definitions overlap
#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#include "glad/glad.h"
#endif
#include "GLFW/glfw3.h"
#include "slides/ui/WindowManager.h"
#include "slides/core/PlaybackState.h"
#include "slides/ui/DragEditor.h"
#include "slides/capture/TimeTracker.h"
#include "slides/capture/CameraExporter.h"
#include "slides/ui/HUD.h"

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

    // rebuilds the slide structure at runtime. Disables what is shown and the
    // given stale primitives, clears the slides, re-runs the composer and
    // restores the playback position. The primitives themselves are untouched.
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
    // How far the slide change has got, 0 to 1, the deck wide reading of what
    // renderSlide hands each primitive. 1 whenever nothing is in transition.
    parameter transitionProgress(TimeTypeSec t) const;
    // records when the current slide was first reached and forgets the ones
    // after it, so secondsSinceKeyframe restarts when the show is rewound
    void noteSlideArrival();
    std::map<int, TimeTypeSec> slide_times;
    void prompt();
    void handleTransition();

    // called once per frame, so every primitive shares the same delta
    inline TimeObject getTimeObject() const {
        TimeObject T;
        T.from_begin = TimeFrom(from_begin);
        T.from_action = TimeFrom(state.from_action);
        T.absolute_frame_number = state.current;
        T.delta_time = TimeFrom(last_frame);
        last_frame = Time::now();
        TimeObject::keyframes = &keyframes;
        TimeObject::slide_times = &slide_times;
        return T;
    }

    std::string getSlideTitle(int slide_nb);
    TimeTypeSec transitionTime = 0.5;

    void initializeSlides();

    void exportPDF();

    void loadSlides();

    PlaybackState state;

    WindowManager wm;
    TimeStamp from_begin;
    mutable TimeStamp last_frame = Time::now();
    ImGuiWindowFlags window_flags = 0;

    void renderSlide(TimeTypeSec t, Slide& CS, TimeObject& T);

    // resolves the current slide's background, lerping out of the previous one
    void updateBackground(parameter transition);

    void transformEditor();
    void clearTransformGizmos();
    int gizmo_slide = -1;

    void handleInputs();

    void handleGuizmos();

    bool inGizmoMode() const;


    void displayPopUps();

    static void ImGuiWindowConfig();
    static void onWindowClose(GLFWwindow* w);



    InputManager input_manager;
    void addKeyboardInputs();

    bool help_wanted = false;
};


}

#endif // SLIDESHOW_H
