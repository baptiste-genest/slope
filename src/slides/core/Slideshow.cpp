#include "slides/core/Slideshow.h"
#include "content/config/ReloadErrors.h"
#include "content/screen_primitives/text/LateX.h"
#include "content/screen_primitives/text/Code.h"
#include "content/screen_primitives/text/Algorithm.h"
#include "content/screen_primitives/gpu/Shader.h"
#include "spdlog/spdlog.h"
#include "polyscope/pick.h"
#include "content/authoring/color_tools.h"
#include "content/authoring/Params.h"
#include "content/authoring/Snippet.h"
#include "ImGuizmo.h"
#include <spdlog/spdlog.h>
#include <cstdio>

// X11 comes in through GLFW and defines None, which eats TransparencyMode::None
#ifdef None
#undef None
#endif

void slope::Slideshow::nextFrame() {
    if (state.current == slides.size() - 1)
        return;
    state.goForward();
}

void slope::Slideshow::previousFrame() {
    if (!state.current)
        return;
    for (auto& s : uniqueNext(transitions[state.current - 1]))
        s->disable();
    for (auto& s : uniquePrevious(transitions[state.current - 1]))
        s->enable();
    state.goBackward();
    for (auto& s : slides[state.current]) {
        auto p = s.first;
        TimeObject t(p->getInnerTime(), 1);
        t.absolute_frame_number = state.current;
        p->intro(t, s.second);
    }
    slides[state.current].setCam(false); // going back is a jump too
}

void slope::Slideshow::forceNextFrame() {
    if (state.current == slides.size() - 1)
        return;
    for (auto& s : uniquePrevious(transitions[state.current]))
        s->disable();
    for (auto& s : uniqueNext(transitions[state.current]))
        s->enable();
    state.skip();
    for (auto& s : slides[state.current]) {
        auto p = s.first;
        TimeObject t(p->getInnerTime(), 1);
        t.absolute_frame_number = state.current;
        p->intro(t, s.second);
    }
    slides[state.current].setCam(false); // skipping, so no flight either
}

void slope::Slideshow::play() {

    handleGuizmos();

    Params::NewFrame();
    if (!wm.isOpen(WindowType::Tuner))
        Params::clearGizmos(); // no panel, no sync

    ImGuiWindowConfig();
    // this window's active id would block ImGuizmo from grabbing the gizmo
    ImGuiWindowFlags flags = window_flags;
    // ImGuizmo cannot grab a handle while this window holds an active id, so
    // the mouse is given up in gizmo mode, and under a shown handle
    if (inGizmoMode() || Params::cursorOnGizmo())
        flags |= ImGuiWindowFlags_NoMouseInputs;
    // polyscope wires the glfw clipboard on its first ImGui context only, not the one slope draws in
    ImGuiPlatformIO& pio = ImGui::GetPlatformIO();
    pio.Platform_SetClipboardTextFn = [](ImGuiContext*, const char* text) {
        glfwSetClipboardString(glfwGetCurrentContext(), text);
    };
    pio.Platform_GetClipboardTextFn = [](ImGuiContext*) {
        return glfwGetClipboardString(glfwGetCurrentContext());
    };
    ImGui::Begin("Slope", NULL, flags);

    if (!initialized)
        initializeSlides();

    auto t = TimeFrom(state.from_action);
    auto& CS = slides[state.current];
    setInnerTime();
    noteSlideArrival();
    TimeObject T = getTimeObject();
    // the deck wide reading, shared by every primitive whatever path draws it
    T.slide_progress = transitionProgress(t);
    TimeObject ST = T;
    ST.transition_parameter = T.slide_progress;
    Snippet::setTime(ST);

    updateBackground(T.slide_progress);

    // Depth peeling costs a full scene pass per layer, and alpha only drops
    // during transitions. Dropping it here lets polyscope turn it back on by
    // itself, which it does on any setTransparency below 1 or any transparency
    // quantity, both re-asserted every frame by the primitives that need them.
    polyscope::options::transparencyMode = polyscope::TransparencyMode::None;

    renderSlide(t, CS, T);

    prompt();

    if (state.isPaused()) {
        hud.drawPauseIndicator(TimeFrom(*state.pause_since), CS.pause_duration);
        if (TimeFrom(*state.pause_since) >= CS.pause_duration) {
            state.stopPause();
            nextFrame();
        }
    }

    handleInputs();
    time_tracker.record(getSlideTitle(state.current));

    if (LatexLoader::initialized)
        LatexLoader::HotReloadIfModified();
    Latex::HotReloadPrefixIfModified();
    Params::HotReloadIfModified();
    Shader::HotReloadIfModified();
    Code::HotReloadIfModified();
    Algorithm::HotReloadIfModified();
    Snippet::HotReloadIfModified();

    if (onFrame)
        onFrame();

    if (display_slide_number)
        hud.drawSlideNumber(state.current);

    if (inGizmoMode())
        hud.drawGizmoMode(wm.isOpen(WindowType::Transform) ? "transform" : "parameter");

    ImGui::End();

    // the editor already lists them, and an export never shows them
    if (display_reload_errors && !Options::ExportMode && !Options::RecordMode && !wm.isOpen(WindowType::FileEditor)) {
        const auto broken = hud.drawReloadErrors(ReloadErrors::all());
        if (!broken.empty() && !wm.isAnyOpen()) {
            wm.Toggle(WindowType::FileEditor);
            file_editor.open(broken);
        }
    }

    Params::DrawVisible(wm.isOpen(WindowType::Tuner));
    displayPopUps();

    // polyscope aims the gizmos at a window drawn first, hence under everything
    ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());
}

