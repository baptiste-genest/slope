#include "Slideshow.h"
#include "content/LateX.h"
#include "spdlog/spdlog.h"
#include "polyscope/pick.h"


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
                    T.transitionParameter = t/transitionTime;
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

    displayPopUps();

    if (LatexLoader::initialized)
        LatexLoader::HotReloadIfModified();


    if (display_slide_number)
        displaySlideNumber();

    ImGui::End();
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
    auto io = ImGui::GetIO();

    static double x_offset = 0;
    static double y_offset = 0;
    static double original_alpha = 0;
    static auto time_at_pick = Time::now();
    static polyscope::PersistentValue<glm::mat4> transform("gizmo transform", glm::mat4(1.0f));
    static polyscope::TransformationGizmo guizmo("slope guizmo",transform.get(),&transform);//("slope transfo",transform);

    bool ctrl = ImGui::IsKeyDown(ImGuiKey_LeftCtrl);
    bool click = io.MouseClicked[0];
//    spdlog::info("ctrl {} click {}",ctrl,click);

    if (ctrl && click && selected_primitive == nullptr){//CTRL {
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
        auto pick = polyscope::pick::pickAtScreenCoords(glm::vec2(io.MousePos.x,io.MousePos.y));
        if (pick.first) {
            spdlog::info("Picked primitive: {}", pick.first->getName());
            pick.first->drawPick();
            transform = pick.first->getTransform();
            guizmo.prepare();
//            guizmo.draw();
        }

    }

    if (!ctrl && click && selected_primitive != nullptr) {
//        spdlog::info("unselected {}",selected_primitive->pid);
        slides[current_slide][selected_primitive].alpha = original_alpha;
        selected_primitive = nullptr;
        return;
    }


    bool horizontal = ImGui::IsKeyDown(ImGuiKey_H);
    bool vertical = ImGui::IsKeyDown(ImGuiKey_V);

    if (selected_primitive != nullptr) {

        ImGui::SetNextFrameWantCaptureKeyboard(false);
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
    return;

    if (!ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || io.MouseReleased[0] > 0){//CTRL {
//    if (!ImGui::IsKeyPressed(ImGuiKey_LeftCtrl) || io.MouseReleased[0] > 0){//CTRL {
        selected_primitive = nullptr;
     //   io.WantCaptureMouse = false;
        ImGui::SetNextFrameWantCaptureMouse(false);

        return;
    }

    ImGui::SetNextFrameWantCaptureMouse(true);
//    io.WantCaptureMouse = true;


    auto S = ImGui::GetWindowSize();
    auto x = double(io.MousePos.x)/S.x;
    auto y = double(io.MousePos.y)/S.y;
    if (selected_primitive == nullptr && io.MouseDown[0] > 0){
        selected_primitive = getPrimitiveUnderMouse(x,y);
        if (selected_primitive != nullptr) {
            auto& pis = slides[current_slide][selected_primitive];
            LabelAnchorPtr lab = std::dynamic_pointer_cast<LabelAnchor>(pis.anchor);
            if (lab != nullptr) {
                x_offset = lab->getPos()(0) - x;
                y_offset = lab->getPos()(1) - y;
            }
        }
    }
    else if (io.MouseReleased[0] > 0. && selected_primitive != nullptr) {
        selected_primitive = nullptr;
    }
    if (io.MouseDown[0] > 0 && selected_primitive != nullptr) {
        spdlog::info("offset {} {}",x_offset,y_offset);
        auto& pis = slides[current_slide][selected_primitive];
        LabelAnchorPtr lab = std::dynamic_pointer_cast<LabelAnchor>(pis.anchor);
        lab->writePosAtLabel(x+x_offset,y+y_offset,true);
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
//    io.WantCaptureMouse = true;
    ImGui::SetNextWindowPos(ImVec2(0,0));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x,io.DisplaySize.y));
    ImGui::SetNextFrameWantCaptureMouse(false);
    ImGui::SetNextFrameWantCaptureKeyboard(true);
}

