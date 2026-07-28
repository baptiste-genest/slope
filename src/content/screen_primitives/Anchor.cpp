#include "Anchor.h"

namespace slope {

AnchorPtr GlobalAnchor = AbsoluteAnchor::Add(vec2(0,0));

void LabelAnchor::writeAtLabel(double x, double y,scalar s, bool overwrite) const
{
    std::error_code ec;
    std::filesystem::create_directories(slope::Options::ProjectViewsPath, ec);
    std::string filepath = slope::Options::ProjectViewsPath + label + ".pos";
    bool exists = io::file_exists(filepath);
    if (!exists || overwrite){
        if (!exists)
            created_labels.insert(label);
        std::ofstream file(filepath);
        if (!file.is_open()){
            spdlog::error("could not open file {}",filepath);
            throw std::runtime_error("could not open file");
        }
        file << x << " " << y << " " << s << std::endl;
    }
}

void LabelAnchor::reportLabelIssues()
{
    // anchors whose .pos had to be created : on a run where no new content was
    // added, these are almost always a mistyped label name
    if (!created_labels.empty()) {
        std::string list;
        for (const auto& l : created_labels)
            list += (list.empty() ? "" : ", ") + l;
        spdlog::info("[labels] {} anchor(s) created at default position: {}",
                     created_labels.size(), list);
    }

    if (!unreadable_labels.empty()) {
        std::string list;
        for (const auto& l : unreadable_labels)
            list += (list.empty() ? "" : ", ") + l;
        spdlog::warn("[labels] {} anchor(s) had an unreadable .pos file: {}",
                     unreadable_labels.size(), list);
    }

    for (const auto& [lbl, n] : label_usage)
        if (n > 1)
            spdlog::warn("[labels] '{}' is used by {} anchors : they will sit on top of each other", lbl, n);

    // .pos files on disk that no anchor refers to any more, e.g. left behind
    // by a renamed or deleted primitive. Never deleted automatically.
    std::error_code ec;
    std::vector<std::string> orphans;
    for (const auto& e : std::filesystem::directory_iterator(slope::Options::ProjectViewsPath, ec)) {
        if (!e.is_regular_file() || e.path().extension() != ".pos")
            continue;
        const auto stem = e.path().stem().string();
        if (!label_usage.contains(stem))
            orphans.push_back(stem);
    }
    if (!orphans.empty()) {
        std::string list;
        for (const auto& l : orphans)
            list += (list.empty() ? "" : ", ") + l;
        spdlog::info("[labels] {} unused .pos file(s) in {}: {}",
                     orphans.size(), slope::Options::ProjectViewsPath, list);
    }
}

void LabelAnchor::writeToSession(double x, double y, scalar scale) const
{
    writeToSessionAt(label, x, y, scale);
}

void LabelAnchor::writeToSessionAt(const std::string& label, double x, double y, scalar scale)
{
    session_cache[label] = {x, y, scale};
    dirty_labels.insert(label);
}

void LabelAnchor::saveAllDirty()
{
    // labels that could not be written stay dirty, so the quit warning keeps
    // firing and a later save can still rescue them
    std::set<std::string> unsaved;
    for (const auto& lbl : dirty_labels) {
        auto it = session_cache.find(lbl);
        if (it == session_cache.end())
            continue;
        const auto& v = it->second;
        std::string filepath = slope::Options::ProjectViewsPath + lbl + ".pos";
        std::ofstream file(filepath);
        if (!file.is_open()) {
            spdlog::error("could not write {}", filepath);
            unsaved.insert(lbl);
            continue;
        }
        file << v[0] << " " << v[1] << " " << v[2] << std::endl;
    }
    dirty_labels = std::move(unsaved);
    if (dirty_labels.empty())
        spdlog::info("positions saved");
    else
        spdlog::error("{} position(s) could not be saved", dirty_labels.size());
}

bool LabelAnchor::hasDirty()
{
    return !dirty_labels.empty();
}

vec LabelAnchor::readFromLabel() const
{
    auto it = session_cache.find(label);
    if (it != session_cache.end()) {
        vec rslt;
        rslt(0) = it->second[0];
        rslt(1) = it->second[1];
        rslt(2) = it->second[2];
        return rslt;
    }

    // this runs from getPos()/getScale(), i.e. several times per primitive per
    // frame : whatever we resolve here must land in the cache, or every frame
    // re-opens the file
    vec rslt;
    std::ifstream file (slope::Options::ProjectViewsPath + label + ".pos");
    if (!file.is_open() || !(file >> rslt(0) >> rslt(1))) {
        // the file is normally created by the constructor ; if it went missing
        // or is truncated, fall back to the same defaults rather than killing
        // the presentation mid-render
        spdlog::error("could not read position of label '{}', using defaults", label);
        unreadable_labels.insert(label);
        rslt(0) = 0.5;
        rslt(1) = 0.5;
        rslt(2) = 1;
        session_cache[label] = {rslt(0), rslt(1), rslt(2)};
        return rslt;
    }

    // check if can read scale
    if (!(file >> rslt(2))){
        rslt(2) = 1;
    }

    session_cache[label] = {rslt(0), rslt(1), rslt(2)};
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