void slope::Slideshow::updateBackground(parameter transition) {
    if (slides.empty())
        return;
    // the deck-wide default, a palette entry like any other
    static Color fallback("background", ColorType(1, 1, 1, 1));
    auto resolve = [&](const Slide& s) -> Color {
        return s.background ? *s.background : fallback;
    };
    const Color current = resolve(slides[state.current]);
    // lerping from the slide being left, which a primitive could never see
    polyscope::view::bgColor =
        (state.current > 0 && transition < 1)
            ? Lerp(resolve(slides[state.current - 1]), current, transition).toArray()
            : current.toArray();
}

void slope::Slideshow::renderSlide(TimeTypeSec t, Slide& CS, TimeObject& T) {
    // latex texture reloads are only safe from here, see Latex::requestTexels
    Latex::in_render_pass = true;
    struct PassGuard {
        ~PassGuard() { Latex::in_render_pass = false; }
    } pass_guard;

    if (state.backward || !state.locked) {
        state.settle();
        for (auto& s : CS.getDepthSorted())
            s.first->play(T, CS[s.first]);
        return;
    }

    if (state.current > 0) {
        auto& PS = slides[state.current - 1];
        auto&& [common, UA, UB] = transitions[state.current - 1];
        if (t < 2 * transitionTime) {
            T.transition_parameter = std::min<parameter>(1, 0.5 * t / transitionTime);
            for (auto& c : common) {
                auto st = transition(0.5 * t / transitionTime, PS[c], CS[c]);
                st.persistentTransform = PersistentTransform();
                c->play(T, st);
            }
            if (t < transitionTime) {
                T.transition_parameter = t / transitionTime;
                for (auto& ua : UA)
                    ua->outro(T(t / transitionTime), PS[ua]);
            } else {
                handleTransition();
                for (auto& ub : UB)
                    ub->intro(T(t / transitionTime - 1.), CS[ub]);
            }
        } else {
            for (auto& s : CS.getDepthSorted())
                s.first->play(T, CS[s.first]);
            state.settle();
        }
    } else {
        if (t < transitionTime) {
            for (auto& s : CS.getDepthSorted())
                s.first->intro(T(t / transitionTime), CS[s.first]);
        } else {
            for (auto& s : CS.getDepthSorted())
                s.first->play(T, CS[s.first]);
            state.settle();
        }
    }
}

void slope::Slideshow::noteSlideArrival() {
    const TimeTypeSec now = TimeFrom(from_begin);
    // a jump skips the slides before it, so back-fill them as settled
    constexpr TimeTypeSec settle_time = 10;
    for (int k = 0; k < (int)state.current; k++)
        slide_times.emplace(k, now - settle_time);
    slide_times.emplace(state.current, now);
    // anything past the current slide has been left, so a second visit is a
    // fresh arrival rather than the original one
    slide_times.erase(slide_times.upper_bound(state.current), slide_times.end());
}

slope::parameter slope::Slideshow::transitionProgress(TimeTypeSec t) const {
    if (state.backward || !state.locked)
        return 1;
    if (state.current > 0)
        return t < 2 * transitionTime ? std::min<parameter>(1, 0.5 * t / transitionTime) : 1;
    return t < transitionTime ? parameter(t / transitionTime) : 1;
}

void slope::Slideshow::setInnerTime() {
    if (state.visited == (int)state.current)
        return;
    for (auto& p : appearing_primitives[state.current])
        p->handleInnerTime();
    state.visited = (int)state.current;
}

void slope::Slideshow::prompt() {
    if (prompter_ptr == nullptr)
        return;
    for (const auto& R : scripts_ranges)
        if (R.inRange(state.current)) {
            prompter_ptr->write(R.tag, from_begin);
            return;
        }
    prompter_ptr->erase(from_begin);
}

void slope::Slideshow::handleTransition() {
    if (state.done || state.current == 0)
        return;
    state.done = true;
    for (auto& s : uniquePrevious(transitions[state.current - 1]))
        s->disable();
    for (auto& s : uniqueNext(transitions[state.current - 1]))
        s->enable();

    if (!slides[state.current - 1].sameCamera(slides[state.current]))
        slides[state.current].setCam();
}

