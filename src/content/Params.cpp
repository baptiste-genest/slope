#include "Params.h"
#include "io.h"
#include "imgui.h"
#include "spdlog/spdlog.h"
#include "polyscope/transformation_gizmo.h"
#include "polyscope/view.h"

namespace slope {

// opt-in 3D manipulator for the vec parameters, ownership stays here :
// polyscope's remove() only deregisters the widget
static std::map<std::string, std::shared_ptr<polyscope::TransformationGizmo>> vec_gizmos;

static std::shared_ptr<polyscope::TransformationGizmo> makeVecGizmo(const std::string& name,
                                                                    const vec& value)
{
    auto g = std::shared_ptr<polyscope::TransformationGizmo>(
        new polyscope::TransformationGizmo(name),
        [](polyscope::TransformationGizmo* g) {
            if (g == nullptr)
                return;
            g->setEnabled(false);
            g->remove();
            delete g;
        });
    g->setAllowTranslation(true);
    g->setAllowRotation(false);
    g->setAllowScaling(false);
    g->setPosition(glm::vec3(float(value(0)), float(value(1)), float(value(2))));
    g->setEnabled(true);
    return g;
}

std::map<std::string, Params::EntryPtr> Params::registry;
std::set<std::string> Params::dirty;
std::set<std::string> Params::edited;
json Params::file_values = json::object();
bool Params::file_loaded = false;
long Params::frame = 0;
std::filesystem::file_time_type Params::last_modified;

path Params::file()
{
    return path(Options::ProjectViewsPath) / "params.json";
}

void Params::ensureLoaded()
{
    if (file_loaded)
        return;
    file_loaded = true;
    if (!io::file_exists(file()))
        return;
    try {
        std::ifstream f(file());
        f >> file_values;
        last_modified = std::filesystem::last_write_time(file());
    } catch (const std::exception& e) {
        spdlog::warn("could not read {} : {}", file().string(), e.what());
        file_values = json::object();
    }
}

template<class E>
std::shared_ptr<E> Params::addEntry(const std::string& name)
{
    ensureLoaded();
    auto it = registry.find(name);
    if (it != registry.end()) {
        auto e = std::dynamic_pointer_cast<E>(it->second);
        if (!e)
            throw std::runtime_error("parameter \"" + name + "\" already registered with another type");
        return e;
    }
    auto e = std::make_shared<E>();
    e->name = name;
    registry[name] = e;
    return e;
}

bool Params::keepEditedValue(const std::string& name)
{
    return dirty.count(name) > 0;
}

void Params::applyFileValue(const EntryPtr& e, const std::string& name)
{
    if (!file_values.contains(name))
        return;
    e->fromJson(file_values[name]);
    edited.insert(name);
}

Params::ScalarParam Params::Add(const std::string& name, scalar def, scalar min, scalar max)
{
    auto e = addEntry<ScalarEntry>(name);
    e->min = min;
    e->max = max;
    if (!keepEditedValue(name)) {
        e->value = def;
        applyFileValue(e, name);
    }
    return {e};
}

Params::IntParam Params::AddInt(const std::string& name, int def, int min, int max)
{
    auto e = addEntry<IntEntry>(name);
    e->min = min;
    e->max = max;
    if (!keepEditedValue(name)) {
        e->value = def;
        applyFileValue(e, name);
    }
    return {e};
}

Params::BoolParam Params::AddBool(const std::string& name, bool def)
{
    auto e = addEntry<BoolEntry>(name);
    if (!keepEditedValue(name)) {
        e->value = def;
        applyFileValue(e, name);
    }
    return {e};
}

Params::ColorParam Params::AddColor(const std::string& name, const RGBA& def)
{
    auto e = addEntry<ColorEntry>(name);
    if (!keepEditedValue(name)) {
        e->value = def;
        applyFileValue(e, name);
    }
    return {e};
}

Params::Vec2Param Params::AddVec2(const std::string& name, const vec2& def,
                                  scalar min, scalar max)
{
    auto e = addEntry<Vec2Entry>(name);
    e->min = min;
    e->max = max;
    if (!keepEditedValue(name)) {
        e->value = def;
        applyFileValue(e, name);
    }
    return {e};
}

Params::VecParam Params::AddVec(const std::string& name, const vec& def,
                                scalar min, scalar max)
{
    auto e = addEntry<VecEntry>(name);
    e->min = min;
    e->max = max;
    if (!keepEditedValue(name)) {
        e->value = def;
        applyFileValue(e, name);
    }
    return {e};
}

Params::DirParam Params::AddDir(const std::string& name, const vec& def)
{
    auto e = addEntry<DirEntry>(name);
    if (!keepEditedValue(name)) {
        e->value = def.norm() > 1e-9 ? vec(def.normalized()) : vec(0,0,1);
        applyFileValue(e, name);
    }
    return {e};
}

scalar Params::get(const std::string& name, scalar def, scalar min, scalar max)
{
    auto it = registry.find(name);
    if (it != registry.end()) {
        it->second->last_read = frame;
        return std::static_pointer_cast<ScalarEntry>(it->second)->value;
    }
    return Add(name, def, min, max);
}

// ------------------------------------------------------------------ widgets

bool Params::ScalarEntry::drawUI(const char* label)
{
    float f = value;
    bool changed = (min < max)
        ? ImGui::SliderFloat(label, &f, min, max)
        : ImGui::DragFloat(label, &f, 0.01f);
    if (changed)
        value = f;
    return changed;
}

bool Params::IntEntry::drawUI(const char* label)
{
    return (min < max)
        ? ImGui::SliderInt(label, &value, min, max)
        : ImGui::DragInt(label, &value);
}

bool Params::BoolEntry::drawUI(const char* label)
{
    return ImGui::Checkbox(label, &value);
}

bool Params::ColorEntry::drawUI(const char* label)
{
    return ImGui::ColorEdit4(label, (float*)&value.Value);
}

json Params::ColorEntry::toJson() const
{
    return {value.Value.x, value.Value.y, value.Value.z, value.Value.w};
}

void Params::ColorEntry::fromJson(const json& j)
{
    value = RGBA((float)j[0], (float)j[1], (float)j[2],
                 j.size() > 3 ? (float)j[3] : 1.f);
}

bool Params::Vec2Entry::drawUI(const char* label)
{
    float f[2] = {float(value(0)), float(value(1))};
    bool changed = (min < max)
        ? ImGui::SliderFloat2(label, f, float(min), float(max))
        : ImGui::DragFloat2(label, f, 0.01f);
    if (changed)
        value = vec2(f[0], f[1]);
    return changed;
}

json Params::Vec2Entry::toJson() const
{
    return {value(0), value(1)};
}

void Params::Vec2Entry::fromJson(const json& j)
{
    value = vec2((scalar)j[0], (scalar)j[1]);
}

bool Params::VecEntry::drawUI(const char* label)
{
    float f[3] = {float(value(0)), float(value(1)), float(value(2))};
    bool changed = (min < max)
        ? ImGui::SliderFloat3(label, f, float(min), float(max))
        : ImGui::DragFloat3(label, f, 0.01f);
    if (changed)
        value = vec(f[0], f[1], f[2]);
    return changed;
}

json Params::VecEntry::toJson() const
{
    return {value(0), value(1), value(2)};
}

void Params::VecEntry::fromJson(const json& j)
{
    value = vec((scalar)j[0], (scalar)j[1], (scalar)j[2]);
}

// the screen counterpart of the gizmo : a vec2 parameter grabbed where it
// acts, in 0..1 across the window with y up — gl_FragCoord's convention, so a
// full screen shader reads the handle's position as its own uv
static std::set<std::string> vec2_handles;
static std::string dragged_handle;

static ImVec2 handlePixel(const vec2& value)
{
    ImVec2 d = ImGui::GetIO().DisplaySize;
    return ImVec2(float(value(0)) * d.x, float(1.0 - value(1)) * d.y);
}

static vec2 handleValue(const ImVec2& p)
{
    ImVec2 d = ImGui::GetIO().DisplaySize;
    return vec2(std::clamp(scalar(p.x / d.x), scalar(0), scalar(1)),
                std::clamp(scalar(1.f - p.y / d.y), scalar(0), scalar(1)));
}

// toggle and drag, true when the handle moved the value
static bool drawVec2Handle(const std::string& name, vec2& value)
{
    auto it = vec2_handles.find(name);
    bool active = it != vec2_handles.end();

    ImGui::SameLine();
    if (ImGui::SmallButton(active ? "2D*" : "2D")) {
        if (active) {
            vec2_handles.erase(it);
            if (dragged_handle == name)
                dragged_handle.clear();
        } else {
            vec2_handles.insert(name);
        }
        return false;
    }
    if (!active)
        return false;

    const ImVec2 c = handlePixel(value);
    const ImVec2 m = ImGui::GetIO().MousePos;
    const float  r = 9.f;
    bool over = (m.x - c.x) * (m.x - c.x) + (m.y - c.y) * (m.y - c.y) < 4.f * r * r;

    bool changed = false;
    if (dragged_handle == name) {
        if (ImGui::IsMouseDown(0)) {
            vec2 moved = handleValue(m);
            changed = (moved - value).norm() > 1e-6;
            value = moved;
        } else {
            dragged_handle.clear();
        }
    } else if (over && dragged_handle.empty() && ImGui::IsMouseClicked(0)) {
        dragged_handle = name;
    }

    bool live = over || dragged_handle == name;
    if (live) // the camera must not spin under the drag
        ImGui::SetNextFrameWantCaptureMouse(true);

    auto* dl = ImGui::GetForegroundDrawList();
    ImU32 col = ImGui::GetColorU32(live ? ImGuiCol_ButtonHovered : ImGuiCol_Text);
    dl->AddCircle(c, r, col, 24, 2.f);
    dl->AddLine(ImVec2(c.x - r * 1.7f, c.y), ImVec2(c.x + r * 1.7f, c.y), col, 1.f);
    dl->AddLine(ImVec2(c.x, c.y - r * 1.7f), ImVec2(c.x, c.y + r * 1.7f), col, 1.f);
    dl->AddText(ImVec2(c.x + r * 1.9f, c.y - r * 1.9f), col, name.c_str());

    return changed;
}

static void enableGizmo(const std::string& name, const vec& value)
{
    if (!vec_gizmos.count(name))
        vec_gizmos[name] = makeVecGizmo("param " + name, value);
}

// gizmo toggle and two way sync, true when the gizmo moved the value
static bool drawVecGizmoLine(const std::string& name, vec& value, bool value_changed)
{
    auto it = vec_gizmos.find(name);
    bool active = it != vec_gizmos.end();

    ImGui::SameLine();
    if (ImGui::SmallButton(active ? "3D*" : "3D")) {
        if (active) {
            vec_gizmos.erase(it); // the deleter disables and deregisters it
            return false;
        }
        vec_gizmos[name] = makeVecGizmo("param " + name, value);
        return false;
    }
    if (!active)
        return false;

    auto& g = it->second;
    if (value_changed) {
        g->setPosition(glm::vec3(float(value(0)), float(value(1)), float(value(2))));
        return false;
    }

    glm::vec3 p = g->getPosition();
    vec moved((scalar)p.x, (scalar)p.y, (scalar)p.z);
    if ((moved - value).norm() < 1e-6) // float round trip is not an edit
        return false;
    value = moved;
    return true;
}

// ------------------------------------------------------------ direction ball

// the unit sphere seen from the current camera : x right, y up, z toward the
// viewer, so the ball is oriented like the scene behind it
static glm::mat3 viewRotation()
{
    return glm::mat3(polyscope::view::getCameraViewMatrix());
}

static ImVec2 toDisc(const glm::vec3& v, const ImVec2& center, float radius)
{
    return ImVec2(center.x + v.x * radius, center.y - v.y * radius);
}

bool Params::DirEntry::drawUI(const char* label)
{
    const float size = ImGui::GetFrameHeight() * 3.f;
    const ImVec2 origin = ImGui::GetCursorScreenPos();

    ImGui::InvisibleButton("##ball", ImVec2(size, size));
    const ImVec2 center(origin.x + size * 0.5f, origin.y + size * 0.5f);
    const float radius = size * 0.5f - 2.f;

    glm::mat3 R = viewRotation();
    glm::vec3 d = R * glm::vec3(float(value(0)), float(value(1)), float(value(2)));

    bool changed = false;
    if (ImGui::IsItemActive()) { // held : a click aims as well as a drag
        ImVec2 m = ImGui::GetMousePos();
        glm::vec2 p((m.x - center.x) / radius, (center.y - m.y) / radius);
        float r2 = glm::dot(p, p);
        if (r2 > 1.f) { // past the rim : slide along the silhouette
            p /= std::sqrt(r2);
            r2 = 1.f;
        }
        float z = std::sqrt(std::max(0.f, 1.f - r2)) * (d.z < 0 ? -1.f : 1.f);
        d = glm::normalize(glm::vec3(p.x, p.y, z));
        glm::vec3 w = glm::transpose(R) * d;
        value = vec(w.x, w.y, w.z);
        changed = true;
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(1)) { // the hidden axis
        value = -value;
        d = -d;
        changed = true;
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("drag to aim, right click to flip");

    auto* dl = ImGui::GetWindowDrawList();
    const ImU32 rim  = ImGui::GetColorU32(ImGuiCol_Text, 0.45f);
    const ImU32 fill = ImGui::GetColorU32(ImGuiCol_FrameBg);
    const ImU32 col  = ImGui::GetColorU32(ImGuiCol_Text);
    dl->AddCircleFilled(center, radius, fill, 48);
    dl->AddCircle(center, radius, rim, 48);

    static const glm::vec3 axes[3] = {{1,0,0},{0,1,0},{0,0,1}};
    static const ImU32 axis_cols[3] = {IM_COL32(220,80,80,180), IM_COL32(80,200,80,180),
                                       IM_COL32(90,120,230,180)};
    for (int i = 0; i < 3; i++) {
        glm::vec3 a = R * axes[i];
        dl->AddLine(center, toDisc(a * 0.85f, center, radius), axis_cols[i], 1.5f);
    }

    ImVec2 tip = toDisc(d, center, radius);
    dl->AddLine(center, tip, col, 2.f);
    if (d.z >= 0)
        dl->AddCircleFilled(tip, 4.f, col, 16);
    else
        dl->AddCircle(tip, 4.f, col, 16, 2.f);

    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::TextUnformatted(label);
    ImGui::TextDisabled("%.2f %.2f %.2f", value(0), value(1), value(2));
    ImGui::EndGroup();

    return changed;
}

json Params::DirEntry::toJson() const
{
    return {value(0), value(1), value(2)};
}

void Params::DirEntry::fromJson(const json& j)
{
    vec v((scalar)j[0], (scalar)j[1], (scalar)j[2]);
    value = v.norm() > 1e-9 ? vec(v.normalized()) : vec(0,0,1);
}

// -------------------------------------------------------------------- panel

void Params::DrawPanel()
{
    ImGui::Begin("Animation parameters", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    if (registry.empty())
        ImGui::TextDisabled("no parameter registered (Params::Add)");

    static bool show_all = false;
    ImGui::Checkbox("show all parameters", &show_all);
    // the handles of every vec2 / vec parameter listed below, at once
    ImGui::SameLine();
    ImGui::TextDisabled("handles");
    ImGui::SameLine();
    bool handles_on = ImGui::SmallButton("all");
    ImGui::SameLine();
    if (ImGui::SmallButton("none"))
        clearGizmos();
    ImGui::Separator();

    bool any_shown = false;
    std::string current_group;
    bool group_open = true;
    for (auto& [name, e] : registry) {
        // a parameter "appears in the current slide" when an updater read
        // its value in one of the last frames
        if (!show_all && e->last_read < frame - 2)
            continue;
        any_shown = true;
        auto slash = name.find('/');
        std::string group = slash == std::string::npos ? "" : name.substr(0, slash);
        std::string label = slash == std::string::npos ? name : name.substr(slash + 1);
        if (group != current_group || (group.empty() && !current_group.empty())) {
            current_group = group;
            group_open = group.empty()
                ? true
                : ImGui::CollapsingHeader(group.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
        }
        if (!group_open)
            continue;
        ImGui::PushID(name.c_str());
        bool changed = e->drawUI(label.c_str());
        if (auto v = std::dynamic_pointer_cast<VecEntry>(e)) {
            if (handles_on)
                enableGizmo(name, v->value);
            changed |= drawVecGizmoLine(name, v->value, changed);
        } else if (auto v2 = std::dynamic_pointer_cast<Vec2Entry>(e)) {
            if (handles_on)
                vec2_handles.insert(name);
            changed |= drawVec2Handle(name, v2->value);
        }
        if (changed) {
            dirty.insert(name);
            edited.insert(name);
        }
        ImGui::PopID();
    }

    if (!any_shown && !registry.empty())
        ImGui::TextDisabled("no parameter used in this slide");

    if (!dirty.empty()) {
        ImGui::Separator();
        ImGui::TextDisabled("Ctrl+S to save %d change(s)", (int)dirty.size());
    }

    // keep the camera still while tweaking : ImGui must capture the mouse
    // when it interacts with this panel (slope forces the capture off each
    // frame so that clicks reach the slides / polyscope)
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)
        || ImGui::IsAnyItemActive())
        ImGui::SetNextFrameWantCaptureMouse(true);

    ImGui::End();
}

// -------------------------------------------------------------- persistence

bool Params::hasDirty()
{
    return !dirty.empty();
}

bool Params::hasLiveGizmo()
{
    return !vec_gizmos.empty() || !vec2_handles.empty();
}

void Params::clearGizmos()
{
    vec_gizmos.clear(); // the deleters disable and deregister the widgets
    vec2_handles.clear();
    dragged_handle.clear();
}

void Params::saveAllDirty()
{
    if (dirty.empty())
        return;
    // only ever-edited parameters are written : the others keep following
    // their code defaults
    for (const auto& name : edited)
        if (registry.count(name))
            file_values[name] = registry[name]->toJson();

    // std::filesystem, no shell needed and cmd.exe has no "2>/dev/null"
    std::error_code mkdir_ec;
    std::filesystem::create_directories(Options::ProjectViewsPath, mkdir_ec);
    std::ofstream f(file());
    if (!f.is_open()) {
        spdlog::error("could not write {}", file().string());
        return;
    }
    f << file_values.dump(1) << std::endl;
    f.close();
    // remember our own write so the hot-reload watcher ignores it
    try {
        last_modified = std::filesystem::last_write_time(file());
    } catch (const std::exception&) {}
    dirty.clear();
    spdlog::info("parameters saved");
}

void Params::HotReloadIfModified()
{
    if (!file_loaded || registry.empty())
        return;
    static auto last_refresh = Time::now();
    if (TimeFrom(last_refresh) < 0.2)
        return;
    last_refresh = Time::now();
    try {
        if (!io::file_exists(file()))
            return;
        auto last_write = std::filesystem::last_write_time(file());
        if (!(last_modified < last_write))
            return;
        last_modified = last_write;
        std::ifstream f(file());
        f >> file_values;
        for (auto& [name, e] : registry)
            if (file_values.contains(name)) {
                e->fromJson(file_values[name]);
                edited.insert(name);
                dirty.erase(name);
            }
        spdlog::info("parameters reloaded from {}", file().string());
    } catch (const std::exception& e) {
        spdlog::warn("params file unavailable or invalid : {}", e.what());
    }
}

}