void slope::Slideshow::init(std::string project_name,int argc,char** argv)
{
    if (parseCLI(argc,argv))
        throw std::runtime_error("exiting for option checking");
    from_action = Time::now();
    from_begin = Time::now();

    slope::Options::ProjectName = project_name;
//    slope::Options::ProjectPath = slope::Options::SlopePath+std::string("/projects/")+slope::Options::ProjectName+std::string("/");

    std::cout << "			[ slope PROJECT : " << slope::Options::ProjectName << " ]" << std::endl;

//    std::cout << "[ slope PATH ] " << slope::Options::SlopePath << std::endl;
    std::cout << "[ PROJECT PATH ] " << slope::Options::ProjectPath << std::endl;
    std::cout << "[ PROJECT DATA PATH ] " << slope::Options::ProjectDataPath << std::endl;
    std::cout << "[ PROJECT CACHE PATH ] " << slope::Options::CachePath << std::endl;
    std::cout << "[ PROJECT VIEWS PATH ] " << slope::Options::ProjectViewsPath << std::endl;
    std::cout << "[ SCREEN RESOLUTION ] " << slope::Options::ScreenResolutionWidth<<"x"<<slope::Options::ScreenResolutionHeight  << std::endl;

    std::cout << "[ KEY GUIDE ] " << std::endl;
    std::cout << "  - right arrow : next slide" << std::endl;
    std::cout << "  - left arrow : previous slide" << std::endl;
    std::cout << "  - down arrow : skip to next slide without transition" << std::endl;
    std::cout << "  - tab : slide menu" << std::endl;
    std::cout << "  - c : export current camera view" << std::endl;
    std::cout << "  - p : take a screenshot" << std::endl;
    std::cout << "  - d : show polyscope interface" << std::endl;
    std::cout << "  - ctrl + left click : drag labeled screen primitives" << std::endl;

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

void slope::Slideshow::slideMenu()
{
    ImGui::Begin("Slides");
    std::set<std::string> done;
    for (int i = 0;i<slides.size();i++){
        auto title = getSlideTitle(i);
        if (done.contains(title))
            continue;
        done.insert(title);
        if (ImGui::Button(title.c_str()))
            goToSlide(i);
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
    from_action = Time::now();
    from_action = Time::now();
}

int slope::Slideshow::getRelativeSlideNumber(Primitive *p)
{
    computeFirstSlideNumbers();
    return p->relativeSlideIndex(slides.size()-1);
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
}

void slope::Slideshow::computeFirstSlideNumbers()
{
    for (int i = 0;const auto& v : slides){
        for (auto& p : v)
            p.first->upFirstSlideNumber(i);
        i++;
    }
}

void slope::Slideshow::loadSlides()
{
    slide_numbers.resize(slides.size());
    std::set<std::string> done;

    for (int i = 0;i<slides.size();i++){
        done.insert(getSlideTitle(i));
        slide_numbers[i] = done.size()-1;
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
        if (!pis.second.anchor->isPersistant())
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

void slope::Slideshow::TransformEditor()
{
    ImGui::Begin("Edit labeled transform");

    // ImGui::BeginChild("accordion_scroll", ImVec2(0,0), false,
    //                   ImGuiWindowFlags_AlwaysVerticalScrollbar);


    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth
                               | ImGuiTreeNodeFlags_FramePadding;

    ImGui::SetWindowFocus();
    ImGui::SetNextFrameWantCaptureMouse(true);

    std::map<std::string,PersistentTransform> unique_transforms;
    for (auto& pis : slides[current_slide].getPolyscopePrimitives()){
        if (!pis.second.persistentTransform.isActive())
            continue;
        unique_transforms[pis.second.persistentTransform.getLabel()] = pis.second.persistentTransform;
    }

    for (const auto& [label,PT] : unique_transforms){
        ImGui::PushID(label.c_str());
        // pis.first->editTransform(pis.second.persistentTransform);
        if (ImGui::CollapsingHeader(label.c_str(), flags))
        {
            ImGui::Indent();
            Transform T = PT.readFromLabel();;
            if (PT.ImGuiInterface(T)) {
                PT.writeAtLabel(T);
            }
            if (ImGui::Button("reset")) {
                PT.writeAtLabel(Transform());
            }
            ImGui::Unindent();
        }
        ImGui::PopID();
    }

    // ImGui::EndChild();

    if (ImGui::Button("close")){
        transform_editor = false;
        // ImGui::SetNextFrameWantCaptureMouse(false);
    }
    // ImGui::SetNextFrameWantCaptureMouse(false);

    ImGui::End();
}

void slope::Slideshow::handleInputs()
{
    handleDragAndDrop();

    if (!keyboardOpen())
        return;

    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) && !locked){
        nextFrame();
    }
    else if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)){
        previousFrame();
    }else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)){
        forceNextFrame();
    }
    if (ImGui::IsKeyDown(ImGuiKey_Tab))
        slideMenu();
    if (ImGui::IsKeyDown(ImGuiKey_T))
        transform_editor = true;
    if (ImGui::IsKeyPressed(ImGuiKey_C)){
        camera_popup = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_D)){
        polyscope::options::buildGui = !polyscope::options::buildGui;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_P)){
        static int screenshot_count = 0;
        constexpr int nb_zeros = 6;
        auto n = std::to_string(screenshot_count++);
        path file =  "/tmp/screenshot_" + std::string(nb_zeros-n.size(),'0') + n + ".png";
        slope::screenshot(file.string());
        spdlog::info("screenshot saved at {}", file.string());
    }
    if (ImGui::IsKeyPressed(ImGuiKey_R)){
        slope::LatexLoader::ReloadContentAndUpdate();
    }

}

void slope::Slideshow::displayPopUps()
{
    std::string file;

    if (camera_popup){
        ImGui::OpenPopup("Save current camera");
        // adapt text to the size of the window
        if (ImGui::BeginPopupModal("Save current camera",NULL,
                                   ImGuiWindowFlags_AlwaysAutoResize)){
            static char buffer[256];
            ImGui::SetWindowFontScale(0.5);
            ImGui::InputText("filename",buffer,256);
            if (ImGui::Button("cancel")){
                camera_popup = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("save")){
                file = formatCameraFilename(std::string(buffer));
                if (io::file_exists(file) && false){
                    std::cerr << "FILE ALREADY EXISTS " << buffer << std::endl;
                    return;
                }
                camera_popup = false;
                saveCamera(file);
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
    if (transform_editor)
    {
        TransformEditor();
    }
}