// the one thing a deck item needs that the author has to invent
void slope::Slideshow::copyLabelSuggestion(bool as_placement) {
    std::string l = LabelAnchor::suggestLabel();
    if (l.empty()) {
        spdlog::warn("could not find a free label to suggest");
        return;
    }
    // no layout of our own, an editor reindenting a pasted line being free to
    // undo it. The name alone serves as an "id:" just as well.
    std::string paste = as_placement ? "at: " + l : l;
    // straight to glfw, ImGui::SetClipboardText going nowhere when the current
    // context is not the one its glfw backend was set up on
    GLFWwindow* win = glfwGetCurrentContext();
    if (win == nullptr) {
        spdlog::warn("no window to hold the clipboard, \"{}\" was not copied", l);
        return;
    }
    glfwSetClipboardString(win, paste.c_str());
    // reading it back says whether the selection was really taken
    const char* back = glfwGetClipboardString(win);
    if (back == nullptr || paste != back) {
        spdlog::warn("the clipboard did not take \"{}\", use it as a name by hand", l);
        return;
    }
    spdlog::info("copied \"{}\" to the clipboard", paste);
}

void slope::Slideshow::ImGuiWindowConfig() {
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, io.DisplaySize.y));
    ImGui::SetNextFrameWantCaptureMouse(false);
    ImGui::SetNextFrameWantCaptureKeyboard(true);
}

void slope::Slideshow::onWindowClose(GLFWwindow* w) {
    auto* self = static_cast<Slideshow*>(glfwGetWindowUserPointer(w));
    if (LabelAnchor::hasDirty() || Params::hasDirty() || PersistentTransform::hasDirty() || self->time_tracker.hasRecordableSession()) {
        glfwSetWindowShouldClose(w, GLFW_FALSE);
        if (!self->wm.isAnyOpen())
            self->wm.Toggle(WindowType::QuitWarning);
    }
}

void slope::Slideshow::init(std::string project_name, int argc, char** argv) {
    help_wanted = slope::parseCLI(argc, argv);
    if (help_wanted)
        return;

    if (slope::Options::HideSlideNumbers)
        display_slide_number = false;

    slope::Options::ProjectName = project_name;

    std::cout << "			[ SLOPE PROJECT : " << slope::Options::ProjectName << " ]" << std::endl;

    std::cout << "[ PROJECT PATH ] " << slope::Options::ProjectPath << std::endl;
    std::cout << "[ PROJECT DATA PATH ] " << slope::Options::ProjectDataPath << std::endl;
    std::cout << "[ PROJECT CACHE PATH ] " << slope::Options::CachePath << std::endl;
    std::cout << "[ PROJECT VIEWS PATH ] " << slope::Options::ProjectViewsPath << std::endl;
    std::cout << "[ SCREEN RESOLUTION ] " << slope::Options::ScreenResolutionWidth << "x" << slope::Options::ScreenResolutionHeight << std::endl;

    addKeyboardInputs();
    input_manager.printInputs();

    file_editor.onFrameJump = [this](const path& file, int frame) {
        std::error_code a, b;
        if (std::filesystem::weakly_canonical(file, a) != std::filesystem::weakly_canonical(getFrameDeck(), b))
            return;
        if (frame < (int)getFrameStarts().size())
            goToSlide(getFrameStarts()[frame]);
    };

    file_editor.currentFrameOf = [this](const path& file) {
        std::error_code a, b;
        if (std::filesystem::weakly_canonical(file, a) != std::filesystem::weakly_canonical(getFrameDeck(), b))
            return -1;
        // the last frame that started at or before the slide on screen
        int frame = -1;
        for (int i = 0; i < (int)getFrameStarts().size(); ++i)
            if (getFrameStarts()[i] <= (int)state.current)
                frame = i;
        return frame;
    };

    polyscope::options::allowHeadlessBackends = slope::Options::ExportMode;
    polyscope::view::windowWidth = (int)Options::ScreenResolutionWidth;
    polyscope::view::windowHeight = (int)Options::ScreenResolutionHeight;

    polyscope::init();
    polyscope::options::buildGui = false;
    polyscope::options::autocenterStructures = false;
    polyscope::options::autoscaleStructures = false;
    polyscope::options::groundPlaneEnabled = false;
    polyscope::options::giveFocusOnShow = false;
    polyscope::options::automaticallyComputeSceneExtents = false;
    // polyscope defaults to 8, which is 8 full scene passes during a fade
    polyscope::options::transparencyRenderPasses = 4;
    polyscope::state::lengthScale = 2.;
    polyscope::state::boundingBox =
        std::tuple<glm::vec3, glm::vec3>{{-1., -1., -1.}, {1., 1., 1.}};
    polyscope::view::upDir = polyscope::view::UpDir::ZUp;

    polyscope::options::ssaaFactor = 2;

    window_flags = 0;
    window_flags |= ImGuiWindowFlags_NoTitleBar;
    window_flags |= ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoResize;
    window_flags |= ImGuiWindowFlags_NoBackground;
    window_flags |= ImGuiWindowFlags_NoScrollbar;
    // fullscreen, so focusing it would bury the panels behind it
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;

    spdlog::set_pattern("[slope] %v ");

    // a headless backend never initializes GLFW, touching the window crashes
    if (!polyscope::render::engine->isHeadless()) {
        GLFWwindow* win = glfwGetCurrentContext();
        glfwSetWindowUserPointer(win, this);
        glfwSetWindowCloseCallback(win, onWindowClose);
    }
}

