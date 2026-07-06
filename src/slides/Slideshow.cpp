#include "Slideshow.h"
#include "content/screen_primitives/LateX.h"
#include "spdlog/spdlog.h"
#include "polyscope/pick.h"
#include "content/color_tools.h"
#include "content/polyscope_primitives/BackgroundColor.h"
#include "content/Params.h"


void slope::Slideshow::nextFrame()
{
    if (state.current == slides.size()-1)
        return;
    state.goForward();
}

void slope::Slideshow::previousFrame()
{
    if (!state.current)
        return;
    for (auto& s : uniqueNext(transitions[state.current-1]))
        s->disable();
    for (auto& s : uniquePrevious(transitions[state.current-1]))
        s->enable();
    state.goBackward();
    for (auto& s : slides[state.current]){
        auto p = s.first;
        p->intro(TimeObject(p->getInnerTime(),1),s.second);
    }
    slides[state.current].setCam();
}

void slope::Slideshow::forceNextFrame()
{
    if (state.current == slides.size()-1)
        return;
    for (auto& s : uniquePrevious(transitions[state.current]))
        s->disable();
    for (auto& s : uniqueNext(transitions[state.current]))
        s->enable();
    state.skip();
    for (auto& s : slides[state.current]){
        auto p = s.first;
        p->intro(TimeObject(p->getInnerTime(),1),s.second);
    }
    slides[state.current].setCam();
}

void slope::Slideshow::play() {

    handleGuizmos();
    if (halt_slope_display)
        return;

    polyscope::view::bgColor = BackgroundColor::Default.toArray();

    Params::NewFrame();

    ImGuiWindowConfig();
    ImGui::Begin("Slope",NULL,window_flags);

    if (!initialized)
        initializeSlides();

    auto t = TimeFrom(state.from_action);
    auto& CS = slides[state.current];
    setInnerTime();
    TimeObject T = getTimeObject();

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

    if (onFrame)
        onFrame();

    if (display_slide_number)
        hud.drawSlideNumber(state.current);


    ImGui::End();
    displayPopUps();
}

void slope::Slideshow::renderSlide(TimeTypeSec t, Slide& CS, TimeObject& T)
{
    if (state.backward || !state.locked) {
        state.settle();
        for (auto& s : CS.getDepthSorted())
            s.first->play(T, CS[s.first]);
        return;
    }

    if (state.current > 0) {
        auto& PS = slides[state.current-1];
        auto&& [common, UA, UB] = transitions[state.current-1];
        if (t < 2*transitionTime) {
            for (auto& c : common) {
                auto st = transition(0.5*t/transitionTime, PS[c], CS[c]);
                st.persistentTransform = PersistentTransform();
                c->play(T, st);
            }
            if (t < transitionTime) {
                T.transition_parameter = t/transitionTime;
                for (auto& ua : UA)
                    ua->outro(T(t/transitionTime), PS[ua]);
            } else {
                handleTransition();
                for (auto& ub : UB)
                    ub->intro(T(t/transitionTime-1.), CS[ub]);
            }
        } else {
            for (auto& s : CS.getDepthSorted())
                s.first->play(T, CS[s.first]);
            state.settle();
        }
    } else {
        if (t < transitionTime) {
            for (auto& s : CS.getDepthSorted())
                s.first->intro(T(t/transitionTime), CS[s.first]);
        } else {
            for (auto& s : CS.getDepthSorted())
                s.first->play(T, CS[s.first]);
            state.settle();
        }
    }
}

void slope::Slideshow::setInnerTime()
{
    if (state.visited == (int)state.current)
        return;
    for (auto& p : appearing_primitives[state.current])
        p->handleInnerTime();
    state.visited = (int)state.current;
}

void slope::Slideshow::prompt()
{
    if (prompter_ptr == nullptr)
        return;
    for (const auto& R : scripts_ranges)
        if (R.inRange(state.current)){
            prompter_ptr->write(R.tag,from_begin);
            return;
        }
    prompter_ptr->erase(from_begin);
}

void slope::Slideshow::handleTransition()
{
    if (state.done || state.current == 0)
        return;
    state.done = true;
    for (auto& s : uniquePrevious(transitions[state.current-1]))
        s->disable();
    for (auto& s : uniqueNext(transitions[state.current-1]))
        s->enable();

    if (!slides[state.current-1].sameCamera(slides[state.current]))
        slides[state.current].setCam();
}



