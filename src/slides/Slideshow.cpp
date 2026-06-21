#include "Slideshow.h"
#include "content/screen_primitives/LateX.h"
#include "spdlog/spdlog.h"
#include "polyscope/pick.h"
#include "content/color_tools.h"
#include "content/polyscope_primitives/BackgroundColor.h"


void slope::Slideshow::nextFrame()
{
    if (current_slide == slides.size()-1)
        return;
    pause_active = false;
    current_slide++;
    from_action = Time::now();
    backward = false;
    locked = true;
    transition_done = false;
}

void slope::Slideshow::previousFrame()
{
    if (!current_slide)
        return;
    pause_active = false;
    for (auto& s : uniqueNext(transitions[current_slide-1]))
        s->disable();
    for (auto& s : uniquePrevious(transitions[current_slide-1]))
        s->enable();
    current_slide--;
    for (auto& s : slides[current_slide]){
        auto p = s.first;
        p->intro(TimeObject(p->getInnerTime(),1),s.second);
    }

    slides[current_slide].setCam();

    from_action = Time::now();
    backward = true;
    locked = true;
}

void slope::Slideshow::forceNextFrame()
{
    if (current_slide == slides.size()-1)
        return;
    pause_active = false;
    for (auto& s : uniquePrevious(transitions[current_slide]))
        s->disable();
    for (auto& s : uniqueNext(transitions[current_slide]))
        s->enable();
    current_slide++;
    for (auto& s : slides[current_slide]){
        auto p = s.first;
        p->intro(TimeObject(p->getInnerTime(),1),s.second);
    }

    slides[current_slide].setCam();

    from_action = Time::now();
    backward = false;
    locked = false;
}

void slope::Slideshow::play() {

    handleGuizmos();
    if (halt_slope_display)
        return;

    polyscope::view::bgColor = BackgroundColor::Default.toArray();

    ImGuiWindowConfig();
    ImGui::Begin("Slope",NULL,window_flags);

    if (!initialized)
        initializeSlides();

    auto t = TimeFrom(from_action);

    auto& CS = slides[current_slide];

    setInnerTime();

    bool triggerIT = false;

    TimeObject T = getTimeObject();




    if (backward || !locked) {
        locked = false;
        for (auto& s : CS.getDepthSorted())
            s.first->play(T,CS[s.first]);
    }
    else {
        if (current_slide > 0){
            auto& PS = slides[current_slide-1];
            auto&& [common,UA,UB] = transitions[current_slide-1];
            if (t < 2*transitionTime){
                for (auto& c : common) {
                    auto st = transition(0.5*t/transitionTime,
                                         PS[c],
                                         CS[c]);
                    st.persistentTransform = PersistentTransform(); // disable to force transition
                    c->play(T,st);
                }
                if (t < transitionTime){
                    T.transition_parameter = t/transitionTime;
                    for (auto& ua : UA){
                        auto p = ua;
                        p->outro(T(t/transitionTime),PS[ua]);
                    }
                }
                else {
                    handleTransition();
                    for (auto& ub : UB){
                        ub->intro(T(t/transitionTime-1.),CS[ub]);
                    }
                }
            }
            else {
                for (auto& s : CS.getDepthSorted()){
                    s.first->play(T,CS[s.first]);
                }
                locked = false;
            }
        }
        else {
            if (t < transitionTime){
                for (auto& s : CS.getDepthSorted()){
                    s.first->intro(T(t/transitionTime),CS[s.first]);
                }
            }
            else {
                for (auto& s : CS.getDepthSorted()){
                    s.first->play(T,CS[s.first]);
                }
                locked = false;
            }
        }
    }


    prompt();

    if (pause_active) {
        hud.drawPauseIndicator(TimeFrom(pause_start), slides[current_slide].pause_duration);
        if (TimeFrom(pause_start) >= slides[current_slide].pause_duration) {
            pause_active = false;
            nextFrame();
        }
    }

    handleInputs();

    time_tracker.record(getSlideTitle(current_slide));


    if (LatexLoader::initialized)
        LatexLoader::HotReloadIfModified();


    if (display_slide_number)
        hud.drawSlideNumber(current_slide);


    ImGui::End();
    displayPopUps();
}

void slope::Slideshow::setInnerTime()
{
    if (visited_slide == current_slide)
        return;
    for (auto& p : appearing_primitives[current_slide])
        p->handleInnerTime();
    visited_slide = current_slide;
}

void slope::Slideshow::prompt()
{
    if (prompter_ptr == nullptr)
        return;
    for (const auto& R : scripts_ranges)
        if (R.inRange(current_slide)){
            prompter_ptr->write(R.tag,from_begin);
            return;
        }
    prompter_ptr->erase(from_begin);
}