std::string slope::Slideshow::getSlideTitle(int i) {
    auto title = slides[i].getTitle();
    if (title == "")
        title = std::to_string(i);
    return title;
}

void slope::Slideshow::goToSlide(int slide_nb) {
    if ((size_t)slide_nb == state.current)
        return;
    for (auto& p : slides[state.current])
        p.first->disable();
    state.jumpTo(slide_nb);
    for (auto& p : slides[state.current])
        p.first->enable();
    slides[state.current].setCam(false); // a jump lands, it does not fly
}

void slope::Slideshow::run() {
    if (help_wanted)
        return;
    if (!Options::ExportMode) {
        polyscope::state::userCallback = [this]() {
            play();
        };
        polyscope::show();
    } else if (Options::RecordMode) {
        recordVideo();
    } else if (Options::ExportTransitionSamples > 0) {
        exportTransitions();
    } else {
        exportPDF();
    }
}

void slope::Slideshow::initializeSlides() {
    precomputeTransitions();
    loadSlides();
    from_begin = Time::now();
    time_tracker.load();
    time_tracker.start();
    slides[state.current].setCam();
}

namespace {
std::string quote(const std::string& s) { return "\"" + s + "\""; }
} // namespace

void slope::Slideshow::exportPDF() {
    if (!initialized)
        initializeSlides();

    // /tmp is POSIX only, this is %TEMP% on Windows and /tmp elsewhere
    const std::filesystem::path tmp_dir = std::filesystem::temp_directory_path();
    auto slidePng = [&](int i) {
        return tmp_dir / ("slope_export_slide_" + std::to_string(i) + ".png");
    };

    const TimeTypeSec settle_time = 10;

    polyscope::state::userCallback = [this, settle_time]() {
        ImGuiWindowConfig();
        ImGui::Begin("Slope", NULL, window_flags);
        for (auto& s : slides[state.current])
            s.first->settleInnerTime(settle_time);
        state.from_action = Time::now() - std::chrono::duration_cast<TimeStamp::duration>(DurationSec(settle_time));
        noteSlideArrival();
        // An export runs the whole show in milliseconds, so a keyframe reached
        // two slides ago is only milliseconds old and an ease built on it would
        // render mid flight. Back date every arrival, as from_action is.
        for (auto& [slide, when] : slide_times)
            when = TimeFrom(from_begin) - settle_time;
        TimeObject T = getTimeObject();
        Snippet::setTime(T);
        // export renders settled slides, so no lerp out of the previous one
        updateBackground(1);
        auto& CS = slides[state.current];
        polyscope::options::transparencyMode = polyscope::TransparencyMode::None;
        for (auto& s : CS.getDepthSorted())
            s.first->play(T, CS[s.first]);
        if (display_slide_number)
            hud.drawSlideNumber(state.current);
        ImGui::End();
    };

    polyscope::ScreenshotOptions opts;
    opts.includeUI = true;
    opts.transparentBackground = false;

    for (int i = 0; i < (int)slides.size(); i++) {
        spdlog::info("exporting slide {} / {}", i + 1, slides.size());
        state.current = i;
        if (i > 0) {
            for (auto& s : uniquePrevious(transitions[i - 1]))
                s->disable();
            for (auto& s : uniqueNext(transitions[i - 1]))
                s->enable();
        }
        slides[i].setCam();
        polyscope::screenshot(slidePng(i).string(), opts);
    }

    polyscope::state::userCallback = nullptr;

    const std::string out = Options::ProjectPath + Options::ProjectName + ".pdf";
    std::string cmd = quote(Options::PathToCONVERT);
    for (int i = 0; i < (int)slides.size(); i++)
        cmd += " " + quote(slidePng(i).string());
    cmd += " " + quote(out);
    spdlog::info("generating PDF: {}", out);
    if (runCommand(cmd) != 0)
        spdlog::error("PDF generation failed, ImageMagick `convert` returned non-zero");
    else
        spdlog::info("PDF saved to {}", out);
}