void slope::Slideshow::ImGuiWindowConfig()
{
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::SetNextWindowPos(ImVec2(0,0));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x,io.DisplaySize.y));
    ImGui::SetNextFrameWantCaptureMouse(false);
    ImGui::SetNextFrameWantCaptureKeyboard(true);
}

void slope::Slideshow::onWindowClose(GLFWwindow* w)
{
    auto* self = static_cast<Slideshow*>(glfwGetWindowUserPointer(w));
    if (LabelAnchor::hasDirty() || Params::hasDirty()) {
        glfwSetWindowShouldClose(w, GLFW_FALSE);
        if (!self->wm.isAnyOpen())
            self->wm.Toggle(WindowType::QuitWarning);
    }
}

void slope::Slideshow::init(std::string project_name,int argc,char** argv)
{
    help_wanted = slope::parseCLI(argc,argv);
    if (help_wanted)
        return;

    slope::Options::ProjectName = project_name;

    std::cout << "			[ SLOPE PROJECT : " << slope::Options::ProjectName << " ]" << std::endl;

    std::cout << "[ PROJECT PATH ] " << slope::Options::ProjectPath << std::endl;
    std::cout << "[ PROJECT DATA PATH ] " << slope::Options::ProjectDataPath << std::endl;
    std::cout << "[ PROJECT CACHE PATH ] " << slope::Options::CachePath << std::endl;
    std::cout << "[ PROJECT VIEWS PATH ] " << slope::Options::ProjectViewsPath << std::endl;
    std::cout << "[ SCREEN RESOLUTION ] " << slope::Options::ScreenResolutionWidth<<"x"<<slope::Options::ScreenResolutionHeight  << std::endl;

    addKeyboardInputs();
    input_manager.printInputs();


    polyscope::options::allowHeadlessBackends = slope::Options::ExportMode;
    polyscope::view::windowWidth  = (int)Options::ScreenResolutionWidth;
    polyscope::view::windowHeight = (int)Options::ScreenResolutionHeight;

    polyscope::init();
    polyscope::options::buildGui = false;
    polyscope::options::autocenterStructures = false;
    polyscope::options::autoscaleStructures = false;
    polyscope::options::groundPlaneEnabled = false;
    polyscope::options::giveFocusOnShow = false;
    polyscope::options::automaticallyComputeSceneExtents = false;
    polyscope::options::transparencyMode = polyscope::TransparencyMode::Pretty;
    polyscope::state::lengthScale = 2.;
    polyscope::state::boundingBox =
        std::tuple<glm::vec3, glm::vec3>{ {-1., -1., -1.}, {1., 1., 1.} };
    polyscope::view::upDir = polyscope::view::UpDir::ZUp;

    polyscope::options::ssaaFactor = 2;

    window_flags = 0;
    window_flags |= ImGuiWindowFlags_NoTitleBar;
    window_flags |= ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoResize;
    window_flags |= ImGuiWindowFlags_NoBackground;
    window_flags |= ImGuiWindowFlags_NoScrollbar;

    spdlog::set_pattern("[slope] %v ");

    ImPlot::CreateContext();

    GLFWwindow* win = glfwGetCurrentContext();
    glfwSetWindowUserPointer(win, this);
    glfwSetWindowCloseCallback(win, onWindowClose);
}

std::string slope::Slideshow::getSlideTitle(int i)
{
    auto title = slides[i].getTitle();
    if (title == "")
        title = std::to_string(i);
    return title;
}

void slope::Slideshow::goToSlide(int slide_nb)
{
    if ((size_t)slide_nb == state.current)
        return;
    for (auto& p : slides[state.current])
        p.first->disable();
    state.jumpTo(slide_nb);
    for (auto& p : slides[state.current])
        p.first->enable();
    slides[state.current].setCam();
}

void slope::Slideshow::run()
{
    if (help_wanted)
        return;
    if (!Options::ExportMode){
        polyscope::state::userCallback = [this](){
            play();
        };
        polyscope::show();
    } else {
        exportPDF();
    }
}

void slope::Slideshow::initializeSlides()
{
    precomputeTransitions();
    loadSlides();
    from_begin = Time::now();
    time_tracker.start();
    slides[state.current].setCam();
}


