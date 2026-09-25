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
#include "slides/ui/FileEditor.h"

namespace slope {

// The window, the playback of the slides and the tools around them.
class Slideshow : public PrompterModule
{
public:
    
    Slideshow() {}

    // Goes to the next slide with a transition.
    void nextFrame();

    // Goes to the previous slide with a transition.
    void previousFrame();

    // Goes to the next slide with no transition.
    void forceNextFrame();

    // Draws the current slide. Called once per frame.
    void play();

    // Sets the duration of a transition in seconds.
    void setTransitionTimeSecond(TimeTypeSec s) {
        transitionTime = s;
    }

    // Creates the window and reads the command line.
    void init(std::string project_name,int argc,char** argv);

    // Jumps to a slide with no transition.
    void goToSlide(int slide_nb);

    bool display_slide_number = true;
    // Shows a notice in the corner when a reload failed.
    bool display_reload_errors = true;

    // Runs the show until the window is closed.
    void run();

    // True when the command line asked for the help text.
    bool helpWanted() const {return help_wanted;}

    // Builds the slides again while the show runs.
    // It hides what is shown and the given stale primitives, clears the slides, runs the composer
    // and goes back to the previous slide. The primitives themselves are not changed.
    // If the composer throws, the fallback composes instead and the function returns false.
    bool recompose(const std::function<void(SlideManager&)>& composer,
                   const std::set<PrimitivePtr>& stale = {},
                   const std::function<void(SlideManager&)>& fallback = {});

    // Called once per frame at the end of play(). It can watch external sources such as deck.yaml
    // or generated data, and compose the slides again when they change.
    std::function<void()> onFrame;

    // Puts a new unused name on the clipboard. It is written as "at: <name>" when as_placement is true,
    // and as the bare name otherwise, which can also be used as an "id:".
    void copyLabelSuggestion(bool as_placement = false);

private:

    DragEditor drag_editor;
    TimeTracker time_tracker;
    CameraExporter camera_exporter;
    HUD hud;
    FileEditor file_editor;
    // Slide where the editor was last opened.
    int editor_jump_slide = -1;

    // Updates the inner time of the primitives.
    void setInnerTime();
    // Progress of the slide change from 0 to 1, seen from the whole deck. It is what renderSlide gives each primitive.
    // It is 1 when there is no transition.
    parameter transitionProgress(TimeTypeSec t) const;
    // Records when the current slide was first reached and forgets the later ones,
    // so secondsSinceKeyframe starts again when the show is rewound.
    void noteSlideArrival();
    std::map<int, TimeTypeSec> slide_times;
    // Shows the prompter.
    void prompt();
    // Advances the transition, and starts the next slide when a pause ends.
    void handleTransition();

    // Builds the TimeObject of the frame. It is called once per frame, so every primitive shares the same delta.
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

    // Title of a slide.
    std::string getSlideTitle(int slide_nb);
    TimeTypeSec transitionTime = 0.5;

    // Prepares the slides for playing.
    void initializeSlides();

    // Exports one image per slide as a PDF.
    void exportPDF();

    // Exports images taken during every slide change, and not only at its ends.
    void exportTransitions();

    // Records a continuous sequence of frames of the whole deck and encodes it to <ProjectName>.mp4.
    void recordVideo();

    // Builds the slides.
    void loadSlides();

    PlaybackState state;

    WindowManager wm;
    TimeStamp from_begin;
    mutable TimeStamp last_frame = Time::now();
    ImGuiWindowFlags window_flags = 0;

    // Draws a slide at time t, with the TimeObject of the frame.
    void renderSlide(TimeTypeSec t, Slide& CS, TimeObject& T);

    // Finds the background of the current slide, blending from the previous one during a transition.
    void updateBackground(parameter transition);

    // Shows the panel that edits the transform of the selected label.
    void transformEditor();
    // Removes the transform gizmos.
    void clearTransformGizmos();
    // Slide for which the gizmos were made.
    int gizmo_slide = -1;

    // Reads the keyboard and the mouse.
    void handleInputs();

    // Draws the gizmos.
    void handleGuizmos();

    // True when a gizmo is being used.
    bool inGizmoMode() const;

    // Draws the pop-ups.
    void displayPopUps();

    // Configures the flags of the ImGui window.
    static void ImGuiWindowConfig();
    // Called when the window is closed, to save the edits.
    static void onWindowClose(GLFWwindow* w);

    InputManager input_manager;
    // Adds the keyboard shortcuts.
    void addKeyboardInputs();

    bool help_wanted = false;
};


}

#endif // SLIDESHOW_H