// Replays each slide change by back dating state.from_action, the clock
// renderSlide measures a transition by. Camera flights do not replay.
void slope::Slideshow::exportTransitions() {
    if (!initialized)
        initializeSlides();

    const int samples = Options::ExportTransitionSamples;
    const TimeTypeSec settle_time = 10;

    // keyed by project, so exporting two decks does not overwrite one another
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / ("slope_transitions_" + Options::ProjectName);
    std::filesystem::create_directories(dir);

    auto backdate = [](TimeTypeSec s) {
        return Time::now() - std::chrono::duration_cast<TimeStamp::duration>(DurationSec(s));
    };

    // the sample being drawn, read by the callback below
    TimeTypeSec dt = 0;
    TimeTypeSec step_dt = 0;
    std::set<PrimitivePtr> appearing;

    polyscope::state::userCallback = [&]() {
        ImGuiWindowConfig();
        ImGui::Begin("Slope", NULL, window_flags);

        // the clock renderSlide reads, so t is exactly the sample time
        state.from_action = backdate(dt);
        // every arrival, since noteSlideArrival only knows the current slide
        const TimeTypeSec now = TimeFrom(from_begin);
        for (int k = 0; k <= (int)state.current; k++)
            slide_times[k] = now - (k == (int)state.current ? dt : settle_time);

        // one arriving on this very change enters at the midpoint, not before
        for (auto& p : slides[state.current])
            p.first->settleInnerTime(appearing.count(p.first)
                                         ? std::max<TimeTypeSec>(0, dt - transitionTime)
                                         : settle_time);

        TimeObject T = getTimeObject();
        // the wall clock delta is microseconds here, and nothing could integrate it
        T.delta_time = step_dt;
        T.slide_progress = transitionProgress(dt);
        TimeObject ST = T;
        ST.transition_parameter = T.slide_progress;
        Snippet::setTime(ST);

        updateBackground(T.slide_progress);
        polyscope::options::transparencyMode = polyscope::TransparencyMode::None;

        auto& CS = slides[state.current];
        renderSlide(dt, CS, T);

        if (display_slide_number)
            hud.drawSlideNumber(state.current);
        ImGui::End();
    };

    polyscope::ScreenshotOptions opts;
    opts.includeUI = true;
    opts.transparentBackground = false;

    // zero padded on both axes, so a lexical sort is the order they play in
    auto shot = [&](size_t i, int k) {
        char name[64];
        std::snprintf(name, sizeof(name), "slide_%03zu_step_%02d.png", i, k);
        polyscope::screenshot((dir / name).string(), opts);
    };

    for (size_t i = 0; i < slides.size(); i++) {
        spdlog::info("exporting transition into slide {} / {}", i + 1, slides.size());

        // Same state as after a key press, with the previous slide up and the change not handled.
        state.current = i;
        state.locked = true;
        state.done = false;
        state.backward = false;
        state.visited = (int)i; // inner times are set per sample below
        slide_times.clear();    // rebuilt per sample, up to the current slide

        // slide 0 comes from nothing, so all of it arrives, over one transitionTime
        appearing.clear();
        if (i > 0) {
            auto& fresh = uniqueNext(transitions[i - 1]);
            appearing.insert(fresh.begin(), fresh.end());
        } else {
            for (auto& p : slides[0])
                appearing.insert(p.first);
        }

        const TimeTypeSec span = (i > 0 ? 2 : 1) * transitionTime;
        step_dt = span / samples;
        for (int k = 0; k < samples; k++) {
            dt = k * step_dt;
            shot(i, k);
        }

        // the settled still, guarded so it only acts if no sample crossed the swap
        handleTransition();
        state.settle();
        slides[i].setCam(false);
        dt = settle_time;
        shot(i, samples);
    }

    polyscope::state::userCallback = nullptr;
    spdlog::info("{} stills written to {}", slides.size() * (samples + 1), dir.string());
}