void slope::Slideshow::exportPDF()
{
    if (!initialized)
        initializeSlides();

    polyscope::state::userCallback = [this]() {
        ImGuiWindowConfig();
        ImGui::Begin("Slope", NULL, window_flags);
        setInnerTime();
        TimeObject T = getTimeObject();
        auto& CS = slides[state.current];
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
        spdlog::info("exporting slide {} / {}", i+1, slides.size());
        state.current = i;
        polyscope::screenshot("/tmp/slide_" + std::to_string(i) + ".png", opts);
    }

    polyscope::state::userCallback = nullptr;

    const std::string out = Options::ProjectPath + Options::ProjectName + ".pdf";
    std::string cmd = "convert";
    for (int i = 0; i < (int)slides.size(); i++)
        cmd += " /tmp/slide_" + std::to_string(i) + ".png";
    cmd += " " + out;
    spdlog::info("generating PDF: {}", out);
    if (std::system(cmd.c_str()) != 0)
        spdlog::error("PDF generation failed — ImageMagick `convert` returned non-zero");
    else
        spdlog::info("PDF saved to {}", out);
}

void slope::Slideshow::recompose(const std::function<void(SlideManager&)>& composer,
                                 const std::set<PrimitivePtr>& stale)
{
    size_t cur = state.current;

    if (!slides.empty())
        for (auto& p : slides[state.current])
            p.first->disable();
    for (auto& S : slides)
        for (auto& p : S)
            p.first->resetFirstSlideNumber();
    for (auto& p : stale) {
        p->disable();
        p->resetFirstSlideNumber();
    }
    slides.clear();
    transitions.clear();
    appearing_primitives.clear();
    initialized = false;

    try {
        composer(*this);
    } catch (const std::exception& e) {
        spdlog::error("recompose failed: {}", e.what());
    }
    if (slides.empty())
        addSlide(Slide());

    state.jumpTo(std::min(cur, slides.size()-1));
    state.visited = -1;
    state.settle();
    for (auto& p : slides[state.current])
        p.first->enable();
}

void slope::Slideshow::loadSlides()
{
    hud.initialize(slides.size(), [this](int i){ return getSlideTitle(i); });
    spdlog::info("[ number of distinct slides : {} ]", slides.size());
}


void slope::Slideshow::transformEditor()
{
    auto PP = slides[state.current].getPolyscopePrimitives();
    for (const auto& pis : PP){
        PrimitivePtr p = pis.first;
        auto& pt = slides[state.current].at(p).persistentTransform;
        if (!pt.isActive())
            continue;
        if (pt.guizmo == nullptr){
            pt.guizmo = new polyscope::TransformationGizmo(pis.first->getPolyscopeName()+pt.getLabel());
            pt.guizmo->setAllowTranslation(true);
            pt.guizmo->setAllowRotation(true);
            pt.guizmo->setAllowScaling(true);
            pt.guizmo->setInteractInLocalSpace(true);
            pt.writeAtLabel(pt.readFromLabel());
            pt.guizmo->setTransform(pt.readFromLabel().getMatrix());
            pt.guizmo->setEnabled(true);
        }
        else {
            Transform T;T.fromGLMMat4(pt.guizmo->getTransform());
            pt.writeAtLabel(T);
            pis.first->setTransform(pis.second);
        }
    }
}

void slope::Slideshow::handleInputs()
{
    drag_editor.handle(slides[state.current], wm);

    if (wm.isModalOpen())
        return;

    if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_S)) {
        LabelAnchor::saveAllDirty();
        Params::saveAllDirty();
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Escape) && !wm.isAnyOpen()) {
        if (LabelAnchor::hasDirty() || Params::hasDirty())
            wm.Toggle(WindowType::QuitWarning);
        else
            polyscope::unshow();
    }

    for (const auto& input : input_manager.getInputs()) {
        if (input.trigger == ImGuiKey_None) continue;
        if (ImGui::IsKeyPressed(input.trigger)) {
            if (!input.isPopUp && !wm.isAnyOpen())
                input.callback();
            else if (input.isPopUp)
                input.callback();
        }
    }

    polyscope::options::buildGui = wm.isOpen(WindowType::PolyscopeGUI);
}

