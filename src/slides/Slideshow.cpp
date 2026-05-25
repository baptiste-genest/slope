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

    handleInputs();

    recordTime();


    if (LatexLoader::initialized)
        LatexLoader::HotReloadIfModified();


    if (display_slide_number)
        displaySlideNumber();


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

void slope::Slideshow::handleDragAndDrop()
{
    if (wm.isOtherOpen(WindowType::DragAndDrop))
        return;

    auto io = ImGui::GetIO();

    static double x_offset = 0;
    static double y_offset = 0;
    static double original_alpha = 0;
    static auto time_at_pick = Time::now();

    bool ctrl = ImGui::IsKeyDown(ImGuiKey_LeftCtrl);
    bool click = io.MouseClicked[0];

    if (ctrl && click && selected_primitive == nullptr){
        wm.Toggle(WindowType::DragAndDrop);
        auto S = ImGui::GetWindowSize();
        auto x = double(io.MousePos.x)/S.x;
        auto y = double(io.MousePos.y)/S.y;
        selected_primitive = getPrimitiveUnderMouse(x,y);
        if (selected_primitive != nullptr) {
            auto& pis = slides[current_slide][selected_primitive];
            LabelAnchorPtr lab = std::dynamic_pointer_cast<LabelAnchor>(pis.anchor);
            if (lab != nullptr) {
                x_offset = lab->getPos()(0) - x;
                y_offset = lab->getPos()(1) - y;
                original_alpha = pis.alpha;
                time_at_pick = Time::now();
            }
        }
    }

    if (!ctrl && click && selected_primitive != nullptr) {
        slides[current_slide][selected_primitive].alpha = original_alpha;
        selected_primitive = nullptr;
        wm.Toggle(WindowType::DragAndDrop);
        return;
    }


    bool horizontal = ImGui::IsKeyDown(ImGuiKey_H);
    bool vertical = ImGui::IsKeyDown(ImGuiKey_V);

    if (selected_primitive != nullptr) {

        ImGui::SetNextFrameWantCaptureKeyboard(false);
        ImGui::SetNextFrameWantCaptureMouse(true);
        auto S = ImGui::GetWindowSize();
        auto x = double(io.MousePos.x)/S.x;
        auto y = double(io.MousePos.y)/S.y;
        auto& pis = slides[current_slide][selected_primitive];
        LabelAnchorPtr lab = std::dynamic_pointer_cast<LabelAnchor>(pis.anchor);

        scalar zoom = 1.1;

        if (io.MouseWheel > 0.0f)
            lab->writeScaleAtLabel(lab->getScale()*zoom,true);
        else if (io.MouseWheel < 0.0f){
            lab->writeScaleAtLabel(lab->getScale()/zoom,true);
        }

        if (horizontal) {
            x_offset = 0.5-x;
            x = 0.5;
            lab->writePosAtLabel(x,y+y_offset,true);
        } else if (vertical) {
            y_offset = 0.5-y;
            y = 0.5;
            lab->writePosAtLabel(x+x_offset,y,true);
        } else
            lab->writePosAtLabel(x+x_offset,y+y_offset,true);
        pis.alpha = (std::cos(TimeFrom(time_at_pick)*5) + 1)*0.8 + 0.2;
    }
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

void slope::Slideshow::recordTime()
{
    static TimeStamp last_recorded_time = Time::now();
    auto curr_title = getSlideTitle(current_slide);
    auto dt = TimeFrom(last_recorded_time);
    time_per_slide_group[curr_title] += dt;
    time_from_start += dt;
    last_recorded_time = Time::now();
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

std::string formatTime(float totalSeconds)
{
    int seconds = std::round(totalSeconds);

    int hours = seconds / 3600;
    int minutes = (seconds % 3600) / 60;
    int secs = seconds % 60;

    std::ostringstream out;
    out << std::setw(2) << std::setfill('0') << hours << "h:"
        << std::setw(2) << minutes << "m:"
        << std::setw(2) << secs << "s";

    return out.str();
}

void slope::Slideshow::slideMenu()
{
    ImGui::Begin("Slides");

    // Top timer
    ImGui::Text("Elapsed: %s",
                formatTime(time_from_start).c_str());

    ImGui::Separator();

    std::set<std::string> done;

    if (ImGui::BeginTable("SlideTable", 2,
                          ImGuiTableFlags_SizingStretchProp))
    {
        for (int i = 0; i < slides.size(); i++)
        {
            auto title = getSlideTitle(i);

            if (done.contains(title))
                continue;
            done.insert(title);

            ImGui::TableNextRow();

            // Column 0: button
            ImGui::TableSetColumnIndex(0);
            if (ImGui::Button(title.c_str()))
                goToSlide(i);

            // Column 1: duration
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(
                formatTime(time_per_slide_group[title]).c_str()
                );
        }

        ImGui::EndTable();
    }

    ImGui::End();
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

void slope::Slideshow::saveCamera(std::string file)
{
    std::ofstream camfile(file);
    camfile << removeResolutionFromCamfile(polyscope::view::getCameraJson());
    spdlog::info("current camera view exported at {}",file);
}

void slope::Slideshow::initializeSlides()
{
    precomputeTransitions();
    loadSlides();
    computeFirstSlideNumbers();
    from_begin = Time::now();
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
    slide_numbers.resize(slides.size());
    std::set<std::string> done;

    for (int i = 0;i<slides.size();i++){
        auto title = getSlideTitle(i);
        done.insert(title);
        slide_numbers[i] = done.size()-1;
        time_per_slide_group[title] = 0;
    }
    nb_distinct_slides = done.size();

    spdlog::info("[ number of distinct slides : {} ]",nb_distinct_slides);

    slide_number_display.resize(nb_distinct_slides);
    for (int i = 0;i<nb_distinct_slides;i++){
        auto display = std::to_string(i+1) + "/" + std::to_string(nb_distinct_slides);
        slide_number_display[i] = PlaceBottomRight(Text::Add(display),0.01);
    }
}


slope::PrimitivePtr slope::Slideshow::getPrimitiveUnderMouse(scalar x,scalar y) const
{
    auto io = ImGui::GetIO();
    auto S = ImGui::GetWindowSize();
    for (auto& pis : slides[current_slide].getScreenPrimitives()){
        if (!pis.second.anchor->isPersistent())
            continue;
        auto p = pis.second.getPosition();
        auto prim_size = pis.first->getSize()*pis.second.getScale();
        if (std::abs(p(0) - x) < prim_size(0)/2/S.x && std::abs(p(1) - y) < prim_size(1)/2/S.y)
            return pis.first;
    }
    return nullptr;
}

void slope::Slideshow::displaySlideNumber()
{
    const auto& DSN = slide_number_display[slide_numbers[current_slide]];
    DSN.first->play(TimeObject(),DSN.second);
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

    handleDragAndDrop();
    for (const auto& input : input_manager.getInputs()) {
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
        [this](){if (!locked) nextFrame();},false);
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
        [this](){
            time_from_start = 0;
            for (auto& tpsg : time_per_slide_group)
                tpsg.second = 0;
        },true);

    input_manager.addInput("show transform guizmo editor","T",ImGuiKey_T,true);
    input_manager.addInput("center horizontally dragged primitive","H",ImGuiKey_H,false);
    input_manager.addInput("center vertically dragged primitive","V",ImGuiKey_V,false);
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

    if (wm.isOpen(WindowType::Camera)) {
        ImGui::OpenPopup("Save current camera");

        ImGui::SetNextWindowSizeConstraints(ImVec2(300, 0), ImVec2(FLT_MAX, FLT_MAX));

        if (ImGui::BeginPopupModal("Save current camera", NULL,
                                   ImGuiWindowFlags_AlwaysAutoResize)) {

            static char buffer[256];

            ImVec2 display = ImGui::GetIO().DisplaySize;
            ImGui::SetNextItemWidth(display.x * 0.25f);

            ImGui::InputText("filename", buffer, 256);

            if (ImGui::Button("cancel")) {
                wm.CloseAll();
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            if (ImGui::Button("save")) {
                file = formatCameraFilename(std::string(buffer));

                if (io::file_exists(file) && false) {
                    std::cerr << "FILE ALREADY EXISTS " << buffer << std::endl;
                    return;
                }

                wm.CloseAll();
                saveCamera(file);
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }
    else if (wm.isOpen(WindowType::Palette))
        PaletteHandler::ShowColorPickingModule();
    else if (wm.isOpen(WindowType::SlideMenu))
        slideMenu();
}