// Each slide change is replayed as in exportTransitions, then the settled slide
// is held for RecordDwell seconds with primitive clocks still advancing. Frames
// are numbered globally so ffmpeg reads one sequence. Camera flights do not replay.
void slope::Slideshow::recordVideo() {
    if (!initialized)
        initializeSlides();

    const int fps = std::max(1, Options::RecordFPS);
    const TimeTypeSec step_dt = 1.0 / fps;
    const int dwell_frames = std::max(0, (int)(Options::RecordDwell * fps + 0.5));
    const TimeTypeSec settle_time = 10;

    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / ("slope_record_" + Options::ProjectName);
    std::filesystem::create_directories(dir);

    auto backdate = [](TimeTypeSec s) {
        return Time::now() - std::chrono::duration_cast<TimeStamp::duration>(DurationSec(s));
    };

    // the frame being drawn, read by the callback below
    TimeTypeSec dt = 0;
    std::set<PrimitivePtr> appearing;

    polyscope::state::userCallback = [&]() {
        ImGuiWindowConfig();
        ImGui::Begin("Slope", NULL, window_flags);

        state.from_action = backdate(dt);
        const TimeTypeSec now = TimeFrom(from_begin);
        for (int k = 0; k <= (int)state.current; k++)
            slide_times[k] = now - (k == (int)state.current ? dt : settle_time);

        for (auto& p : slides[state.current])
            p.first->settleInnerTime(appearing.count(p.first)
                                         ? std::max<TimeTypeSec>(0, dt - transitionTime)
                                         : settle_time);

        TimeObject T = getTimeObject();
        T.delta_time = step_dt;
        T.slide_progress = transitionProgress(dt);
        TimeObject ST = T;
        ST.transition_parameter = T.slide_progress;
        Snippet::setTime(ST);

        updateBackground(T.slide_progress);
        polyscope::options::transparencyMode = polyscope::TransparencyMode::None;

        auto& CS = slides[state.current];
        renderSlide(dt, CS, T);

        if (display_slide_number)
            hud.drawSlideNumber(state.current);
        ImGui::End();
    };

    polyscope::ScreenshotOptions opts;
    opts.includeUI = true;
    opts.transparentBackground = false;

    size_t frame = 0;
    auto shot = [&]() {
        char name[32];
        std::snprintf(name, sizeof(name), "frame_%06zu.png", frame++);
        polyscope::screenshot((dir / name).string(), opts);
    };

    for (size_t i = 0; i < slides.size(); i++) {
        spdlog::info("recording slide {} / {}", i + 1, slides.size());

        state.current = i;
        state.locked = true;
        state.done = false;
        state.backward = false;
        state.visited = (int)i;
        slide_times.clear();

        appearing.clear();
        if (i > 0) {
            auto& fresh = uniqueNext(transitions[i - 1]);
            appearing.insert(fresh.begin(), fresh.end());
        } else {
            for (auto& p : slides[0])
                appearing.insert(p.first);
        }

        const TimeTypeSec span = (i > 0 ? 2 : 1) * transitionTime;
        for (dt = 0; dt < span; dt += step_dt)
            shot();

        // guarded, so it only acts if no frame landed inside the swap window
        handleTransition();
        state.settle();
        slides[i].setCam(false);

        for (int f = 0; f < dwell_frames; f++) {
            dt = span + f * step_dt;
            shot();
        }
    }

    polyscope::state::userCallback = nullptr;

    const std::string out = Options::ProjectPath + Options::ProjectName + ".mp4";
    const std::string cmd = quote(Options::PathToFFMPEG) + " -y -loglevel error -framerate " + std::to_string(fps) + " -i " + quote((dir / "frame_%06d.png").string()) + " -c:v libx264 -pix_fmt yuv420p -movflags +faststart" + " -vf \"scale=trunc(iw/2)*2:trunc(ih/2)*2\" " + quote(out);
    spdlog::info("encoding {} frames to {}", frame, out);
    if (runCommand(cmd) != 0)
        spdlog::error("video encoding failed, ffmpeg returned non-zero");
    else
        spdlog::info("video saved to {}", out);
}

bool slope::Slideshow::recompose(const std::function<void(SlideManager&)>& composer,
                                 const std::set<PrimitivePtr>& stale,
                                 const std::function<void(SlideManager&)>& fallback) {
    size_t cur = state.current;

    if (!slides.empty())
        for (auto& p : slides[state.current])
            p.first->disable();
    for (auto& p : stale) {
        p->disable();
        p->resetFirstSlideNumber();
    }
    auto reset = [this] {
        for (auto& S : slides)
            for (auto& p : S)
                p.first->resetFirstSlideNumber();
        slides.clear();
        transitions.clear();
        appearing_primitives.clear();
        initialized = false;
    };
    reset();

    bool ok = true;
    try {
        composer(*this);
    } catch (const std::exception& e) {
        ok = false;
        spdlog::error("recompose failed: {}", e.what());
        // a half built show would replace the working one
        if (fallback) {
            reset();
            try {
                fallback(*this);
            } catch (const std::exception& e2) {
                spdlog::error("recompose fallback failed: {}", e2.what());
            }
        }
    }
    if (slides.empty())
        addSlide(Slide());

    state.jumpTo(std::min(cur, slides.size() - 1));
    state.visited = -1;
    state.settle();
    for (auto& p : slides[state.current])
        p.first->enable();
    return ok;
}

void slope::Slideshow::loadSlides() {
    if (display_slide_number)
        hud.initialize(slides.size(), [this](int i) { return getSlideTitle(i); });
    spdlog::info("[ number of distinct slides : {} ]", slides.size());
    if (Options::CheckLabels)
        LabelAnchor::reportLabelIssues();
}

void slope::Slideshow::clearTransformGizmos() {
    for (auto& S : slides)
        for (auto& p : S)
            p.second.persistentTransform.guizmo = nullptr;
}

