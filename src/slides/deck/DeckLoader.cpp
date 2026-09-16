#include "content/config/ReloadErrors.h"
#include "content/authoring/Snippet.h"
#include "slides/deck/DeckLoader.h"
#include "content/screen_primitives/text/Code.h"
#include "content/screen_primitives/text/Algorithm.h"
#include "slides/deck/items/DeckItem.h"
#include "slides/deck/items/ShaderItem.h"
#include "slides/deck/items/JsonRead.h"
#include "slides/core/Slideshow.h"
#include "content/screen_primitives/text/LateX.h"
#include "content/screen_primitives/shapes/Shape2D.h"
#include "content/screen_primitives/shapes/Stack2D.h"
#include "content/screen_primitives/gpu/Shader.h"
#include "content/authoring/Params.h"
#include "content/polyscope_primitives/PolyscopePrimitive.h"
#include "content/polyscope_primitives/CameraView.h"
#include "spdlog/spdlog.h"
#include "yaml-cpp/yaml.h"
#include <spdlog/spdlog.h>

namespace slope {

// the manifest is authored in YAML (lighter to edit, no backslash escaping)
// but converted to json internally
static json yamlToJson(const YAML::Node& node)
{
    switch (node.Type()) {
    case YAML::NodeType::Null:
        return nullptr;
    case YAML::NodeType::Scalar: {
        bool b; long long i; double d;
        if (YAML::convert<bool>::decode(node, b) && node.Tag() != "!")
            return b;
        if (YAML::convert<long long>::decode(node, i) && node.Tag() != "!")
            return i;
        if (YAML::convert<double>::decode(node, d) && node.Tag() != "!")
            return d;
        return node.as<std::string>();
    }
    case YAML::NodeType::Sequence: {
        json arr = json::array();
        for (const auto& child : node)
            arr.push_back(yamlToJson(child));
        return arr;
    }
    case YAML::NodeType::Map: {
        json obj = json::object();
        for (const auto& kv : node)
            obj[kv.first.as<std::string>()] = yamlToJson(kv.second);
        return obj;
    }
    default:
        return nullptr;
    }
}

DeckLoader::DeckLoader() {}
DeckLoader::~DeckLoader() {}

void DeckLoader::init(path deck_file)
{
    source_path = formatPath(deck_file);
    FileEditor::registerExtra(source_path);
    Latex::default_origin = source_path;
    parse();
    loadLatexResources();
    source_last_modified = std::filesystem::last_write_time(source_path);
    latex_generation = LatexLoader::generation;
    initialized = true;
}

// latex resources are chosen by the top-level "commands" (tex prefix) and
// "latex" (definitions file) keys, defaulting to the project conventions
// commands.tex / latex.json when present
void DeckLoader::loadLatexResources()
{
    auto pick = [&](const char* key, const char* fallback) -> std::string {
        if (source.is_object() && source.contains(key)) {
            std::string f = source[key];
            if (!io::file_exists(formatPath(f)))
                throw std::runtime_error("deck file references missing \""
                                         + std::string(key) + "\" file " + f);
            return f;
        }
        return io::file_exists(formatPath(fallback)) ? fallback : "";
    };
    if (auto f = pick("commands", "commands.tex"); f != "")
        Latex::AddFileToPrefix(f);
    if (auto f = pick("latex", "latex.json"); f != "")
        LatexLoader::Init(f);

    // "snippets:" is one file or a list of them. Loading is idempotent, so a
    // deck rebuild does not stack duplicates.
    if (source.is_object() && source.contains("snippets")) {
        const json& sn = source["snippets"];
        if (sn.is_string())
            Snippet::load(sn.get<std::string>());
        else if (sn.is_array())
            for (const auto& f : sn)
                Snippet::load(f.get<std::string>());
        else
            throw std::runtime_error("\"snippets\" must be a file name or a list of them");
    }
}

void DeckLoader::init(const std::string& project_name, path deck_file, int argc, char** argv)
{
    owned_show = std::make_unique<Slideshow>();
    owned_show->init(project_name, argc, argv);
    if (owned_show->helpWanted())
        return;
    init(deck_file);
}

Slideshow& DeckLoader::slideshow()
{
    if (!owned_show)
        throw std::runtime_error("DeckLoader does not own a slideshow "
                                 "(use init(project, deck, argc, argv))");
    return *owned_show;
}

void DeckLoader::run()
{
    auto& show = slideshow();
    if (!show.helpWanted()) {
        build(show);
        show.onFrame = [this, &show] { hotReload(show); };
    }
    show.run();
}

void DeckLoader::registerObject(const std::string& name, const ObjectFactory& factory)
{
    object_registry[name] = factory;
}

void DeckLoader::registerPlacer(const std::string& name, const std::function<vec2()>& placer)
{
    placer_registry[name] = placer;
}

void DeckLoader::registerObject(const std::string& name, const PrimitiveInSlide& pis)
{
    instantiated_objects[name] = pis;
}

void DeckLoader::registerObject(const std::string& name, const PrimitiveFactory& factory)
{
    object_registry[name] = [factory]() -> PrimitiveInSlide {
        return {factory(), StateInSlide()};
    };
}

void DeckLoader::registerObject(const std::string& name, PrimitivePtr ptr)
{
    instantiated_objects[name] = {ptr, StateInSlide()};
}

void DeckLoader::registerObject(const std::string& name, const GroupFactory& factory)
{
    group_registry[name] = factory;
}

void DeckLoader::registerObject(const std::string& name, const PrimitiveGroup& group)
{
    instantiated_groups[name] = group;
}

void DeckLoader::parse()
{
    if (!io::file_exists(source_path))
        throw std::runtime_error("did not find deck file " + source_path.string());
    try {
        source = yamlToJson(YAML::LoadFile(source_path.string()));
    } catch (const YAML::Exception& e) {
        throw std::runtime_error("invalid yaml in deck file " + source_path.string()
                                 + " : " + e.what());
    }
}

bool DeckLoader::sourceModified()
{
    // checked often, the deck being what the author saves to see a change
    static auto last_refresh = Time::now();
    if (TimeFrom(last_refresh) < 0.05)
        return false;
    last_refresh = Time::now();
    try {
        auto last_write = std::filesystem::last_write_time(source_path);
        if (source_last_modified < last_write) {
            source_last_modified = last_write;
            parse();
            return true;
        }
    } catch (std::exception& e) {
        spdlog::warn("deck file unavailable or invalid: {}", e.what());
        ReloadErrors::report(source_path, "deck", e.what());
    }
    return false;
}

bool DeckLoader::camerasModified()
{
    static auto last_refresh = Time::now();
    if (TimeFrom(last_refresh) < 0.2)
        return false;
    last_refresh = Time::now();

    std::vector<std::string> changed;
    for (const auto& [key, entry] : camera_cache) {
        try {
            if (entry.last_modified < std::filesystem::last_write_time(entry.file))
                changed.push_back(key);
        } catch (const std::exception&) {}
    }
    // dropped entries are recreated (re-reading the file) at next build
    for (const auto& key : changed)
        camera_cache.erase(key);
    return !changed.empty();
}

void DeckLoader::hotReload(Slideshow& show)
{
    if (!initialized)
        return;
    bool deck_changed = sourceModified();
    bool cams_changed = camerasModified();
    // a "load:" of a key the json did not have fails the build, so a fixed
    // json has to rebuild the deck and not only refresh the latex content
    bool latex_changed = LatexLoader::generation != latex_generation;
    latex_generation = LatexLoader::generation;
    if (!deck_changed && !cams_changed && !latex_changed)
        return;
    // saving the deck is what asks for a name, and the rebuild below can take
    // a latex compile, so the paste is ready before any of that starts
    if (Options::AutoSuggest && deck_changed)
        show.copyLabelSuggestion(true);

    spdlog::info("{} changed, rebuilding slides...",
                 deck_changed ? "deck file" : (cams_changed ? "camera view" : "latex source"));
    // a fixed formula's old, broken Latex primitive lingers forever in Primitive::primitives
    const std::set<PrimitivePtr> previously_used = used_primitives;
    const bool ok = show.recompose(
        [this](SlideManager& sm) {
            try {
                build(sm);
            } catch (const std::exception& e) {
                ReloadErrors::report(source_path, "deck", e.what());
                throw;
            }
        },
        used_primitives,
        [this](SlideManager& sm) {
            if (last_good_source.is_null())
                return;
            spdlog::warn("deck: keeping the last version that built");
            source = last_good_source;
            build(sm);
        });
    if (ok) {
        ReloadErrors::clear(source_path, "deck");
        bool dropped_latex_error = false;
        for (const auto& p : previously_used) {
            if (used_primitives.count(p))
                continue;
            if (auto l = std::dynamic_pointer_cast<Latex>(p); l && !l->compile_error.empty()) {
                l->compile_error.clear();
                dropped_latex_error = true;
            }
        }
        if (dropped_latex_error)
            Latex::PublishErrors();
    }

    LabelAnchor::takeFreshLabels();
}

PrimitivePtr DeckLoader::cached(const std::string& key, const std::function<PrimitivePtr()>& create)
{
    auto it = primitive_cache.find(key);
    if (it != primitive_cache.end())
        return it->second;
    auto ptr = create();
    primitive_cache[key] = ptr;
    return ptr;
}

PrimitivePtr DeckLoader::resolve(const std::string& name) const
{
    auto it = named.find(name);
    if (it == named.end())
        throw std::runtime_error("deck references unknown item \"" + name
                                 + "\" (declared later, or missing an id?)");
    return it->second;
}

// A 3 component value is a world position in the scene. A 2 component one is a
// screen position, unless an "<item>." prefix names a shader, and then it is a
// point of that shader's world space. Nothing is ever inferred.
std::function<vec2()> DeckLoader::resolveFollow(const std::string& spec)
{
    // a registered placer is already a screen position
    auto reg = placer_registry.find(spec);
    if (reg != placer_registry.end())
        return reg->second;

    // "<item>.<name>" only when the prefix really names an item, so a
    // parameter with a dot in its name is still read whole
    std::string id, var = spec;
    if (auto dot = spec.rfind('.'); dot != std::string::npos
        && named.count(spec.substr(0, dot))) {
        id  = spec.substr(0, dot);
        var = spec.substr(dot + 1);
    }

    ShaderPtr sh;
    if (!id.empty()) {
        sh = std::dynamic_pointer_cast<Shader>(resolve(id));
        if (!sh)
            throw std::runtime_error("\"follow: " + spec + "\" : item \"" + id + "\" is not a "
                                     "shader, so it has no world space to read \"" + var
                                     + "\" in");
        if (!sh->hasView())
            throw std::runtime_error("\"follow: " + spec + "\" : shader \"" + id
                                     + "\" has no \"view:\", so it has no world points");
    }

    // A parameter has a known width, so everything wrong can be said right now.
    // A snippet variable only reveals its width when it is read.
    if (int n = Params::components(var); n > 0) {
        if (n != 2 && n != 3)
            throw std::runtime_error("\"follow: " + spec + "\" : parameter \"" + var + "\" has "
                                     + std::to_string(n) + " components, and a point to follow "
                                     "needs 2 (screen, or a shader's world space) or 3 (the "
                                     "3D scene)");
        if (n == 3) {
            if (sh)
                throw std::runtime_error("\"follow: " + spec + "\" : \"" + var + "\" is a 3D "
                                         "point, always in the scene's world space; drop the \""
                                         + id + ".\"");
            return [var] {
                scalar p[4] = {0, 0, 0, 0};
                Params::read(var, p);
                return WorldToScreen(vec(p[0], p[1], p[2]));
            };
        }
        std::function<vec2()> p2 = [var] {
            scalar p[4] = {0, 0, 0, 0};
            Params::read(var, p);
            return vec2(p[0], p[1]);
        };
        return sh ? sh->tracker(p2) : p2;
    }

    auto said = std::make_shared<bool>(false);
    return [var, spec, sh, said]() -> vec2 {
        // before the first frame nothing has been evaluated, so say nothing
        if (!Snippet::ready())
            return vec2(0.5, 0.5);
        auto v = Snippet::get(var);
        if (v.n == 3) {
            if (sh && !*said) {
                *said = true;
                spdlog::error("\"follow: {}\" : \"{}\" is a 3D point, always in the scene's "
                              "world space, not a shader's", spec, var);
            }
            return WorldToScreen(v.v3());
        }
        if (v.n == 2)
            return sh ? sh->worldToScreen(v.v2()) : v.v2();
        if (!*said) {
            *said = true;
            spdlog::error("\"follow: {}\" : no snippet variable, parameter or registered "
                          "placer called \"{}\"", spec, var);
        }
        return vec2(0.5, 0.5);
    };
}

ScreenPrimitivePtr DeckLoader::resolveScreen(const std::string& name) const
{
    auto sp = std::dynamic_pointer_cast<ScreenPrimitive>(resolve(name));
    if (!sp)
        throw std::runtime_error("deck item \"" + name + "\" is not a screen primitive");
    return sp;
}

void DeckLoader::applyDeckConfig()
{
    // an absent block, or a dropped key, resets that knob to its compiled default
    const json empty = json::object();
    const json& cfg = (source.is_object() && source.contains("config"))
                     ? source["config"] : empty;
    if (!cfg.is_object())
        throw std::runtime_error("\"config\" must be a map of settings");

    Options::TitleScale         = cfg.value("title_scale",  Options::DefaultTitleScale);
    Options::DefaultLatexScale  = cfg.value("latex_scale",  Options::DefaultLatexScaleValue);
    Options::DefaultBoxRoundness = cfg.value("box_roundness",
                                             (double)Options::DefaultBoxRoundnessValue);

    // "margin" is one number (both axes) or [x, y]
    const scalar dm = Options::DefaultScreenMargin;
    if (!cfg.contains("margin"))
        Options::ScreenMargin = vec2(dm, dm);
    else if (cfg["margin"].is_number()) {
        scalar m = cfg["margin"].get<scalar>();
        Options::ScreenMargin = vec2(m, m);
    }
    else
        Options::ScreenMargin = readVec2(cfg["margin"], "margin");

    auto point = [&](const char* key, const vec2& def) {
        return cfg.contains(key) ? readVec2(cfg[key], key) : def;
    };
    TOP    = point("top",    placement_default::TOP);
    CENTER = point("center", placement_default::CENTER);
    BOTTOM = point("bottom", placement_default::BOTTOM);

    // top-level "preamble:", a string or list of inline latex prefix lines, on top of commands.tex
    TexObject preamble;
    if (source.is_object() && source.contains("preamble")) {
        const json& p = source["preamble"];
        if (p.is_string())
            preamble = p.get<std::string>();
        else if (p.is_array()) {
            for (const auto& line : p) {
                if (!line.is_string())
                    throw std::runtime_error("\"preamble\" list entries must be strings");
                preamble += line.get<std::string>() + "\n";
            }
        }
        else
            throw std::runtime_error("\"preamble\" must be a string or a list of strings");
    }
    Latex::SetDeckPrefix(preamble);
}

void DeckLoader::build(SlideManager& show)
{
    if (!source.contains("slides") || !source["slides"].is_array())
        throw std::runtime_error("deck file must contain a top-level \"slides\" array");
    auto reserved = [](const std::string& key) {
        for (const auto& [k, doc] : deckTopLevelKeys())
            if (k == key) return true;
        return false;
    };
    applyDeckConfig();
    deck_groups.clear();
    expanding.clear();
    collectors.clear();
    for (const auto& [key, val] : source.items())
        if (!reserved(key))
            declareGroup(key, val);
    // an arg named like a group would read as a second call
    for (const auto& [name, g] : deck_groups)
        for (const auto& [param, def] : g.params.items())
            if (deck_groups.count(param))
                throw std::runtime_error("group \"" + name + "\" has a param named like the "
                                         "group \"" + param + "\", rename one");

    used_primitives.clear();
    named.clear();
    show.clearGroups();
    show.clearKeyframes();
    Code::ClearAllCues();
    Algorithm::ClearAllCues();

    // drop what an "object:" item no longer declares, and only that, the rest
    // of that shader's binds belong to its C++ owner
    for (auto& [object, declared] : object_uniforms)
        for (const auto& name : declared.second)
            declared.first->unset(name);
    object_uniforms.clear();

    const json* tmpl = nullptr;
    if (source.contains("template")) {
        if (!source["template"].is_array())
            throw std::runtime_error("\"template\" must be a list of items, like a frame");
        for (const auto& it : source["template"])
            if (it.is_string() && it == "step")
                throw std::runtime_error("a template cannot contain \"step\", it is "
                                         "added to the first step of every frame");
        tmpl = &source["template"];
    }
    // built once, then the same primitives are re-added : a template rebuilt
    // per frame would make new ones, and every slide change would cross-fade
    std::vector<PrimitiveInSlide> template_items;
    bool template_built = false;

    bool first = true;
    for (const auto& frame : source["slides"]) {
        const json* items = nullptr;
        bool same_title = false;
        bool no_template = false;
        if (frame.is_array())
            items = &frame;
        else if (frame.is_object() && frame.contains("frame") && frame["frame"].is_array()) {
            items = &frame["frame"];
            same_title = frame.value("same_title", false);
            no_template = frame.value("no_template", false);
        }
        else
            throw std::runtime_error("each element of \"slides\" must be \"- frame:\" "
                                     "followed by a list of items");
        if (!first)
            show << (same_title ? newFrameSameTitle : newFrame);
        first = false;
        step_primitives.clear();
        if (tmpl && !no_template) {
            if (!template_built) {
                show.getLastSlide();  // the first frame has no slide until an item makes one
                const int slides_before = show.getNumberSlides();
                buildFrame(show, *tmpl);
                if (show.getNumberSlides() != slides_before)
                    throw std::runtime_error("the template uses a group with \"step\", it is "
                                             "added to the first step of every frame");
                template_items = show.getLastSlide().getDepthSorted();
                template_built = true;
            } else {
                for (const auto& [ptr, sis] : template_items)
                    show.addToLastSlide(ptr, sis);
            }
        }
        buildFrame(show, *items);
    }
    if (show.getNumberSlides() == 0)
        show.addSlide(Slide());
    // an id is a name too, and a suggestion clashing with one is unusable
    std::set<std::string> taken;
    for (const auto& [n, prim] : named)
        taken.insert(n);
    LabelAnchor::reserveNames(std::move(taken));
    if (!first_build_done) {
        LabelAnchor::takeFreshLabels();
        first_build_done = true;
    }
    last_good_source = source;
}

static bool isParamName(const std::string& s);

// Any other top level key is a group : a list of items, or a map with "items"
// and optionally "params".
void DeckLoader::declareGroup(const std::string& name, const json& val)
{
    DeckGroup g;
    if (val.is_array())
        g.items = val;
    else if (val.is_object() && val.contains("items") && val["items"].is_array()) {
        g.items = val["items"];
        for (const auto& [key, v] : val.items())
            if (key != "items" && key != "params")
                spdlog::warn("deck: ignored key \"{}\" on group \"{}\"", key, name);
        if (val.contains("params")) {
            if (!val["params"].is_object())
                throw std::runtime_error("\"params\" of group \"" + name + "\" must be a map "
                                         "of name: default");
            g.params = val["params"];
        }
    }
    else {
        spdlog::warn("deck: ignored top-level key \"{}\", a group is a list of items or a map "
                     "with \"items:\"", name);
        return;
    }
    // "- name: value" is told apart from an item by its type key, so neither the
    // group nor an arg may carry one
    auto isType = [](const std::string& key) {
        if (key == "step" || key == "background")
            return true;
        for (const auto& spec : itemSpecs())
            if (spec.type == key)
                return true;
        return false;
    };
    if (isType(name))
        throw std::runtime_error("group \"" + name + "\" has the name of an item type, rename it");
    for (const auto& [param, def] : g.params.items()) {
        if (!isParamName(param))
            throw std::runtime_error("param \"" + param + "\" of group \"" + name + "\" must be "
                                     "letters, digits and _, not starting with a digit");
        if (isType(param))
            throw std::runtime_error("param \"" + param + "\" of group \"" + name + "\" has the "
                                     "name of an item type, the call would read as that item");
    }
    for (const auto& reserved : {"id", "group"})
        if (g.params.contains(reserved))
            throw std::runtime_error("group \"" + name + "\" cannot take a param named \""
                                     + reserved + "\"");
    deck_groups[name] = std::move(g);
}

const std::string* DeckLoader::groupCallOf(const json& item) const
{
    // an item type wins, so a group may share its name with an item's other keys
    if (findItemSpec(item) || item.contains("background"))
        return nullptr;
    const std::string* found = nullptr;
    for (const auto& [key, val] : item.items()) {
        auto it = deck_groups.find(key);
        if (it == deck_groups.end())
            continue;
        if (found)
            throw std::runtime_error("an item calls two groups, \"" + *found + "\" and \""
                                     + key + "\"");
        found = &it->first;
    }
    return found;
}

static bool isParamName(const std::string& s)
{
    if (s.empty() || !(std::isalpha((unsigned char)s[0]) || s[0] == '_'))
        return false;
    for (char c : s)
        if (!(std::isalnum((unsigned char)c) || c == '_'))
            return false;
    return true;
}

// "$name" as a whole value keeps the arg's type, "${name}" goes inside a string.
// Names args does not hold are left as written, which is what keeps latex intact.
static json substituteArgs(const json& v, const json& args, const std::string& group)
{
    if (v.is_object()) {
        json out = json::object();
        for (const auto& [key, x] : v.items())
            out[key] = substituteArgs(x, args, group);
        return out;
    }
    if (v.is_array()) {
        json out = json::array();
        for (const auto& x : v)
            out.push_back(substituteArgs(x, args, group));
        return out;
    }
    if (!v.is_string())
        return v;
    const std::string& s = v.get_ref<const std::string&>();
    if (s.size() > 1 && s[0] == '$' && isParamName(s.substr(1))) {
        if (args.contains(s.substr(1)))
            return args[s.substr(1)];
        // a whole "$word" is no latex, so it is a misspelt param
        spdlog::warn("deck: \"{}\" in group \"{}\" names no param", s, group);
    }

    std::string out;
    size_t pos = 0;
    for (size_t open; (open = s.find("${", pos)) != std::string::npos;) {
        size_t close = s.find('}', open);
        if (close == std::string::npos)
            break;
        std::string name = s.substr(open + 2, close - open - 2);
        if (!isParamName(name) || !args.contains(name)) {
            out += s.substr(pos, open + 2 - pos);
            pos = open + 2;
            continue;
        }
        const json& a = args[name];
        out += s.substr(pos, open - pos);
        if (a.is_string())
            out += a.get<std::string>();
        else if (a.is_number() || a.is_boolean())
            out += a.dump();
        else
            throw std::runtime_error("group \"" + group + "\" puts \"" + name + "\" inside a "
                                     "string, which takes a text or a number, not " + a.dump());
        pos = close + 1;
    }
    out += s.substr(pos);
    return out;
}

static bool isIdName(const std::string& s)
{
    if (s.empty())
        return false;
    for (char c : s)
        if (!(std::isalnum((unsigned char)c) || c == '_' || c == '-'))
            return false;
    return true;
}

void DeckLoader::markUsed(const PrimitivePtr& ptr)
{
    used_primitives.insert(ptr);
    for (auto* c : collectors)
        c->insert(ptr);
}

std::set<PrimitivePtr> DeckLoader::collect(const std::function<void()>& run)
{
    std::set<PrimitivePtr> placed;
    collectors.push_back(&placed);
    try {
        run();
    } catch (...) {
        collectors.pop_back();
        throw;
    }
    collectors.pop_back();
    return placed;
}

// A group is rebuilt at each use, as if its items were written there. The
// content cache hands back the same primitives, so a reuse moves them instead
// of cross-fading copies, and remove/set/keyframe inside it run every time.
std::set<PrimitivePtr> DeckLoader::expandGroup(SlideManager& show, const std::string& name,
                                               const json& call)
{
    auto it = deck_groups.find(name);
    if (it == deck_groups.end())
        throw std::runtime_error("deck references unknown group \"" + name + "\", declare it "
                                 "as a top level list beside \"slides\"");
    const DeckGroup& g = it->second;
    if (std::find(expanding.begin(), expanding.end(), name) != expanding.end())
        throw std::runtime_error("group \"" + name + "\" uses itself");

    json args = g.params;
    std::string id;
    if (call.is_object()) {
        const json& value = call[name];
        if (value.is_string())
            id = value.get<std::string>();
        else if (!value.is_null())
            throw std::runtime_error("\"" + name + ": " + value.dump() + "\" : the value "
                                     "after a group name is its id, a name");
        for (const auto& [key, val] : call.items()) {
            if (key == name || key == "group")
                continue;
            if (key == "id")
                throw std::runtime_error("a group call takes its id after the name, write \"- "
                                         + name + ": some_id\" instead of \"id:\"");
            else if (!g.params.contains(key))
                spdlog::warn("deck: ignored key \"{}\" on group \"{}\"", key, name);
            else if (!val.is_null())  // "key:" left empty keeps the default
                args[key] = val;
        }
    }
    for (const auto& [key, val] : args.items())
        if (val.is_null())
            throw std::runtime_error("group \"" + name + "\" needs \"" + key + ":\"");
    if (!id.empty()) {
        // it ends up in ids and label files through ${id}
        if (!isIdName(id))
            throw std::runtime_error("the id \"" + id + "\" of group \"" + name + "\" may only "
                                     "hold letters, digits, _ and -");
        args["id"] = id;
    }
    else if (const std::string body = g.items.dump();
             body.find("\"$id\"") != std::string::npos || body.find("${id}") != std::string::npos)
        throw std::runtime_error("group \"" + name + "\" uses $id, give it one with \"- "
                                 + name + ": some_id\"");

    std::set<PrimitivePtr> placed;
    expanding.push_back(name);
    try {
        placed = collect([&] { buildFrame(show, substituteArgs(g.items, args, name)); });
    } catch (const std::exception& e) {
        expanding.pop_back();
        throw std::runtime_error(std::string(e.what()) + " (in group \"" + name + "\")");
    }
    expanding.pop_back();
    // tags, so "remove:" takes off every use of the group, or this one by its id
    for (const auto& ptr : placed) {
        show.addToGroup(name, ptr);
        if (!id.empty())
            show.addToGroup(id, ptr);
    }
    return placed;
}

void DeckLoader::buildFrame(SlideManager& show, const json& items)
{
    for (const auto& item : items) {
        if (item.is_string() && item == "step") {
            show << inNextFrame;
            // the next step inherits these items, re-placing one there moves it
            step_primitives.clear();
            continue;
        }
        if (item.is_string()) {
            expandGroup(show, item.get<std::string>(), json());
            continue;
        }
        if (!item.is_object())
            throw std::runtime_error("deck items must be yaml maps, the bare \"- step\" marker, "
                                     "or the bare name of a top level group");
        if (item.contains("step"))
            throw std::runtime_error("\"step:\" subtrees were replaced by the flat "
                                     "\"- step\" marker : items after it belong to the next step");
        if (const std::string* called = groupCallOf(item)) {
            auto placed = expandGroup(show, *called, item);
            if (item.contains("group"))
                for (const auto& ptr : placed)
                    show.addToGroup(item["group"].get<std::string>(), ptr);
            continue;
        }
        if (item.contains("group")) {
            // every primitive the item adds, box subtree included, joins the group
            for (const auto& p : collect([&] { addItem(show, item); }))
                show.addToGroup(item["group"].get<std::string>(), p);
        } else {
            addItem(show, item);
        }
    }
}

// "uniforms:" and "textures:" on an "object:" item. A shader registered from
// C++ takes the same declarative inputs as a "shader:" item, with parameters
// named after the object rather than after the item's id.
//
// Its C++ owner binds uniforms of its own, so a reload drops only what the deck
// declared last time (in build()). retainTextures() is already that careful.
void DeckLoader::declareObjectShaderInputs(const ShaderPtr& shader, const std::string& object,
                                           const json& item)
{
    if (!item.contains("uniforms") && !item.contains("textures"))
        return;
    // one shader however many slides show it, so two declarations would fight
    if (object_uniforms.count(object)) {
        spdlog::warn("deck: object \"{}\" declares \"uniforms\" or \"textures\" on more than "
                     "one item ; only the first declaration is used", object);
        return;
    }
    object_uniforms[object] = {shader, declareShaderUniforms(shader, item, object, false)};
    if (item.contains("textures"))
        declareShaderTextures(shader, item);
}

std::pair<ScreenPrimitivePtr,std::string> DeckLoader::makeScreenPrimitive(const json& item)
{
    const ItemSpec* spec = findItemSpec(item);
    if (!spec || spec->kind != ItemSpec::Kind::Screen)
        throw std::runtime_error("expected a screen item (" + screenItemTypes()
                                 + "), got: " + item.dump());

    // the id is part of the cache key, so two items with the same content
    // but different ids are distinct primitives (shown simultaneously)
    PrimitivePtr prim = cached("id=" + item.value("id", std::string()) + ":" + spec->key(item),
                               [&] { return spec->make(item); });
    std::string name = item.value("id", spec->name(item));
    if (spec->configure)
        spec->configure(prim, item, name);

    auto sp = std::static_pointer_cast<ScreenPrimitive>(prim);
    if (name != "")
        named[name] = sp;
    return {sp, name};
}

// children of a "stack" are laid out by the stack (below one another),
// so they are screen items without placement; "- step" works as usual, and
// an item with an explicit "at" escapes the layout
void DeckLoader::buildStackChildren(SlideManager& show, const Stack2DPtr& stack,
                                    const json& items)
{
    for (const auto& item : items) {
        if (item.is_string() && item == "step") {
            show << inNextFrame;
            step_primitives.clear();
            continue;
        }
        if (item.is_string() || (item.is_object() && groupCallOf(item)))
            throw std::runtime_error("a stack cannot call a group, write its items in the stack");
        if (!item.is_object())
            throw std::runtime_error("stack items must be yaml maps (or the bare \"- step\" marker)");
        if (item.contains("step"))
            throw std::runtime_error("\"step:\" subtrees were replaced by the flat "
                                     "\"- step\" marker : items after it belong to the next step");
        if (item.contains("at")) { // explicit placement escapes the layout
            addItem(show, item);
            continue;
        }
        warnUnknownKeys(item);
        auto [prim, name] = makeScreenPrimitive(item);
        stack->addChild(prim);
        show.addToLastSlide(stack->place(prim, item.value("alpha", 1.)));
        markUsed(prim);
        if (item.contains("group"))
            show.addToGroup(item["group"].get<std::string>(), prim);
    }
}

// handle anchor of a stack item. "at" as [x,y] is a fixed handle, as a string
// a drag-editable label, falling back to the id
AnchorPtr DeckLoader::makeHandleAnchor(const json& item)
{
    if (item.contains("at") && item["at"].is_array())
        return AbsoluteAnchor::Add(readVec2(item["at"], "at"));
    if (item.contains("at"))
        return LabelAnchor::Add(item["at"].get<std::string>());
    return LabelAnchor::Add(item.value("id", "stack"));
}

// the fields carried by the slide state rather than by the primitive, so a
// later "set" of any of them is animated by the transition
static void applyStateOptions(StateInSlide& sis, const json& item)
{
    if (item.contains("alpha"))
        sis.alpha = item["alpha"].get<scalar>();
    if (item.contains("rot"))
        sis.angle = item["rot"].get<scalar>() * M_PI / 180.;
    if (item.contains("zoom"))
        sis.scale = item["zoom"].get<scalar>();
    if (item.contains("two_sided"))
        sis.plane.double_sided = item["two_sided"].get<bool>();
}

// "on: <id>" names a plane the T gizmo owns, a map is one the deck owns.
// In the map form each vector is [x,y,z] or a snippet name, so a plane can move
static ScreenPrimitiveInSlide placeOnPlane(ScreenPrimitivePtr prim, const json& on, scalar alpha)
{
    if (on.is_boolean())
        throw std::runtime_error("\"on:\" read as a boolean. Quote plane ids like \"on\", "
                                 "\"off\", \"yes\" or \"no\"");
    if (on.is_string())
        return prim->onPlane(on.get<std::string>(), alpha);
    if (!on.is_object())
        throw std::runtime_error("\"on:\" must be a plane id, or {origin, u, normal}");
    for (const char* k : {"origin", "u", "normal"})
        if (!on.contains(k))
            throw std::runtime_error(std::string("\"on:\" as a map needs \"") + k + ":\"");

    LivePlane l;
    l.origin = readLiveVec(on["origin"], "on.origin");
    l.u      = readLiveVec(on["u"], "on.u");
    l.normal = readLiveVec(on["normal"], "on.normal");
    // three constants make a plane that never moves, so resolve it once
    if (!l.origin.live() && !l.u.live() && !l.normal.live())
        return prim->onPlane(l.origin.fixed, l.u.fixed, l.normal.fixed, alpha);
    return prim->onPlane(l, alpha);
}

// applies the placement fields of a screen item, at (label, [x,y] or a named
// position), on (a world plane) or below/above/right_of/left_of
// "reveal" and "focus" are slide state, so they are streamed for the frame
// being composed rather than set on the primitive
template <class Listing>
static void streamCues(SlideManager& show, const std::shared_ptr<Listing>& code,
                       const json& item)
{
    if (item.contains("reveal")) {
        const json& r = item["reveal"];
        if (r.is_number_integer())
            show << code->reveal(r.get<int>());
        else if (r.is_string()) {
            const std::string v = r.get<std::string>();
            if (v == "START")      show << code->reveal(START);
            else if (v == "END")   show << code->reveal(END);
            else                   show << code->reveal(v);
        }
        else
            throw std::runtime_error("\"reveal\" takes START, END, a label or a line");
    }
    if (item.contains("focus")) {
        const json& f = item["focus"];
        if (f.is_string())
            show << code->focus(f.get<std::string>());
        else if (f.is_array() && f.size() == 2 && f[0].is_number_integer())
            show << code->focus(f[0].get<int>(), f[1].get<int>());
        else if (f.is_array() && f.size() == 2 && f[0].is_string())
            show << code->focus(f[0].get<std::string>(), f[1].get<std::string>());
        else if (f.is_null())
            show << code->unfocus();
        else
            throw std::runtime_error("\"focus\" takes a region, [label, label] or "
                                     "[first, last]");
    }
}

static void applyCodeCues(SlideManager& show, const ScreenPrimitivePtr& prim,
                          const json& item)
{
    if (!item.contains("reveal") && !item.contains("focus"))
        return;
    if (auto code = std::dynamic_pointer_cast<Code>(prim))
        return streamCues(show, code, item);
    if (auto algo = std::dynamic_pointer_cast<Algorithm>(prim))
        return streamCues(show, algo, item);
    throw std::runtime_error("\"reveal\" and \"focus\" belong to a \"code\" or \"algo\" item");
}

void DeckLoader::placeScreenItem(SlideManager& show, ScreenPrimitivePtr prim,
                                 const json& item, const std::string& default_label,
                                 bool keep_placement)
{
    scalar alpha = item.value("alpha", 1.);
    // recorded against the slide being composed, so it happens before any of
    // the placement branches return
    applyCodeCues(show, prim, item);

    // Two items of the same content are one cached primitive, and a slide holds
    // each once, so the second placement would move the first rather than show a
    // copy. A "set", or an item repeated after a "- step", re-places on purpose.
    if (!keep_placement && !step_primitives.insert(prim).second)
        spdlog::warn("deck: \"{}\" is placed twice on the same step. Both are the same "
                     "primitive, so the second placement moves the first rather than adding "
                     "a copy. Give them different \"id:\" to show both",
                     default_label.empty() ? item.dump() : default_label);

    if (item.contains("on"))
        for (const char* k : {"at", "follow", "below", "above", "right_of", "left_of"})
            if (item.contains(k))
                throw std::runtime_error(std::string("\"on:\" pastes an item onto a world "
                    "plane, which leaves no screen position to set with \"") + k + ":\"");

    struct { const char* key; placeX X; placeY Y; } relatives[] = {
        {"below",    placeX::SAME_X,    placeY::REL_BOTTOM},
        {"above",    placeX::SAME_X,    placeY::REL_TOP},
        {"right_of", placeX::REL_RIGHT, placeY::SAME_Y},
        {"left_of",  placeX::REL_LEFT,  placeY::SAME_Y},
    };
    for (const auto& rel : relatives) {
        if (!item.contains(rel.key))
            continue;
        scalar padding = item.value("padding", 0.01);
        ScreenPrimitivePtr other = nullptr; // null is relative to last inserted
        if (item[rel.key].is_string())
            other = resolveScreen(item[rel.key]);
        show << PlaceRelative(prim, other, rel.X, rel.Y, padding, padding);
        auto& slide = show.getLastSlide();
        auto placed = slide.find(std::static_pointer_cast<Primitive>(prim));
        if (placed != slide.end())
            applyStateOptions(placed->second, item);
        markUsed(prim);
        return;
    }

    ScreenPrimitiveInSlide pis;
    if (item.contains("on"))
        pis = placeOnPlane(prim, item["on"], alpha);
    else if (item.contains("follow")) {
        if (item.contains("at"))
            throw std::runtime_error("a \"follow:\" item has no \"at:\" (it rides a moving "
                                     "point) : use \"offset: [x, y]\" to shift it from that "
                                     "point");
        pis = prim->at(resolveFollow(item["follow"].get<std::string>()));
        pis.second.alpha = alpha;
        if (item.contains("offset")) {
            const json& o = item["offset"];
            if (!o.is_array() || o.size() != 2)
                throw std::runtime_error("\"offset\" must be [x, y]");
            pis.second.setOffset(vec2(o[0].get<scalar>(), o[1].get<scalar>()));
        }
    }
    else if (item.contains("at") && item["at"].is_array())
        pis = prim->at(readVec2(item["at"], "at"), alpha);
    else if (item.contains("at")) {
        std::string at = item["at"];
        // sx, sy of -1 / 0 / +1 is flush-low / centre / flush-high on that axis
        auto edge = [&](int sx, int sy) {
            ScreenPrimitivePtr p = prim;
            pis = p->at([p, sx, sy] {
                vec2 m = Options::ScreenMargin;
                vec2 h = p->getRelativeSize() * 0.5;
                scalar x = sx < 0 ? m(0) + h(0) : (sx > 0 ? 1 - m(0) - h(0) : CENTER(0));
                scalar y = sy < 0 ? m(1) + h(1) : (sy > 0 ? 1 - m(1) - h(1) : CENTER(1));
                return vec2(x, y);
            });
            pis.second.alpha = alpha;
        };
        if (at == "TOP") pis = prim->at(TOP, alpha);
        else if (at == "CENTER") pis = prim->at(CENTER, alpha);
        else if (at == "BOTTOM") pis = prim->at(BOTTOM, alpha);
        else if (at == "TOP_LEFT")      edge(-1, -1);
        else if (at == "TOP_RIGHT")     edge(+1, -1);
        else if (at == "BOTTOM_LEFT")   edge(-1, +1);
        else if (at == "BOTTOM_RIGHT")  edge(+1, +1);
        else if (at == "LEFT")          edge(-1,  0);
        else if (at == "RIGHT")         edge(+1,  0);
        else pis = prim->at(at, alpha);
    }
    else if (default_label != "" && !prim->placesItself())
        pis = prim->at(default_label, alpha);
    else if (keep_placement) {
        // a "set" that only changes state keeps wherever the item already is
        auto& slide = show.getLastSlide();
        auto placed = slide.find(std::static_pointer_cast<Primitive>(prim));
        if (placed == slide.end())
            throw std::runtime_error("\"set\" of an item that is not on this slide");
        pis = {prim, placed->second};
    }
    else {
        // no placement given, center like `show << primitive`
        show << std::static_pointer_cast<Primitive>(prim);
        markUsed(prim);
        return;
    }
    applyStateOptions(pis.second, item);
    show.addToLastSlide(pis);
    // a "set" re-places an item it does not own, so no group or box claims it
    if (keep_placement)
        used_primitives.insert(pis.first);
    else
        markUsed(pis.first);
}

// mesh, surface and curve are built from the manifest alone, so one branch
// serves all three
static const ItemSpec* sceneSpecOf(const json& item)
{
    const ItemSpec* spec = findItemSpec(item);
    return spec && spec->kind == ItemSpec::Kind::Scene ? spec : nullptr;
}

// "at:" is the gizmo label, "transform:" what snippets drive inside its frame
static PrimitiveInSlide placeSceneItem(const PolyscopePrimitivePtr& poly, const json& item)
{
    scalar alpha = item.value("alpha", 1.);
    if (item.contains("at") && !item["at"].is_string())
        throw std::runtime_error("\"at:\" of a scene item names a transform label. A position "
                                 "goes in \"transform: {pos: [x, y, z]}\"");
    const std::string label = item.value("at", "");
    if (!item.contains("transform"))
        return label.empty() ? poly->at(alpha) : poly->at(label, alpha);
    const LiveTransform T = readTransform(item["transform"]);
    return label.empty() ? poly->at(T, alpha) : poly->at(label, T, alpha);
}

void DeckLoader::addItem(SlideManager& show, const json& item)
{
    warnUnknownKeys(item);
    if (item.contains("keyframe")) {
        show.markKeyframe(item["keyframe"].get<std::string>());
        return;
    }
    if (item.contains("remove")) {
        auto removeOne = [&](const std::string& name) {
            if (instantiated_groups.count(name)) {
                show.removeFromCurrentSlide(instantiated_groups[name]);
                return;
            }
            if (show.hasGroup(name)) {
                if (named.count(name))
                    spdlog::warn("deck: \"{}\" is both a group and an item id, \"remove\" takes "
                                 "off the group", name);
                show.removeGroup(name);
                return;
            }
            show.removeFromCurrentSlide(resolve(name));
        };
        if (item["remove"].is_array())
            for (const auto& name : item["remove"])
                removeOne(name);
        else
            removeOne(item["remove"]);
    }
    else if (item.contains("set")) {
        // re-places or restyles an already defined item, without redefining it
        auto prim = resolveScreen(item["set"]);
        placeScreenItem(show, prim, item, "", true);
    }
    else if (item.contains("replace")) {
        if (!item.contains("with"))
            throw std::runtime_error("\"replace\" item needs a \"with\" sub-item");
        std::string replaced = item["replace"];
        auto old = resolveScreen(replaced);
        auto [prim, name] = makeScreenPrimitive(item["with"]);
        show << Replace(prim, old);
        // the name now refers to the replacement, or a second "replace" would
        // resolve to the primitive just taken off the slide
        named[replaced] = prim;
        markUsed(prim);
    }
    else if (item.contains("object")) {
        std::string name = item["object"];
        if (group_registry.count(name) && !instantiated_groups.count(name))
            instantiated_groups[name] = group_registry[name]();
        if (instantiated_groups.count(name)) {
            if (item.contains("uniforms") || item.contains("textures") || item.contains("view"))
                throw std::runtime_error("\"object: " + name + "\" is a group of primitives, so "
                                         "it has no \"uniforms\", \"textures\" or \"view\" of "
                                         "its own : those belong to a single shader");
            const auto& G = instantiated_groups[name];
            show << G;
            for (const auto& [ptr, sis] : G.buffer)
                markUsed(ptr);
            return;
        }
        if (!instantiated_objects.count(name)) {
            auto it = object_registry.find(name);
            if (it == object_registry.end())
                throw std::runtime_error("deck references unregistered object \"" + name + "\"");
            instantiated_objects[name] = it->second();
        }
        auto pis = instantiated_objects[name];
        named[item.value("id", name)] = pis.first;
        // a shader registered from C++ still takes its world space, its
        // uniforms and its textures from here
        auto sh = std::dynamic_pointer_cast<Shader>(pis.first);
        if (!sh && (item.contains("uniforms") || item.contains("textures")
                    || item.contains("view")))
            throw std::runtime_error("\"object: " + name + "\" is not a shader, so it takes no "
                                     "\"uniforms\", \"textures\" or \"view\"");
        if (sh) {
            declareShaderView(sh, item);
            declareObjectShaderInputs(sh, name, item);
        }
        if (pis.first->isScreenSpace()) {
            placeScreenItem(show, std::static_pointer_cast<ScreenPrimitive>(pis.first), item, name);
            return;
        }
        if (pis.first->isPolyscopePrimitive()
            && (item.contains("at") || item.contains("transform")))
            pis = placeSceneItem(std::static_pointer_cast<PolyscopePrimitive>(pis.first), item);
        show.addToLastSlide(pis);
        markUsed(pis.first);
    }
    else if (const ItemSpec* spec = sceneSpecOf(item)) {
        auto prim = cached(spec->key(item), [&] { return spec->make(item); });
        std::string name = item.value("id", spec->name(item));
        if (spec->configure)
            spec->configure(prim, item, name);
        named[name] = prim;
        show.addToLastSlide(placeSceneItem(std::static_pointer_cast<PolyscopePrimitive>(prim), item));
        markUsed(prim);
    }
    else if (item.contains("arrow")) {
        const json& raw = item["arrow"];
        // "arrow: id" needs no from/to, registering "id/tail" and "id/tip" as params instead
        const bool shorthand = raw.is_string();
        const std::string id = shorthand ? raw.get<std::string>() : item.value("id", std::string());
        const json& spec = shorthand ? item : raw;
        if (!shorthand) {
            if (!spec.is_object() || !spec.contains("from") || !spec.contains("to"))
                throw std::runtime_error("\"arrow\" item needs {from: ..., to: ...}, or just an id");
            for (const auto& [key, val] : spec.items())
                if (!arrowFields().count(key))
                    spdlog::warn("deck: ignored key \"{}\" on an \"arrow\" item", key);
        }

        // an endpoint is [x,y] (a param when id'd), an item name (attached live), or a label
        auto endpoint = [&](const char* key, const char* suffix, const vec2& def) -> Arrow2D::Endpoint {
            const json v = spec.contains(key) ? spec[key] : json{def(0), def(1)};
            if (v.is_array()) {
                Arrow2D::Endpoint e;
                vec2 p0 = readVec2(v, "arrow endpoint");
                if (id.empty()) {
                    e.fixed = p0;
                } else {
                    // the param is y-up like a shader uv, the arrow is y-down screen space
                    auto param = Params::AddVec2(id + "/" + suffix, vec2(p0(0), 1 - p0(1)));
                    e.follow = [param] { vec2 uv = param; return vec2(uv(0), 1 - uv(1)); };
                }
                return e;
            }
            std::string s = v;
            if (named.count(s)) {
                auto sp = std::dynamic_pointer_cast<ScreenPrimitive>(named[s]);
                if (!sp)
                    throw std::runtime_error("arrow endpoint \"" + s
                                             + "\" is not a screen primitive");
                return Arrow2D::Attach(sp);
            }
            return Arrow2D::AttachLabel(s);
        };

        auto prim = std::static_pointer_cast<Arrow2D>(
            cached("arrow:" + item.dump(), [&]() -> PrimitivePtr {
                return Arrow2D::Add(endpoint("from", "tail", vec2(0.4, 0.5)),
                                    endpoint("to", "tip", vec2(0.6, 0.5)));
            }));
        // endpoints may have been recreated, so re-resolve and restyle here
        prim->from = endpoint("from", "tail", vec2(0.4, 0.5));
        prim->to = endpoint("to", "tip", vec2(0.6, 0.5));
        auto offset = [&](const char* key) {
            return spec.contains(key) ? readVec2(spec[key], key) : vec2(0, 0);
        };
        prim->from.offset = offset("from_offset");
        prim->to.offset = offset("to_offset");
        prim->bend = spec.value("bend", 0.);
        prim->head = spec.value("head", 0.015);
        prim->margin = spec.value("margin", 0.01);
        prim->style.thickness = spec.value("thickness", 3.);
        if (spec.contains("color"))
            prim->style.color = readColor(spec["color"], glm::vec4(0.f, 0.f, 0.f, 1.f));
        if (!id.empty())
            named[id] = prim;
        StateInSlide sis;
        sis.alpha = item.value("alpha", 1.);
        show.addToLastSlide({prim, sis});
        markUsed(prim);
    }
    else if (item.contains("box")) {
        if (!item["box"].is_array())
            throw std::runtime_error("\"box\" item needs a list of items to englobe");

        auto prim = std::static_pointer_cast<Box2D>(
            cached("box:" + item.dump(), [&]() -> PrimitivePtr { return Box2D::Add(); }));

        // inserted before its content, so it stays behind what it englobes
        StateInSlide sis;
        sis.alpha = item.value("alpha", 1.);
        show.addToLastSlide({prim, sis});
        markUsed(prim);

        // build the children as regular items, and englobe the screen
        // primitives they place, one shown on an earlier slide included
        const auto children = collect([&] { buildFrame(show, item["box"]); });

        // targets may have been recreated, so re-resolve and restyle here
        std::vector<ScreenPrimitivePtr> targets;
        for (const auto& p : children)
            if (auto sp = std::dynamic_pointer_cast<ScreenPrimitive>(p))
                targets.push_back(sp);
        prim->setTargets(targets);

        prim->setPadding(item.value("padding", 0.02));
        if (item.contains("padx"))
            prim->padding(0) = item["padx"].get<scalar>();
        if (item.contains("pady"))
            prim->padding(1) = item["pady"].get<scalar>();
        prim->style.thickness = item.value("thickness", 3.);
        prim->style.filled = item.value("filled", false);
        if (item.contains("color"))
            prim->style.color = readColor(item["color"], glm::vec4(0.f, 0.f, 0.f, 1.f));
        if (item.contains("fill_color")) {
            prim->setFillColor(readColor(item["fill_color"], glm::vec4(0.f, 0.f, 0.f, 0.25f)));
            prim->style.filled = true;
        }

        if (item.contains("id"))
            named[item["id"].get<std::string>()] = prim;
    }
    else if (item.contains("stack")) {
        if (!item["stack"].is_array())
            throw std::runtime_error("\"stack\" item needs a list of items to lay out");

        auto prim = std::static_pointer_cast<Stack2D>(
            cached("stack:" + item.dump(), [&]() -> PrimitivePtr { return Stack2D::Add(); }));
        prim->handle = makeHandleAnchor(item);
        prim->spacing = item.value("spacing", 0.015);
        std::string align = item.value("align", "left");
        if (align == "left")        prim->align = Stack2D::Align::LEFT;
        else if (align == "center") prim->align = Stack2D::Align::CENTER;
        else if (align == "right")  prim->align = Stack2D::Align::RIGHT;
        else throw std::runtime_error("stack align must be left, center or right");

        prim->clearChildren();
        show.addToLastSlide({prim, StateInSlide(prim->handle)});
        markUsed(prim);
        if (item.contains("id"))
            named[item["id"].get<std::string>()] = prim;

        buildStackChildren(show, prim, item["stack"]);
    }
    else if (item.contains("camera")) {
        std::string name = item["camera"];
        bool fly = item.value("fly", false); // a camera cuts unless asked to fly
        std::string key = name + (fly ? ":fly" : "");
        if (!camera_cache.count(key)) {
            CameraEntry entry;
            entry.cam = CameraView::Add(name, fly);
            entry.file = formatCameraFilename(name);
            try {
                entry.last_modified = std::filesystem::last_write_time(entry.file);
            } catch (const std::exception&) {}
            camera_cache[key] = entry;
        }
        show << camera_cache[key].cam;
    }
    else if (item.contains("background")) {
        const auto& b = item["background"];
        if (b.is_string())
            show << Background(b.get<std::string>());
        else if (b.is_array() && (b.size() == 3 || b.size() == 4))
            show << Background(b[0].get<float>(), b[1].get<float>(), b[2].get<float>(),
                               b.size() == 4 ? b[3].get<float>() : 1.f);
        else
            spdlog::warn("[deck] background wants a palette name or [r,g,b(,a)]");
    }
    else if (item.contains("pause")) {
        show << Pause::Add(item["pause"].get<TimeTypeSec>());
    }
    else {
        auto [prim, name] = makeScreenPrimitive(item);
        placeScreenItem(show, prim, item, name);
    }
}

}