void slope::Slideshow::addKeyboardInputs()
{
    input_manager.addInput("next slide","right arrow",ImGuiKey_RightArrow,
        [this](){
            if (!state.locked && !state.isPaused()) {
                auto& CS = slides[state.current];
                if (CS.pause_duration > 0) {
                    state.startPause();
                } else {
                    nextFrame();
                }
            }
        },false);
    input_manager.addInput("previous slide","left arrow",ImGuiKey_LeftArrow,
        [this](){previousFrame();},false);
    input_manager.addInput("skip to next slide without transition","down arrow",ImGuiKey_DownArrow,
        [this](){forceNextFrame();},false);
    input_manager.addInput("screenshot","P",ImGuiKey_P,
        [this](){
            static int screenshot_count = 0;
            constexpr int nb_zeros = 6;
            auto n = std::to_string(screenshot_count++);
            path file =  "/tmp/screenshot_" + std::string(nb_zeros-n.size(),'0') + n + ".png";
            slope::screenshot(file.string());
            spdlog::info("screenshot saved at {}", file.string());
        },false);

    input_manager.addInput("reload latex","L",ImGuiKey_L,
        [this](){slope::LatexLoader::ReloadContentAndUpdate();},false);
    input_manager.addInput("show slide goto and timings","Tab",ImGuiKey_Tab,
        [this](){wm.Toggle(WindowType::SlideMenu);},true);
    input_manager.addInput("export current camera view","C",ImGuiKey_C,
        [this](){wm.Toggle(WindowType::Camera);},true);
    input_manager.addInput("show color palette editor","W",ImGuiKey_W,
        [this](){wm.Toggle(WindowType::Palette);},true);
    input_manager.addInput("show animation parameter tuner","A",ImGuiKey_A,
        [this](){wm.Toggle(WindowType::Tuner);},true);
    input_manager.addInput("show polyscope GUI","D",ImGuiKey_D,
        [this](){wm.Toggle(WindowType::PolyscopeGUI);},true);
    input_manager.addInput("reset timings","R",ImGuiKey_R,
        [this](){ time_tracker.reset(); },true);

    input_manager.addInput("show transform guizmo editor","T",ImGuiKey_T,true);
    input_manager.addInput("center horizontally dragged primitive","H",ImGuiKey_H,false);
    input_manager.addInput("center vertically dragged primitive","V",ImGuiKey_V,false);
    input_manager.addInput("save dragged positions to disk","Ctrl+S");
    input_manager.addInput("toggle primitives in a group selection, hold left click to move them together","Ctrl+Shift+click");
}


void slope::Slideshow::handleGuizmos()
{
    if (ImGui::IsKeyPressed(ImGuiKey_T)){
        if (wm.isOpen(WindowType::Transform)){
            halt_slope_display = false;
            for (auto& pis : slides[state.current].getPolyscopePrimitives()){
                auto& pt = slides[state.current].at(pis.first).persistentTransform;
                if (!pt.isActive())
                    continue;
                pt.guizmo->setEnabled(false);
                pt.guizmo->remove();
                pt.guizmo = nullptr;
            }
        }
        if (wm.Toggle(WindowType::Transform))
            halt_slope_display = true;
    }

    if (wm.isOpen(WindowType::Transform)){
        transformEditor();
    }

}

void slope::Slideshow::displayPopUps()
{
    if (wm.isOpen(WindowType::Camera))
        camera_exporter.drawPopup(wm);
    else if (wm.isOpen(WindowType::Palette))
        PaletteHandler::ShowColorPickingModule();
    else if (wm.isOpen(WindowType::Tuner))
        Params::DrawPanel();
    else if (wm.isOpen(WindowType::SlideMenu))
        time_tracker.drawMenu(slides.size(),
            [this](int i){ return getSlideTitle(i); },
            [this](int i){ goToSlide(i); });
    else if (wm.isOpen(WindowType::QuitWarning)) {
        ImGui::OpenPopup("Unsaved positions");
        if (ImGui::BeginPopupModal("Unsaved positions", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Unsaved position changes will be lost.");
            ImGui::Spacing();
            if (ImGui::Button("Save and quit")) {
                LabelAnchor::saveAllDirty();
                Params::saveAllDirty();
                polyscope::unshow();
                ImGui::CloseCurrentPopup();
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

