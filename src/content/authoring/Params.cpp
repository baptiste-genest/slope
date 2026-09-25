#include "content/authoring/Params.h"
#include "content/config/io.h"
#include "content/config/ReloadErrors.h"
#include "imgui.h"
#include "spdlog/spdlog.h"
#include "polyscope/transformation_gizmo.h"
#include "polyscope/view.h"
#include "polyscope/options.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>
#include <cfloat>

namespace slope {

// opt-in 3D manipulator for the vec parameters, ownership stays here since
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
std::set<std::string> Params::auto_manipulators;
std::set<std::string> Params::edited;
json Params::file_values = json::object();
bool Params::file_loaded = false;
long Params::frame = 0;
std::filesystem::file_time_type Params::last_modified;

namespace {
// an unreadable params.json is never written over, it may hold every tuned value
bool file_broken = false;
}

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
        json read;
        f >> read;
        file_values = std::move(read);
        last_modified = std::filesystem::last_write_time(file());
    } catch (const std::exception& e) {
        // first read may come mid frame, so it is reported rather than thrown
        const std::string msg = "cannot read " + file().string() + " : " + e.what()
                              + ", parameters keep their defaults and are not saved until it is fixed";
        spdlog::error("{}", msg);
        ReloadErrors::report(file(), "params", msg);
        file_broken = true;
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

// Stamps last_read like a handle would, so the Tuner still shows it as used.
int Params::components(const std::string& name)
{
    auto it = registry.find(name);
    if (it == registry.end())
        return 0;
    if (std::dynamic_pointer_cast<Vec2Entry>(it->second))  return 2;
    if (std::dynamic_pointer_cast<VecEntry>(it->second))   return 3;
    if (std::dynamic_pointer_cast<DirEntry>(it->second))   return 3;
    if (std::dynamic_pointer_cast<ColorEntry>(it->second)) return 4;
    return 1;
}

int Params::read(const std::string& name, scalar* out4)
{
    auto it = registry.find(name);
    if (it == registry.end())
        return 0;
    const EntryPtr& e = it->second;
    e->last_read = frame;

    if (auto p = std::dynamic_pointer_cast<Vec2Entry>(e)) {
        out4[0] = p->value(0); out4[1] = p->value(1);
        return 2;
    }
    if (auto p = std::dynamic_pointer_cast<VecEntry>(e)) {
        for (int i = 0; i < 3; i++) out4[i] = p->value(i);
        return 3;
    }
    if (auto p = std::dynamic_pointer_cast<DirEntry>(e)) {
        for (int i = 0; i < 3; i++) out4[i] = p->value(i);
        return 3;
    }
    if (auto p = std::dynamic_pointer_cast<ColorEntry>(e)) {
        out4[0] = p->value.Value.x; out4[1] = p->value.Value.y;
        out4[2] = p->value.Value.z; out4[3] = p->value.Value.w;
        return 4;
    }
    if (auto p = std::dynamic_pointer_cast<ScalarEntry>(e)) { out4[0] = p->value; return 1; }
    if (auto p = std::dynamic_pointer_cast<IntEntry>(e))    { out4[0] = p->value; return 1; }
    if (auto p = std::dynamic_pointer_cast<BoolEntry>(e))   { out4[0] = p->value ? 1 : 0; return 1; }
    if (auto p = std::dynamic_pointer_cast<EnumEntry>(e))   { out4[0] = p->value; return 1; }
    return 0;
}

int Params::write(const std::string& name, const scalar* in, int n)
{
    auto it = registry.find(name);
    if (it == registry.end())
        return 0;
    const EntryPtr& e = it->second;
    const int comps = components(name);
    if (n < comps) {
        spdlog::warn("parameter \"{}\" has {} components, {} given", name, comps, n);
        return 0;
    }
    // a driven parameter is a used one, the Tuner shows it like a read does
    e->last_read = frame;

    auto written = [&]{ e->onWritten(); return comps; };
    if (auto p = std::dynamic_pointer_cast<Vec2Entry>(e))  { p->value = vec2(in[0], in[1]); return written(); }
    if (auto p = std::dynamic_pointer_cast<VecEntry>(e))   { p->value = vec(in[0], in[1], in[2]); return written(); }
    if (auto p = std::dynamic_pointer_cast<DirEntry>(e))   { p->value = vec(in[0], in[1], in[2]); return written(); }
    if (auto p = std::dynamic_pointer_cast<ColorEntry>(e)) {
        p->value = RGBA(float(in[0]), float(in[1]), float(in[2]), float(in[3]));
        return written();
    }
    if (auto p = std::dynamic_pointer_cast<ScalarEntry>(e)) { p->value = in[0]; return written(); }
    if (auto p = std::dynamic_pointer_cast<IntEntry>(e))    { p->value = int(std::lround(in[0])); return written(); }
    if (auto p = std::dynamic_pointer_cast<BoolEntry>(e))   { p->value = in[0] != 0; return written(); }
    if (auto p = std::dynamic_pointer_cast<EnumEntry>(e))   { p->value = int(std::lround(in[0])); return written(); }
    return 0;
}

bool Params::write(const std::string& name, scalar v)
{
    return write(name, &v, 1) > 0;
}

bool Params::write(const std::string& name, const vec2& v)
{
    const scalar in[2] = {v(0), v(1)};
    return write(name, in, 2) > 0;
}

bool Params::write(const std::string& name, const vec& v)
{
    const scalar in[3] = {v(0), v(1), v(2)};
    return write(name, in, 3) > 0;
}

bool Params::write(const std::string& name, const RGBA& v)
{
    const scalar in[4] = {v.Value.x, v.Value.y, v.Value.z, v.Value.w};
    return write(name, in, 4) > 0;
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

Params::ScalarParam Params::AddAtLeast(const std::string& name, scalar def, scalar min)
{
    auto e = addEntry<ScalarEntry>(name);
    e->min = min;
    e->max = min;
    e->open_max = true;
    if (!keepEditedValue(name)) {
        e->value = def;
        applyFileValue(e, name);
    }
    e->onWritten();
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

Params::EnumParam Params::AddEnum(const std::string& name,
                                  std::vector<std::string> options,
                                  const std::string& def)
{
    auto e = addEntry<EnumEntry>(name);
    // re-declaring keeps the option held, when the new list still has it
    const std::string held = e->choice();
    e->options = std::move(options);
    auto index = [&](const std::string& want) {
        for (std::size_t i = 0; i < e->options.size(); i++)
            if (e->options[i] == want) return int(i);
        return -1;
    };
    if (index(def) < 0 && !e->options.empty())
        throw std::runtime_error("parameter \"" + name + "\" defaults to \"" + def
                                 + "\", which is not one of its options");
    if (!keepEditedValue(name)) {
        e->value = std::max(0, index(def));
        applyFileValue(e, name);
    } else if (const int keep = index(held); keep >= 0) {
        e->value = keep;
    }
    e->onWritten();
    return {e};
}

bool Params::drive(const std::string& name, const json& value)
{
    auto it = registry.find(name);
    if (it == registry.end())
        return false;
    try {
        it->second->fromJson(value);
    } catch (const json::exception&) {
        throw std::runtime_error("parameter \"" + name + "\" cannot take the "
                                 "value " + value.dump());
    }
    it->second->onWritten();
    // a driven parameter is a used one, the Tuner shows it like a read does
    it->second->last_read = frame;
    return true;
}

json Params::valueOf(const std::string& name)
{
    auto it = registry.find(name);
    return it == registry.end() ? json() : it->second->toJson();
}

bool Params::setDefault(const std::string& name, const json& value)
{
    if (!registry.count(name))
        return false;
    // an unsaved edit is the newest value there is, and outranks a default
    if (!keepEditedValue(name)) {
        drive(name, value);
        applyFileValue(registry[name], name);   // ... a saved value outranks both
    }
    return true;
}

std::string Params::choice(const std::string& name)
{
    auto it = registry.find(name);
    if (it == registry.end()) return {};
    auto e = std::dynamic_pointer_cast<EnumEntry>(it->second);
    if (!e) return {};
    e->last_read = frame;
    return e->choice();
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

static scalar clampToBounds(scalar v, scalar min, scalar max)
{
    return min < max ? std::min(std::max(v, min), max) : v;
}

bool Params::ScalarEntry::drawUI(const char* label)
{
    float f = value;
    bool changed = open_max
        ? ImGui::DragFloat(label, &f, 0.01f, float(min), FLT_MAX, "%.3f",
                           ImGuiSliderFlags_AlwaysClamp)
        : (min < max) ? ImGui::SliderFloat(label, &f, min, max)
                      : ImGui::DragFloat(label, &f, 0.01f);
    if (changed)
        value = f;
    return changed;
}

void Params::ScalarEntry::onWritten()
{
    if (open_max) value = std::max(value, min);
    else          value = clampToBounds(value, min, max);
}

bool Params::IntEntry::drawUI(const char* label)
{
    return (min < max)
        ? ImGui::SliderInt(label, &value, min, max)
        : ImGui::DragInt(label, &value);
}

void Params::IntEntry::onWritten()
{
    if (min < max)
        value = std::min(std::max(value, min), max);
}

bool Params::BoolEntry::drawUI(const char* label)
{
    return ImGui::Checkbox(label, &value);
}

const std::string& Params::EnumEntry::choice() const
{
    static const std::string none;
    return (value >= 0 && value < int(options.size())) ? options[std::size_t(value)] : none;
}

bool Params::EnumEntry::drawUI(const char* label)
{
    std::vector<const char*> names;
    names.reserve(options.size());
    for (const auto& o : options) names.push_back(o.c_str());
    return ImGui::Combo(label, &value, names.data(), int(names.size()));
}

void Params::EnumEntry::onWritten()
{
    value = std::min(std::max(value, 0), int(options.size()) - 1);
}

// by the name it holds, so a saved file survives the list being reordered
json Params::EnumEntry::toJson() const { return choice(); }

void Params::EnumEntry::fromJson(const json& j)
{
    if (j.is_number_integer()) { value = j.get<int>(); onWritten(); return; }
    if (!j.is_string()) return;
    const std::string want = j.get<std::string>();
    for (std::size_t i = 0; i < options.size(); i++)
        if (options[i] == want) { value = int(i); return; }
    // an option that no longer exists, left on what the code declared
    spdlog::warn("parameter \"{}\" has no option \"{}\" any more", name, want);
}

bool Params::ColorEntry::drawUI(const char* label)
{
    // a swatch on the row, the wheel in the popup it opens. The inline drag
    // fields are dropped, a panel of colours is unreadable with four each
    return ImGui::ColorEdit4(label, (float*)&value.Value,
                             ImGuiColorEditFlags_NoInputs
                             | ImGuiColorEditFlags_AlphaBar
                             | ImGuiColorEditFlags_AlphaPreviewHalf
                             | ImGuiColorEditFlags_PickerHueWheel);
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

void Params::Vec2Entry::onWritten()
{
    for (int i = 0; i < 2; i++)
        value(i) = clampToBounds(value(i), min, max);
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

void Params::VecEntry::onWritten()
{
    for (int i = 0; i < 3; i++)
        value(i) = clampToBounds(value(i), min, max);
    // the 3D manipulator only follows the value when the panel moves it
    auto g = vec_gizmos.find(name);
    if (g != vec_gizmos.end())
        g->second->setPosition(glm::vec3(float(value(0)), float(value(1)), float(value(2))));
}

json Params::VecEntry::toJson() const
{
    return {value(0), value(1), value(2)};
}

void Params::VecEntry::fromJson(const json& j)
{
    value = vec((scalar)j[0], (scalar)j[1], (scalar)j[2]);
}

// the screen counterpart of the gizmo, a vec2 parameter grabbed where it acts,
// in 0..1 with y up like gl_FragCoord, so a shader reads it as its own uv
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

// the panel's toggle, the handle itself is drawn by DrawVisible
static void vec2HandleButton(const std::string& name)
{
    auto it = vec2_handles.find(name);
    bool active = it != vec2_handles.end();

    ImGui::SameLine();
    if (!ImGui::SmallButton(active ? "2D*" : "2D"))
        return;
    if (active) {
        vec2_handles.erase(it);
        if (dragged_handle == name)
            dragged_handle.clear();
    } else {
        vec2_handles.insert(name);
    }
}

// drag and draw, true when the handle moved the value
static bool dragVec2Handle(const std::string& name, vec2& value)
{
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

// where the manipulator sits on screen, so the mouse is yielded to it there
// and nowhere else
static bool cursorNearGizmo(const vec& world)
{
    glm::mat4 VP = polyscope::view::getCameraPerspectiveMatrix()
                   * polyscope::view::getCameraViewMatrix();
    glm::vec4 clip = VP * glm::vec4(float(world(0)), float(world(1)), float(world(2)), 1.f);
    if (clip.w <= 0)
        return false;
    const ImVec2 d = ImGui::GetIO().DisplaySize;
    const ImVec2 p((clip.x / clip.w * 0.5f + 0.5f) * d.x,
                   (0.5f - clip.y / clip.w * 0.5f) * d.y);
    const ImVec2 m = ImGui::GetIO().MousePos;
    // the arrows reach about a tenth of the height, times polyscope's ui scale
    const float r = std::clamp(0.1f * d.y * float(polyscope::options::uiScale), 60.f, 160.f);
    return (m.x - p.x) * (m.x - p.x) + (m.y - p.y) * (m.y - p.y) < r * r;
}

static bool cursor_on_gizmo = false;

static void enableGizmo(const std::string& name, const vec& value)
{
    if (!vec_gizmos.count(name))
        vec_gizmos[name] = makeVecGizmo("param " + name, value);
}

// the panel's toggle, the widget is polyscope's and lives until it is dropped
static void vecGizmoButton(const std::string& name, const vec& value)
{
    auto it = vec_gizmos.find(name);
    bool active = it != vec_gizmos.end();

    ImGui::SameLine();
    if (!ImGui::SmallButton(active ? "3D*" : "3D"))
        return;
    if (active)
        vec_gizmos.erase(it); // the deleter disables and deregisters it
    else
        vec_gizmos[name] = makeVecGizmo("param " + name, value);
}

// reads the manipulator back, true when it moved the value
static bool syncVecGizmo(const std::string& name, vec& value)
{
    auto it = vec_gizmos.find(name);
    if (it == vec_gizmos.end())
        return false;

    auto& g = it->second;
    glm::vec3 p = g->getPosition();
    vec moved((scalar)p.x, (scalar)p.y, (scalar)p.z);
    if ((moved - value).norm() < 1e-6) // float round trip is not an edit
        return false;
    value = moved;
    return true;
}

// ------------------------------------------------------------ direction ball

// the unit sphere seen from the current camera, oriented like the scene
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
    if (ImGui::IsItemActive()) { // held, a click aims as well as a drag
        ImVec2 m = ImGui::GetMousePos();
        glm::vec2 p((m.x - center.x) / radius, (center.y - m.y) / radius);
        float r2 = glm::dot(p, p);
        if (r2 > 1.f) { // past the rim, slide along the silhouette
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

void Params::DirEntry::onWritten()
{
    value = value.norm() > 1e-9 ? vec(value.normalized()) : vec(0,0,1);
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

// ------------------------------------------------------------------ visible

void Params::setVisible(const std::string& name, Visible v)
{
    auto it = registry.find(name);
    if (it == registry.end())
        throw std::runtime_error("parameter \"" + name + "\" is not registered, nothing to show");
    it->second->vis = v;
}

Params::Visible Params::getVisible(const std::string& name)
{
    auto it = registry.find(name);
    return it == registry.end() ? Visible::None : it->second->vis;
}

Params::Visible Params::parseVisible(const std::string& mode)
{
    if (mode == "none")   return Visible::None;
    if (mode == "panel")  return Visible::Panel;
    if (mode == "handle") return Visible::Handle;
    if (mode == "both")   return Visible::Both;
    throw std::runtime_error("unknown visibility \"" + mode
                             + "\" (none/panel/handle/both)");
}

void Params::DrawVisible(bool panel_open)
{
    std::vector<std::pair<std::string, EntryPtr>> rows;
    bool near_gizmo = false;

    for (auto& [name, e] : registry) {
        // same reading as the panel's, an updater read it in the last frames
        const bool used = e->last_read >= frame - 2;
        const bool wants_handle = used && (e->vis == Visible::Handle || e->vis == Visible::Both);
        auto v  = std::dynamic_pointer_cast<VecEntry>(e);
        auto v2 = std::dynamic_pointer_cast<Vec2Entry>(e);
        const bool manipulable = v || v2;

        if (wants_handle && manipulable) {
            if (auto_manipulators.insert(name).second) {
                if (v)  enableGizmo(name, v->value);
                if (v2) vec2_handles.insert(name);
            }
        } else if (auto_manipulators.erase(name)) {
            vec_gizmos.erase(name);
            vec2_handles.erase(name);
            if (dragged_handle == name)
                dragged_handle.clear();
        }

        // every manipulator on screen is read here, the panel only toggles them
        bool changed = false;
        if (v) {
            if (vec_gizmos.count(name))
                near_gizmo |= cursorNearGizmo(v->value);
            changed = syncVecGizmo(name, v->value);
        }
        else if (v2 && vec2_handles.count(name))
            changed = dragVec2Handle(name, v2->value);
        if (changed) {
            dirty.insert(name);
            edited.insert(name);
        }

        // a type with no manipulator falls back to its widget
        if (used && (e->vis == Visible::Panel || e->vis == Visible::Both
                     || (e->vis == Visible::Handle && !manipulable)))
            rows.emplace_back(name, e);
    }

    // a drag that wanders off the gizmo keeps the mouse until it is released
    cursor_on_gizmo = near_gizmo || (cursor_on_gizmo && ImGui::IsMouseDown(0));

    if (panel_open || rows.empty())
        return;

    // wide enough for the title, a row of one narrow widget is not
    ImGui::SetNextWindowSizeConstraints(ImVec2(190, 0), ImVec2(FLT_MAX, FLT_MAX));
    ImGui::Begin("slide parameters", nullptr, ImGuiWindowFlags_AlwaysAutoResize
                                              | ImGuiWindowFlags_NoFocusOnAppearing);
    for (auto& [name, e] : rows) {
        auto slash = name.find('/');
        std::string label = slash == std::string::npos ? name : name.substr(slash + 1);
        ImGui::PushID(name.c_str());
        if (e->drawUI(label.c_str())) {
            e->onWritten();
            dirty.insert(name);
            edited.insert(name);
        }
        ImGui::PopID();
    }
    // keep the camera still while tweaking, as the panel does
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)
        || ImGui::IsAnyItemActive())
        ImGui::SetNextFrameWantCaptureMouse(true);
    ImGui::End();
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
            vecGizmoButton(name, v->value);
        } else if (auto v2 = std::dynamic_pointer_cast<Vec2Entry>(e)) {
            if (handles_on)
                vec2_handles.insert(name);
            vec2HandleButton(name);
        }
        if (changed) {
            e->onWritten(); // the manipulator follows the widget
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

    // keep the camera still while tweaking, slope forces the capture off each
    // frame so that clicks reach the slides
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

bool Params::cursorOnGizmo()
{
    return cursor_on_gizmo;
}

bool Params::hasLiveGizmo()
{
    return !vec_gizmos.empty() || !vec2_handles.empty();
}

void Params::clearGizmos()
{
    // the panel only governs what it turned on, a visible parameter keeps its
    // manipulator wherever the panel is
    for (auto it = vec_gizmos.begin(); it != vec_gizmos.end();)
        it = auto_manipulators.count(it->first) ? std::next(it)
                                                : vec_gizmos.erase(it);
    for (auto it = vec2_handles.begin(); it != vec2_handles.end();)
        it = auto_manipulators.count(*it) ? std::next(it) : vec2_handles.erase(it);
    if (!vec2_handles.count(dragged_handle))
        dragged_handle.clear();
}

void Params::saveAllDirty()
{
    if (dirty.empty())
        return;
    if (file_broken) {
        spdlog::error("{} is unreadable, parameters not saved", file().string());
        return;
    }
    // only edited parameters are written, the others follow their code defaults
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
        json read;
        f >> read;
        file_values = std::move(read);
        file_broken = false;
        ReloadErrors::clear(file(), "params");
        for (auto& [name, e] : registry)
            if (file_values.contains(name)) {
                e->fromJson(file_values[name]);
                edited.insert(name);
                dirty.erase(name);
            }
        spdlog::info("parameters reloaded from {}", file().string());
    } catch (const std::exception& e) {
        spdlog::warn("params file unavailable or invalid : {}", e.what());
        ReloadErrors::report(file(), "params", e.what());
    }
}

}
