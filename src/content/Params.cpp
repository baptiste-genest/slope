#include "Params.h"
#include "io.h"
#include "imgui.h"
#include "spdlog/spdlog.h"

namespace slope {

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

Params::ScalarParam Params::Add(const std::string& name, scalar def, scalar min, scalar max)
{
    auto e = addEntry<ScalarEntry>(name);
    e->value = def;
    e->min = min;
    e->max = max;
    if (file_values.contains(name)) {
        e->fromJson(file_values[name]);
        edited.insert(name);
    }
    return {e};
}

Params::IntParam Params::AddInt(const std::string& name, int def, int min, int max)
{
    auto e = addEntry<IntEntry>(name);
    e->value = def;
    e->min = min;
    e->max = max;
    if (file_values.contains(name)) {
        e->fromJson(file_values[name]);
        edited.insert(name);
    }
    return {e};
}

Params::BoolParam Params::AddBool(const std::string& name, bool def)
{
    auto e = addEntry<BoolEntry>(name);
    e->value = def;
    if (file_values.contains(name)) {
        e->fromJson(file_values[name]);
        edited.insert(name);
    }
    return {e};
}

Params::ColorParam Params::AddColor(const std::string& name, const RGBA& def)
{
    auto e = addEntry<ColorEntry>(name);
    e->value = def;
    if (file_values.contains(name)) {
        e->fromJson(file_values[name]);
        edited.insert(name);
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

// -------------------------------------------------------------------- panel

void Params::DrawPanel()
{
    ImGui::Begin("Animation parameters", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    if (registry.empty())
        ImGui::TextDisabled("no parameter registered (Params::Add)");

    static bool show_all = false;
    ImGui::Checkbox("show all parameters", &show_all);
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
        if (e->drawUI(label.c_str())) {
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

void Params::saveAllDirty()
{
    if (dirty.empty())
        return;
    // only ever-edited parameters are written : the others keep following
    // their code defaults
    for (const auto& name : edited)
        if (registry.count(name))
            file_values[name] = registry[name]->toJson();

    int rslt = system(("mkdir " + Options::ProjectViewsPath + " 2>/dev/null").c_str());
    (void)rslt;
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