void slope::Slideshow::transformEditor() {
    // a gizmo lives in its slide's state, leaving the slide would strand it
    if (gizmo_slide != (int)state.current) {
        clearTransformGizmos();
        gizmo_slide = (int)state.current;
    }

    Slide& CS = slides[state.current];
    // a mesh's transform moves it, a label's is the plane it is pasted on
    std::map<PrimitivePtr, PolyscopePrimitivePtr> polyscope_prims;
    for (const auto& pis : CS.getPolyscopePrimitives())
        polyscope_prims[pis.first] = pis.first;

    // with a selection the gizmo follows it, otherwise every transform shows
    PrimitivePtr sel = drag_editor.getSelected();

    std::set<std::string> seen;
    for (auto& p : CS) {
        auto& pt = CS.at(p.first).persistentTransform;
        if (!pt.isActive())
            continue;
        if ((sel && p.first != sel) || !seen.insert(pt.getLabel()).second) {
            pt.guizmo = nullptr;
            continue;
        }

        if (pt.guizmo == nullptr) {
            auto T = pt.stored();
            if (!T) T = LastPlaneDrawn(pt.getLabel());
            // a scene object has no plane to note, so fall back to identity, its actual current pose
            if (!T) T = Transform();
            // grabbing an unplaced plane freezes the billboard it was drawn as
            if (!pt.stored())
                pt.writeAtLabel(*T);

            pt.guizmo = PersistentTransform::makeGuizmo("transform " + pt.getLabel());
            pt.guizmo->setAllowTranslation(true);
            pt.guizmo->setAllowRotation(true);
            pt.guizmo->setAllowScaling(true);
            pt.guizmo->setInteractInLocalSpace(true);
            pt.guizmo->setTransform(T->getMatrix());
            pt.guizmo->setEnabled(true);
        } else {
            Transform T;
            T.fromGLMMat4(pt.guizmo->getTransform());
            // only a real drag counts, or opening the gizmo would dirty everything
            auto cur = pt.stored();
            if (!cur || glm::distance(cur->translation, T.translation) > 1e-6f || glm::distance(cur->scale, T.scale) > 1e-6f || std::abs(cur->angle - T.angle) > 1e-6f || glm::distance(cur->axis, T.axis) > 1e-6f)
                pt.writeAtLabel(T);
            auto pp = polyscope_prims.find(p.first);
            if (pp != polyscope_prims.end())
                pp->second->setTransform(p.second);
        }
    }
}

void slope::Slideshow::handleInputs() {
    drag_editor.handle(slides[state.current], wm);

    if (wm.isModalOpen())
        return;

    // a focused text field (the file editor) owns every keystroke
    const bool typing = ImGui::GetIO().WantTextInput;
    // KeyCtrl is Cmd on macOS, which the key guide already prints
    const bool ctrl = ImGui::GetIO().KeyCtrl;

    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
        LabelAnchor::saveAllDirty();
        Params::saveAllDirty();
        PersistentTransform::saveAllDirty();
    }

    if (!typing && ctrl && ImGui::IsKeyPressed(ImGuiKey_Z))
        drag_editor.undo(slides[state.current], wm);

    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !wm.isAnyOpen()) {
        if (LabelAnchor::hasDirty() || Params::hasDirty() || PersistentTransform::hasDirty() || file_editor.hasUnsaved() || time_tracker.hasRecordableSession())
            wm.Toggle(WindowType::QuitWarning);
        else
            polyscope::unshow();
    }

    for (const auto& input : input_manager.getInputs()) {
        if (input.trigger == ImGuiKey_None) continue;
        if (typing) continue;
        if (ImGui::IsKeyPressed(input.trigger)) {
            if (!input.isPopUp && !wm.isAnyOpen())
                input.callback();
            else if (input.isPopUp)
                input.callback();
        }
    }

    polyscope::options::buildGui = wm.isOpen(WindowType::PolyscopeGUI);
}

