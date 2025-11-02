#include "Anchor.h"

namespace slope {

AnchorPtr GlobalAnchor = AbsoluteAnchor::Add(vec2(0,0));

void LabelAnchor::writeAtLabel(double x, double y,scalar s, bool overwrite) const
{
    int rslt = system(("mkdir " + slope::Options::ProjectViewsPath + " 2>/dev/null").c_str());
    std::string filepath = slope::Options::ProjectViewsPath + label + ".pos";
    if (!io::file_exists(filepath) || overwrite){
        std::ofstream file(filepath);
        if (!file.is_open()){
            spdlog::error("could not open file {}",filepath);
            throw std::runtime_error("could not open file");
        }
        file << x << " " << y << " " << s << std::endl;
    }
}

vec LabelAnchor::readFromLabel() const
{
    std::ifstream file (slope::Options::ProjectViewsPath + label + ".pos");
    if (!file.is_open()){
        std::cerr << "couldn't read label file" << std::endl;
        exit(1);
    }
    vec rslt;
    file >> rslt(0) >> rslt(1);

    // check if can read scale
    if (!(file >> rslt(2))){
        spdlog::info("could not read scale");
        rslt(2) = 1;
    }

    return rslt;
}

vec2 WorldToScreen(const vec &p) {
    glm::vec4 pos = glm::vec4(p(0),p(1),p(2),1);
    glm::vec4 screenPos = polyscope::view::getCameraPerspectiveMatrix()*polyscope::view::viewMat * pos;
    screenPos /= screenPos.w;
    screenPos = (screenPos + glm::vec4(1,1,1,1))/2.f;
    screenPos.y = 1-screenPos.y;
    return vec2(screenPos.x,screenPos.y);
}

vec ScreenToWorld(const vec2& p) {
    //compute pos such that WorldToScreen(pos) = p
    glm::vec4 pos = glm::vec4(p(0)*2 - 1,1-p(1)*2,0,1);
    glm::mat4 Mat = polyscope::view::getCameraPerspectiveMatrix()*polyscope::view::viewMat;
    glm::vec4 worldPos = glm::inverse(Mat) * pos;
    worldPos /= worldPos.w;
    return vec(worldPos.x,worldPos.y,worldPos.z);
}

}