void slope::Slideshow::handleTransition()
{
    if (transition_done || current_slide == 0)
        return;
    transition_done = true;
    for (auto& s : uniquePrevious(transitions[current_slide-1]))
        s->disable();
    for (auto& s : uniqueNext(transitions[current_slide-1]))
        s->enable();

    if (!slides[current_slide-1].sameCamera(slides[current_slide])){
        slides[current_slide].setCam();
    }
}



void slope::Slideshow::ImGuiWindowConfig()
{
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::SetNextWindowPos(ImVec2(0,0));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x,io.DisplaySize.y));
    ImGui::SetNextFrameWantCaptureMouse(false);
    ImGui::SetNextFrameWantCaptureKeyboard(true);
}

void slope::Slideshow::init(std::string project_name,int argc,char** argv)
{
    help_wanted = slope::parseCLI(argc,argv);
    if (help_wanted)
        return;
    from_action = Time::now();
    from_begin = Time::now();

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
    if (slide_nb == current_slide)
        return;
    for (auto& p : slides[current_slide])
        p.first->disable();
    current_slide = slide_nb;
    for (auto& p : slides[current_slide])
        p.first->enable();
    slides[current_slide].setCam();
    from_action = Time::now();
    from_action = Time::now();
}

int slope::Slideshow::getRelativeSlideNumber(Primitive *p)
{
    computeFirstSlideNumbers();
    return p->relativeSlideIndex(slides.size()-1);
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
    computeFirstSlideNumbers();
    from_begin = Time::now();
    time_tracker.start();
    slides[current_slide].setCam();
}

void slope::Slideshow::computeFirstSlideNumbers()
{
    for (int i = 0;const auto& v : slides){
        for (auto& p : v)
            p.first->upFirstSlideNumber(i);
        i++;
    }
}


void slope::Slideshow::exportPDF()
{
    ImGuiWindowConfig();
    ImGui::Begin("Slope",NULL,window_flags);

    if (!initialized)
        initializeSlides();

    bool triggerIT = false;

    for (int i = 0;i<slides.size();i++) {
        spdlog::info("export slide {} over {}",i+1,slides.size());
        current_slide = i;
        auto& CS = slides[current_slide];
        setInnerTime();
        TimeObject T = getTimeObject();
        for (auto& s : CS.getDepthSorted())
            s.first->play(T,CS[s.first]);
        polyscope::screenshot(("/tmp/slide_" + std::to_string(i) + ".png").c_str());
    }
}

void slope::Slideshow::loadSlides()
{
    hud.initialize(slides.size(), [this](int i){ return getSlideTitle(i); });
    spdlog::info("[ number of distinct slides : {} ]", slides.size());
}


void slope::Slideshow::transformEditor()
{
    auto PP = slides[current_slide].getPolyscopePrimitives();
    for (const auto& pis : PP){
        PrimitivePtr p = pis.first;
        auto& pt = slides[current_slide].at(p).persistentTransform;
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
    drag_editor.handle(slides[current_slide], wm);

    if (wm.isModalOpen())
        return;

    if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_S))
        LabelAnchor::saveAllDirty();

    if (ImGui::IsKeyPressed(ImGuiKey_Escape) && !wm.isAnyOpen()) {
        if (LabelAnchor::hasDirty())
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
            if (!locked && !pause_active) {
                auto& CS = slides[current_slide];
                if (CS.pause_duration > 0) {
                    pause_active = true;
                    pause_start = Time::now();
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
    input_manager.addInput("show polyscope GUI","D",ImGuiKey_D,
        [this](){wm.Toggle(WindowType::PolyscopeGUI);},true);
    input_manager.addInput("reset timings","R",ImGuiKey_R,
        [this](){ time_tracker.reset(); },true);

    input_manager.addInput("show transform guizmo editor","T",ImGuiKey_T,true);
    input_manager.addInput("center horizontally dragged primitive","H",ImGuiKey_H,false);
    input_manager.addInput("center vertically dragged primitive","V",ImGuiKey_V,false);
    input_manager.addInput("save dragged positions to disk","Ctrl+S");
}


void slope::Slideshow::handleGuizmos()
{
    if (ImGui::IsKeyPressed(ImGuiKey_T)){
        if (wm.isOpen(WindowType::Transform)){
            halt_slope_display = false;
            for (auto& pis : slides[current_slide].getPolyscopePrimitives()){
                auto& pt = slides[current_slide].at(pis.first).persistentTransform;
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
    std::string file;

    if (wm.isOpen(WindowType::Camera))
        camera_exporter.drawPopup(wm);
    else if (wm.isOpen(WindowType::Palette))
        PaletteHandler::ShowColorPickingModule();
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