void slope::Slideshow::addKeyboardInputs() {
    input_manager.addInput("next slide", "right arrow", ImGuiKey_RightArrow, [this]() {
            if (!state.locked && !state.isPaused()) {
                auto& CS = slides[state.current];
                if (CS.pause_duration > 0) {
                    state.startPause();
                } else {
                    nextFrame();
                }
            } }, false);
    input_manager.addInput("previous slide", "left arrow", ImGuiKey_LeftArrow, [this]() { previousFrame(); }, false);
    input_manager.addInput("skip to next slide without transition", "down arrow", ImGuiKey_DownArrow, [this]() { forceNextFrame(); }, false);
    input_manager.addInput("screenshot", "P", ImGuiKey_P, [this]() {
            static int screenshot_count = 0;
            constexpr int nb_zeros = 6;
            auto n = std::to_string(screenshot_count++);
            std::error_code ec;
            path file = std::filesystem::temp_directory_path(ec)
                        / ("screenshot_" + std::string(nb_zeros-n.size(),'0') + n + ".png");
            slope::screenshot(file.string());
            spdlog::info("screenshot saved at {}", file.string()); }, false);

    input_manager.addInput("copy a label for a new item", "N", ImGuiKey_N, [this]() { copyLabelSuggestion(); }, false);
    input_manager.addInput("reload latex", "L", ImGuiKey_L, [this]() { slope::LatexLoader::ReloadContentAndUpdate(); }, false);
    input_manager.addInput("show slide goto and timings", "Tab", ImGuiKey_Tab, [this]() { wm.Toggle(WindowType::SlideMenu); }, true);
    input_manager.addInput("export current camera view", "C", ImGuiKey_C, [this]() { wm.Toggle(WindowType::Camera); }, true);
    input_manager.addInput("show animation parameter tuner", "A", ImGuiKey_A, [this]() { wm.Toggle(WindowType::Tuner); }, true);
    input_manager.addInput("show polyscope GUI", "D", ImGuiKey_D, [this]() { wm.Toggle(WindowType::PolyscopeGUI); }, true);
    input_manager.addInput("edit hot-reloaded files", "E", ImGuiKey_E, [this]() {
            // Toggle tells whether the window just opened. Jump only if the slide changed since.
            if (wm.Toggle(WindowType::FileEditor) && editor_jump_slide != (int)state.current) {
                editor_jump_slide = (int)state.current;
                if (file_editor.currentFile().empty())
                    file_editor.open(getFrameDeck());
                file_editor.jumpToCurrentFrame();
            } }, true);
    input_manager.addInput("reset timings", "R", ImGuiKey_R, [this]() { time_tracker.reset(); }, true);
    input_manager.addInput("pause/resume the rehearsal timer", "space", ImGuiKey_Space, [this]() { time_tracker.togglePause(); }, true);

    input_manager.addInput("show transform guizmo editor", "T", ImGuiKey_T, true);
    input_manager.addInput("center horizontally dragged primitive", "H", ImGuiKey_H, false);
    input_manager.addInput("center vertically dragged primitive", "V", ImGuiKey_V, false);
    input_manager.addInput("save dragged positions to disk", "Ctrl+S");
    input_manager.addInput("undo last move", "Ctrl+Z");
    input_manager.addInput("insert a label for a new item at the file editor's cursor", "Ctrl+N");
    input_manager.addInput("select/drag a primitive, click again on the same spot to cycle through overlapping ones", "Ctrl+click");
    input_manager.addInput("toggle primitives in a group selection, hold left click to move them together", "Ctrl+Shift+click");
    input_manager.addInput("Scale the selected primitive", "Wheel");
    input_manager.addInput("Change the alpha of the selected primitive", "Shift + Wheel");
    input_manager.addInput("Rotate the selected primitive", "Ctrl + Wheel");
}

bool slope::Slideshow::inGizmoMode() const {
    // a mode is something you enter, a visible parameter's handle is not one
    return wm.isOpen(WindowType::Transform) || (wm.isOpen(WindowType::Tuner) && Params::hasLiveGizmo());
}

void slope::Slideshow::handleGuizmos() {
    if (ImGui::IsKeyPressed(ImGuiKey_T))
        wm.Toggle(WindowType::Transform);

    if (wm.isOpen(WindowType::Transform))
        transformEditor();
    else if (gizmo_slide != -1) {
        // however the mode was left, the widgets go with it
        clearTransformGizmos();
        gizmo_slide = -1;
    }
}

void slope::Slideshow::displayPopUps() {
    if (wm.isOpen(WindowType::Camera))
        camera_exporter.drawPopup(wm);
    else if (wm.isOpen(WindowType::Tuner))
        Params::DrawPanel();
    else if (wm.isOpen(WindowType::FileEditor))
        file_editor.draw(wm);
    else if (wm.isOpen(WindowType::SlideMenu))
        time_tracker.drawMenu(slides.size(), [this](int i) { return getSlideTitle(i); }, [this](int i) { goToSlide(i); });
    else if (wm.isOpen(WindowType::QuitWarning)) {
        ImGui::OpenPopup("Save before quitting?");
        if (ImGui::BeginPopupModal("Save before quitting?", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            if (LabelAnchor::hasDirty() || Params::hasDirty() || PersistentTransform::hasDirty())
                ImGui::Text("Unsaved position/parameter changes.");
            if (file_editor.hasUnsaved())
                ImGui::Text("Unsaved edits to %s in the file editor.",
                            file_editor.currentFile().filename().string().c_str());
            if (time_tracker.hasRecordableSession())
                ImGui::Text("This rehearsal session's timings are not saved.");
            ImGui::Text("Quit without saving?");
            ImGui::Spacing();
            if (ImGui::Button("Save and quit")) {
                LabelAnchor::saveAllDirty();
                Params::saveAllDirty();
                PersistentTransform::saveAllDirty();
                time_tracker.save();
                // a failed editor save keeps the dialog up, its line still showing
                if (file_editor.saveUnsaved()) {
                    polyscope::unshow();
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Discard and quit")) {
                polyscope::unshow();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                wm.CloseAll();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
}
