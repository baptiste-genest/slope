#include "../content/Snippet.h"
#include "DeckLoader.h"
#include "Slideshow.h"
#include "../content/screen_primitives/LateX.h"
#include "../content/screen_primitives/Shape2D.h"
#include "../content/screen_primitives/Stack2D.h"
#include "../content/screen_primitives/Shader.h"
#include "../content/screen_primitives/Video.h"
#include "../content/screen_primitives/Webcam.h"
#include "../content/Params.h"
#include "../content/polyscope_primitives/Mesh.h"
#include "../content/polyscope_primitives/PolyscopeSnippets.h"
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

static void warnUnknownKeys(const json& item);

DeckLoader::DeckLoader() {}
DeckLoader::~DeckLoader() {}

void DeckLoader::init(path deck_file)
{
    source_path = formatPath(deck_file);
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
    static auto last_refresh = Time::now();
    if (TimeFrom(last_refresh) < 0.2)
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
    spdlog::info("{} changed, rebuilding slides...",
                 deck_changed ? "deck file" : (cams_changed ? "camera view" : "latex source"));
    show.recompose([this](SlideManager& sm) { build(sm); }, used_primitives);
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

// "view" is the half-height, a number or a snippet name. Without one a shader
// has no world space and nothing can follow a point of it.
void DeckLoader::declareShaderView(const ShaderPtr& shader, const json& item)
{
    if (!item.contains("view"))
        return;
    const json& v = item["view"];

    auto halfOf = [](const json& h) -> std::function<scalar()> {
        if (h.is_number()) {
            scalar x = h.get<scalar>();
            if (!(x > 0))
                throw std::runtime_error("\"view\" half-height must be greater than zero");
            return [x] { return x; };
        }
        if (h.is_string()) { std::string n = h; return [n] { return Snippet::get(n).num(); }; }
        throw std::runtime_error("\"view\" half-height must be a number or a snippet name");
    };
    auto centerOf = [](const json& c) -> std::function<vec2()> {
        if (c.is_array() && c.size() == 2) {
            vec2 p(c[0].get<scalar>(), c[1].get<scalar>());
            return [p] { return p; };
        }
        if (c.is_string()) { std::string n = c; return [n] { return Snippet::get(n).v2(); }; }
        throw std::runtime_error("\"view\" center must be [x, y] or a snippet name");
    };
    std::function<vec2()> origin = [] { return vec2::Zero(); };

    if (v.is_object()) {
        if (!v.contains("half"))
            throw std::runtime_error("\"view\" needs a \"half\" : half the height it "
                                     "shows, in world units");
        shader->bindView(v.contains("center") ? centerOf(v["center"]) : origin,
                         halfOf(v["half"]));
        return;
    }
    shader->bindView(origin, halfOf(v));
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

void DeckLoader::build(SlideManager& show)
{
    if (!source.contains("slides") || !source["slides"].is_array())
        throw std::runtime_error("deck file must contain a top-level \"slides\" array");
    for (const auto& [key, val] : source.items())
        if (key != "slides" && key != "commands" && key != "latex" && key != "snippets")
            spdlog::warn("deck: ignored top-level key \"{}\"", key);

    used_primitives.clear();
    named.clear();
    show.clearGroups();
    show.clearKeyframes();

    // drop what an "object:" item no longer declares, and only that, the rest
    // of that shader's binds belong to its C++ owner
    for (auto& [object, declared] : object_uniforms)
        for (const auto& name : declared.second)
            declared.first->unset(name);
    object_uniforms.clear();

    bool first = true;
    for (const auto& frame : source["slides"]) {
        const json* items = nullptr;
        bool same_title = false;
        if (frame.is_array())
            items = &frame;
        else if (frame.is_object() && frame.contains("frame") && frame["frame"].is_array()) {
            items = &frame["frame"];
            same_title = frame.value("same_title", false);
        }
        else
            throw std::runtime_error("each element of \"slides\" must be \"- frame:\" "
                                     "followed by a list of items");
        if (!first)
            show << (same_title ? newFrameSameTitle : newFrame);
        first = false;
        step_primitives.clear();
        buildFrame(show, *items);
    }
    if (show.getNumberSlides() == 0)
        show.addSlide(Slide());
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
        if (!item.is_object())
            throw std::runtime_error("deck items must be yaml maps (or the bare \"- step\" marker)");
        if (item.contains("step"))
            throw std::runtime_error("\"step:\" subtrees were replaced by the flat "
                                     "\"- step\" marker : items after it belong to the next step");
        if (item.contains("group")) {
            // every primitive the item adds, box subtree included, joins the group
            auto before = used_primitives;
            addItem(show, item);
            for (const auto& p : used_primitives)
                if (!before.count(p))
                    show.addToGroup(item["group"].get<std::string>(), p);
        } else {
            addItem(show, item);
        }
    }
}

// anchor labels double as .pos filenames, so a title is named after its text,
// otherwise every title shares one anchor, one position and one scale
static std::string titleLabel(const std::string& txt)
{
    std::string slug;
    for (char c : txt) {
        if (std::isalnum(static_cast<unsigned char>(c)))
            slug += c;
        else if (!slug.empty() && slug.back() != '_')
            slug += '_';
        if (slug.size() >= 40)
            break;
    }
    while (!slug.empty() && slug.back() == '_')
        slug.pop_back();
    return slug.empty() ? "title" : "title_" + slug;
}

static RGBA parseColor(const json& c);

// a uniform name has to survive being pasted into GLSL as-is
static bool validGLSLName(const std::string& n)
{
    if (n.empty() || (!std::isalpha((unsigned char)n[0]) && n[0] != '_'))
        return false;
    for (char c : n)
        if (!std::isalnum((unsigned char)c) && c != '_')
            return false;
    return n.compare(0, 3, "gl_") != 0;
}

static const char* uniform_types = "float/int/bool/vec2/vec3/dir/color";

static vec2 parseVec2(const json& v)
{
    if (!v.is_array() || v.size() != 2)
        throw std::runtime_error("a vec2 uniform default must be [x, y]");
    return vec2(v[0].get<scalar>(), v[1].get<scalar>());
}

static vec parseVec3(const json& v)
{
    if (!v.is_array() || v.size() != 3)
        throw std::runtime_error("a vec3 uniform default must be [x, y, z]");
    return vec(v[0].get<scalar>(), v[1].get<scalar>(), v[2].get<scalar>());
}

// yaml takes [0.5] as happily as [x, y], and reading past the end of a json
// array is undefined rather than an error
static vec2 readVec2(const json& v, const std::string& what)
{
    if (!v.is_array() || v.size() != 2)
        throw std::runtime_error("\"" + what + "\" must be [x, y]");
    return vec2(v[0].get<scalar>(), v[1].get<scalar>());
}

// "glsl" is the name the shader sees, "pname" the parameter it is bound to.
// The two differ only for an array element, "controls[3]".
static void declareUniform(const ShaderPtr& shader, const std::string& glsl,
                           const std::string& pname, const std::string& type,
                           const json& def, scalar mn, scalar mx)
{
    if (type == "float") {
        auto p = Params::Add(pname, def.is_null() ? 0. : def.get<scalar>(), mn, mx);
        shader->bind(glsl, [p] { return scalar(p); });
    } else if (type == "int") {
        auto p = Params::AddInt(pname, def.is_null() ? 0 : def.get<int>(),
                                int(mn), int(mx));
        shader->bindInt(glsl, [p] { return int(p); });
    } else if (type == "bool") {
        auto p = Params::AddBool(pname, def.is_null() ? false : def.get<bool>());
        shader->bindInt(glsl, [p] { return bool(p) ? 1 : 0; });
    } else if (type == "vec2") {
        auto p = Params::AddVec2(pname, def.is_null() ? vec2::Zero() : parseVec2(def), mn, mx);
        shader->bind(glsl, [p] { return vec2(p); });
    } else if (type == "vec3") {
        auto p = Params::AddVec(pname, def.is_null() ? vec::Zero() : parseVec3(def), mn, mx);
        shader->bind(glsl, [p] { return vec(p); });
    } else if (type == "dir") {
        auto p = Params::AddDir(pname, def.is_null() ? vec(0,0,1) : parseVec3(def));
        shader->bind(glsl, [p] { return vec(p); });
    } else if (type == "color") {
        auto p = Params::AddColor(pname, def.is_null() ? RGBA(1.f,1.f,1.f,1.f)
                                                       : parseColor(def));
        shader->bind(glsl, [p] { return RGBA(p); });
    } else {
        throw std::runtime_error("uniform \"" + glsl + "\" : unknown type \"" + type
                                 + "\" (" + uniform_types + ")");
    }
}

// the N of a "vec3[8]", or 0 when the type carries no array suffix
static int arrayCount(const std::string& name, const std::string& type)
{
    auto open = type.find('[');
    if (open == std::string::npos)
        return 0;
    if (type.back() != ']')
        throw std::runtime_error("uniform \"" + name + "\" : malformed array type \""
                                 + type + "\", write \"<type>[N]\"");
    int n = 0;
    try {
        size_t used = 0;
        n = std::stoi(type.substr(open + 1, type.size() - open - 2), &used);
        if (used != type.size() - open - 2)
            n = 0;
    } catch (const std::exception&) {
        n = 0;
    }
    if (n < 1 || n > 64)
        throw std::runtime_error("uniform \"" + name + "\" : an array uniform needs a "
                                 "size between 1 and 64, got \"" + type + "\"");
    return n;
}

// "uniforms:" on a shader item, or on an "object:" naming a shader. Each entry
// becomes a persistent, runtime tunable Params entry (Tuner panel, saved to
// views/params.json) bound to the shader uniform of the same name and re-read
// every frame, so the value a shader reacts to is dragged live and survives
// the session, with no C++.
//
//   uniforms:
//     sun:      dir                                 # a type name, zero valued
//     tint:     {type: color, default: "#ffcc88"}   # long form, with a default
//     speed:    {type: float, default: 1.0, min: 0, max: 5}  # bounds -> slider
//     controls: vec3[8]                             # an array, one parameter
//                                                   # per element, controls[i]
//
// An array's "default" is a list of one value per element. Quote the type in a
// flow mapping, {type: "vec3[8]", ...}, where yaml reads brackets itself.
//
// The parameter is named "<item>/<uniform>", which is also how the Tuner panel
// groups it. A uniform the compiled program does not declare is ignored, like
// every other Shader::bind, so editing the .frag live never breaks the deck.
//
// Returns the names it declared. `clear` drops the shader's whole user set
// first, which only suits a shader the deck created and owns.
std::vector<std::string> DeckLoader::declareShaderUniforms(const ShaderPtr& shader,
                                                           const json& item,
                                                           const std::string& ref, bool clear)
{
    std::vector<std::string> declared;
    // the shader is cached across rebuilds, so a dropped uniform needs this
    if (clear)
        shader->clearUniforms();
    if (!item.contains("uniforms"))
        return declared;
    const json& us = item["uniforms"];

    // Two spellings. A list lets a name that needs no type stand on its own,
    // and a map is the older "name: type" form; entries may be mixed.
    std::vector<std::pair<std::string, json>> entries;
    if (us.is_array()) {
        for (const auto& e : us) {
            if (e.is_string())
                entries.emplace_back(e.get<std::string>(), json());
            else if (e.is_object())
                for (const auto& [k, v] : e.items())
                    entries.emplace_back(k, v);
            else if (e.is_boolean() || e.is_number())
                // yaml reads y, n, on, off, yes and no as booleans, and "1e5"
                // and friends as numbers, so such a name arrives already coerced
                throw std::runtime_error("a \"uniforms\" name was read as " + e.dump()
                                         + " : yaml treats y, n, on, off, yes and no as "
                                         "booleans, so quote it, - \"y\"");
            else
                throw std::runtime_error("a \"uniforms\" entry is a bare name, or "
                                         "\"name: <type>\"");
        }
    }
    else if (us.is_object()) {
        for (const auto& [k, v] : us.items())
            entries.emplace_back(k, v);
    }
    else
        throw std::runtime_error("\"uniforms\" must be a list of names, or a map of "
                                 "name: type (or name: {type, default, min, max})");

    for (const auto& [name, spec] : entries) {
        if (!validGLSLName(name)) {
            spdlog::warn("deck: \"{}\" is not a usable GLSL uniform name, ignored", name);
            continue;
        }
        // No type, so the name must already exist, as a parameter declared
        // earlier or as a snippet variable. One namespace, so either works, and
        // a name that resolves to nothing says so once
        if (spec.is_null()) {
            shader->bind(name);
            declared.push_back(name);
            continue;
        }

        json def;
        std::string type;
        scalar mn = 0, mx = 0;
        if (spec.is_string()) {
            type = spec.get<std::string>();
        } else if (spec.is_object()) {
            def = spec.value("default", json());
            type = spec.value("type", "");
            mn = spec.value("min", scalar(0));
            mx = spec.value("max", scalar(0));
            if (type.empty())
                throw std::runtime_error("uniform \"" + name + "\" needs a \"type\" ("
                                         + uniform_types + ")");
        } else {
            throw std::runtime_error("uniform \"" + name + "\" : write its type, "
                                     "\"" + name + ": <" + uniform_types + ">\", or the "
                                     "long form {type: ..., default: ...}");
        }

        int count = arrayCount(name, type);
        if (count == 0) {
            declareUniform(shader, name, ref + "/" + name, type, def, mn, mx);
            declared.push_back(name);
            continue;
        }
        if (!def.is_null() && (!def.is_array() || int(def.size()) != count))
            throw std::runtime_error("uniform \"" + name + "\" : its \"default\" must be "
                                     "a list of " + std::to_string(count) + " values, one "
                                     "per element");
        std::string base = type.substr(0, type.find('['));
        for (int i = 0; i < count; i++) {
            std::string idx = "[" + std::to_string(i) + "]";
            declareUniform(shader, name + idx, ref + "/" + name + idx, base,
                           def.is_null() ? json() : def[i], mn, mx);
            declared.push_back(name + idx);
        }
    }
    return declared;
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

// "textures:" on a shader item. Each entry binds an image file to the sampler
// of the same name, which the shader declares itself :
//
//   uniform sampler2D noise;        // in the .frag
//   uniform vec2      noise_size;   // optional, its size in pixels
//
//   textures:
//     noise: noise.png
//     grad:  {file: gradient.png, filter: nearest, wrap: repeat}
//
// Only image files. A texture fed by another pass needs a streaming order the
// manifest cannot express, and stays on the C++ side.
void DeckLoader::declareShaderTextures(const ShaderPtr& shader, const json& item)
{
    std::vector<std::string> declared;
    if (item.contains("textures")) {
        const json& ts = item["textures"];
        if (!ts.is_object())
            throw std::runtime_error("\"textures\" must be a map of "
                                     "name: file (or name: {file, filter, wrap})");
        for (const auto& [name, spec] : ts.items()) {
            if (!validGLSLName(name)) {
                spdlog::warn("deck: \"{}\" is not a usable GLSL sampler name, ignored", name);
                continue;
            }
            std::string file;
            auto filter = Shader::Filter::Linear;
            auto wrap   = Shader::Wrap::Clamp;
            if (spec.is_object()) {
                if (!spec.contains("file"))
                    throw std::runtime_error("texture \"" + name + "\" needs a \"file\"");
                file = spec["file"];
                const std::string fs = spec.value("filter", "linear");
                const std::string ws = spec.value("wrap", "clamp");
                if      (fs == "nearest") filter = Shader::Filter::Nearest;
                else if (fs != "linear")
                    throw std::runtime_error("texture \"" + name + "\" : filter must be "
                                             "\"nearest\" or \"linear\"");
                if      (ws == "repeat") wrap = Shader::Wrap::Repeat;
                else if (ws != "clamp")
                    throw std::runtime_error("texture \"" + name + "\" : wrap must be "
                                             "\"clamp\" or \"repeat\"");
            } else if (spec.is_string()) {
                file = spec;
            } else {
                throw std::runtime_error("texture \"" + name + "\" must be a file name "
                                         "or {file: ..., filter: ..., wrap: ...}");
            }
            // re-setting the same file is a no-op, so a hot reload does not
            // re-decode every image on every save
            shader->setTexture(name, file, filter, wrap);
            declared.push_back(name);
        }
    }
    // whatever the manifest no longer declares is unbound (the shader itself
    // is cached across rebuilds, so nothing else would drop it)
    shader->retainTextures(declared);
}

std::pair<ScreenPrimitivePtr,std::string> DeckLoader::makeScreenPrimitive(const json& item)
{
    PrimitivePtr prim;
    std::string name;
    ShaderPtr shader;   // set by the shader branch, its uniforms come last,
                        // once the item's reference name is known
    // the id is part of the cache key, so two items with the same content
    // but different ids are distinct primitives (shown simultaneously)
    std::string id = "id=" + item.value("id", "") + ":";
    if (item.contains("title")) {
        std::string txt = item["title"];
        prim = cached(id + "title:" + txt, [&] { return Title(txt); });
        name = titleLabel(txt);
    }
    else if (item.contains("load")) {
        std::string key = item["load"];
        prim = cached(id + "load:" + key, [&] { return LatexLoader::Load(key); });
        name = key;
    }
    else if (item.contains("latex")) {
        std::string txt = item["latex"];
        scalar scale = item.value("scale", Options::DefaultLatexScale);
        int width = item.value("width", -1);
        prim = cached(id + "latex:" + txt + ":" + std::to_string(scale)
                          + ":" + std::to_string(width),
                      [&] { return Latex::Add(txt, scale, width); });
        name = "";
    }
    else if (item.contains("formula")) {
        std::string txt = item["formula"];
        scalar scale = item.value("scale", Options::DefaultLatexScale);
        int width = item.value("width", -1);
        prim = cached(id + "formula:" + txt + ":" + std::to_string(scale)
                          + ":" + std::to_string(width),
                      [&] { return Formula::Add(txt, scale, width); });
        name = "";
    }
    else if (item.contains("image")) {
        std::string file = item["image"];
        scalar scale = item.value("scale", 1.);
        prim = cached(id + "image:" + file + ":" + std::to_string(scale),
                      [&] { return Image::Add(file, scale); });
        name = std::filesystem::path(file).stem().string();
    }
    else if (item.contains("gif")) {
        std::string file = item["gif"];
        int fps = item.value("fps", 10);
        scalar scale = item.value("scale", 1.);
        bool loop = item.value("loop", true);
        prim = cached(id + "gif:" + file + ":" + std::to_string(fps) + ":"
                          + std::to_string(scale) + (loop ? ":loop" : ""),
                      [&]() -> PrimitivePtr { return Gif::Add(file, fps, scale, loop); });
        name = std::filesystem::path(file).stem().string();
    }
    else if (item.contains("video")) {
        std::string file = item["video"];
        int  dw       = item.value("decode_width", 0);
        bool loop     = item.value("loop", true);
        bool autoplay = item.value("autoplay", true);
        // those three shape the decoder and belong in the key, the fields
        // below are re-applied to the cached primitive on every build
        prim = cached(id + "video:" + file + ":" + std::to_string(dw)
                          + (loop ? ":loop" : "") + (autoplay ? ":auto" : ""),
                      [&]() -> PrimitivePtr {
                          return Video::Add(file, dw, loop, autoplay);
                      });
        auto vid = std::static_pointer_cast<Video>(prim);
        scalar sp = item.value("speed", 1.);
        vid->speed = [sp] { return sp; };
        vid->show_stats = item.value("stats", false);
        vid->scale = item.value("scale", 1.);
        name = std::filesystem::path(file).stem().string();
    }
    else if (item.contains("webcam")) {
        // the device is in the key, a camera only opens once
        std::string dev = item["webcam"];
        int w   = item.value("width", 1280);
        int h   = item.value("height", 720);
        int fps = item.value("fps", 30);
        std::string fmt = item.value("input_format", "mjpeg");
        prim = cached(id + "webcam:" + dev + ":" + std::to_string(w) + "x"
                          + std::to_string(h) + "@" + std::to_string(fps) + ":" + fmt,
                      [&]() -> PrimitivePtr { return Webcam::Add(dev, w, h, fps, fmt); });
        auto cam = std::static_pointer_cast<Webcam>(prim);
        cam->show_stats = item.value("stats", false);
        cam->scale = item.value("scale", 1.);
        name = std::filesystem::path(dev).stem().string();
    }
    else if (item.contains("shader")) {
        // a single-pass fragment shader, its uniforms declared right here.
        // Multi-pass, channels and SSBOs stay on the C++ side.
        std::string file = item["shader"];
        int w = 0, h = 0;
        if (item.contains("resolution")) {
            const json& r = item["resolution"];
            if (!r.is_array() || r.size() != 2)
                throw std::runtime_error("\"resolution\" must be [width, height]");
            w = r[0].get<int>();
            h = r[1].get<int>();
        }
        // the resolution is part of the key, one .frag at two sizes is two
        // primitives and a hot reload reuses the GL resources of each
        prim = cached(id + "shader:" + file + ":" + std::to_string(w) + "x" + std::to_string(h),
                      [&]() -> PrimitivePtr { return Shader::FromFile(file, w, h); });
        name = std::filesystem::path(file).stem().string();
        shader = std::static_pointer_cast<Shader>(prim);
    }
    else
        throw std::runtime_error("expected a screen item "
                                 "(title/load/latex/formula/image/gif/video/webcam/shader), "
                                 "got: "
                                 + item.dump());

    name = item.value("id", name);
    // uniforms are named after the item, so two placements of the same .frag
    // under different ids get their own tunable set
    if (shader) {
        declareShaderUniforms(shader, item, name);
        declareShaderTextures(shader, item);
        declareShaderView(shader, item);
    }
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
        used_primitives.insert(prim);
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
}

// applies the placement fields of a screen item, at (label, [x,y] or a named
// position) or below/above/right_of/left_of
void DeckLoader::placeScreenItem(SlideManager& show, ScreenPrimitivePtr prim,
                                 const json& item, const std::string& default_label,
                                 bool keep_placement)
{
    scalar alpha = item.value("alpha", 1.);

    // Two items of the same content are one cached primitive, and a slide holds
    // each once, so the second placement would move the first rather than show a
    // copy. A "set", or an item repeated after a "- step", re-places on purpose.
    if (!keep_placement && !step_primitives.insert(prim).second)
        spdlog::warn("deck: \"{}\" is placed twice on the same step. Both are the same "
                     "primitive, so the second placement moves the first rather than adding "
                     "a copy. Give them different \"id:\" to show both",
                     default_label.empty() ? item.dump() : default_label);

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
        used_primitives.insert(prim);
        return;
    }

    ScreenPrimitiveInSlide pis;
    if (item.contains("follow")) {
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
        if (at == "TOP") pis = prim->at(TOP, alpha);
        else if (at == "CENTER") pis = prim->at(CENTER, alpha);
        else if (at == "BOTTOM") pis = prim->at(BOTTOM, alpha);
        else pis = prim->at(at, alpha);
    }
    else if (default_label != "")
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
        used_primitives.insert(prim);
        return;
    }
    applyStateOptions(pis.second, item);
    show.addToLastSlide(pis);
    used_primitives.insert(pis.first);
}

static RGBA parseColor(const json& c)
{
    if (c.is_array()) {
        if (c.size() < 3 || c.size() > 4)
            throw std::runtime_error("color must be [r,g,b] or [r,g,b,a]");
        return RGBA((float)c[0], (float)c[1], (float)c[2],
                    c.size() > 3 ? (float)c[3] : 1.f);
    }
    std::string s = c;
    if (s.size() < 7 || s[0] != '#')
        throw std::runtime_error("color must be [r,g,b(,a)] or \"#rrggbb\"");
    auto hex = [&](int i) { return std::stoi(s.substr(i, 2), nullptr, 16); };
    return RGBA(hex(1)/255.f, hex(3)/255.f, hex(5)/255.f, 1.f);
}

// warns about misspelled or misplaced fields, which yaml would otherwise
// silently ignore (the deck is hand-edited live, so mistakes must be loud)
static void warnUnknownKeys(const json& item)
{
    static const std::set<std::string> placement =
        {"id","at","follow","offset","alpha","rot","zoom",
         "below","above","right_of","left_of","padding","group"};
    auto with = [](std::set<std::string> s, std::initializer_list<std::string> more) {
        s.insert(more); return s;
    };
    static const std::map<std::string, std::set<std::string>> allowed = {
        {"title",   placement},
        {"load",    placement},
        {"latex",   with(placement, {"scale","width"})},
        {"formula", with(placement, {"scale","width"})},
        {"image",   with(placement, {"scale"})},
        {"gif",     with(placement, {"scale","fps","loop"})},
        {"video",   with(placement, {"scale","decode_width","loop","autoplay","speed","stats"})},
        {"webcam",  with(placement, {"scale","width","height","fps","input_format","stats"})},
        {"shader",  with(placement, {"resolution","uniforms","textures","view"})},
        {"object",  {"id","at","follow","offset","alpha","rot","zoom","view","group",
                     "uniforms","textures"}},
        {"mesh",    {"id","at","alpha","smooth","normalize","group"}},
        {"surface", {"id","at","alpha","smooth","u","v","resolution","closed","group"}},
        {"curve",   {"id","at","alpha","u","resolution","closed","radius","group"}},
        {"arrow",   {"id","alpha","group"}},
        {"box",     {"id","alpha","padding","padx","pady","thickness","color","fill_color","filled","group"}},
        {"stack",   {"id","at","spacing","align","group"}},
        {"camera",  {"fly"}},
        {"pause",   {}},
        {"keyframe",{}},
        {"remove",  {}},
        {"replace", {"with"}},
        {"set",     {"at","alpha","rot","zoom",
                     "below","above","right_of","left_of","padding"}},
    };
    for (const auto& [type, fields] : allowed) {
        if (!item.contains(type))
            continue;
        for (const auto& [key, val] : item.items())
            if (key != type && !fields.count(key))
                spdlog::warn("deck: ignored key \"{}\" on a \"{}\" item", key, type);
        return;
    }
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
        used_primitives.insert(prim);
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
                used_primitives.insert(ptr);
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
        if (item.contains("at") && item["at"].is_string() && pis.first->isPolyscopePrimitive())
            pis = std::static_pointer_cast<PolyscopePrimitive>(pis.first)
                      ->at(item["at"].get<std::string>(), item.value("alpha", 1.));
        show.addToLastSlide(pis);
        used_primitives.insert(pis.first);
    }
    else if (item.contains("mesh")) {
        std::string file = item["mesh"];
        bool smooth = item.value("smooth", true);
        bool normalize = item.value("normalize", false);
        auto prim = std::static_pointer_cast<Mesh>(
            cached("mesh:" + file + (smooth ? ":smooth" : "") + (normalize ? ":norm" : ""),
                   [&]() -> PrimitivePtr {
                       auto m = Mesh::Add(file, smooth);
                       if (normalize)
                           m->normalize();
                       return m;
                   }));
        named[item.value("id", std::filesystem::path(file).stem().string())] = prim;
        scalar alpha = item.value("alpha", 1.);
        auto pis = item.contains("at") && item["at"].is_string()
            ? prim->at(item["at"].get<std::string>(), alpha)
            : prim->at(alpha);
        show.addToLastSlide(pis);
        used_primitives.insert(prim);
    }
    else if (item.contains("surface")) {
        SnippetSurface::Spec spec;
        spec.fn = item["surface"];
        spec.name = item.value("id", spec.fn);
        if (item.contains("u")) spec.u = readVec2(item["u"], "u");
        if (item.contains("v")) spec.v = readVec2(item["v"], "v");
        if (item.contains("resolution")) {
            const json& r = item["resolution"];
            if (r.is_array()) {
                vec2 n = readVec2(r, "resolution");
                spec.res_u = int(n(0));
                spec.res_v = int(n(1));
            } else
                spec.res_u = spec.res_v = r.get<int>();
        }
        if (item.contains("closed")) {
            const json& c = item["closed"];
            if (c.is_array()) {
                if (c.size() != 2)
                    throw std::runtime_error("\"closed\" must be a bool or [u, v]");
                spec.closed_u = c[0].get<bool>();
                spec.closed_v = c[1].get<bool>();
            } else
                spec.closed_u = spec.closed_v = c.get<bool>();
        }
        spec.smooth = item.value("smooth", true);

        // cached on identity alone, so editing the domain or the resolution
        // reconfigures the surface in place instead of building a second one
        auto prim = std::static_pointer_cast<SnippetSurface>(
            cached("surface:" + spec.name + ":" + spec.fn,
                   [&]() -> PrimitivePtr { return SnippetSurface::Add(spec); }));
        prim->configure(spec);
        named[spec.name] = prim;
        scalar alpha = item.value("alpha", 1.);
        auto pis = item.contains("at") && item["at"].is_string()
            ? prim->at(item["at"].get<std::string>(), alpha)
            : prim->at(alpha);
        show.addToLastSlide(pis);
        used_primitives.insert(prim);
    }
    else if (item.contains("curve")) {
        SnippetCurve::Spec spec;
        spec.fn = item["curve"];
        spec.name = item.value("id", spec.fn);
        if (item.contains("u")) spec.u = readVec2(item["u"], "u");
        spec.resolution = item.value("resolution", 200);
        spec.closed = item.value("closed", false);
        spec.radius = item.value("radius", -1.);

        auto prim = std::static_pointer_cast<SnippetCurve>(
            cached("curve:" + spec.name + ":" + spec.fn,
                   [&]() -> PrimitivePtr { return SnippetCurve::Add(spec); }));
        prim->configure(spec);
        named[spec.name] = prim;
        scalar alpha = item.value("alpha", 1.);
        auto pis = item.contains("at") && item["at"].is_string()
            ? prim->at(item["at"].get<std::string>(), alpha)
            : prim->at(alpha);
        show.addToLastSlide(pis);
        used_primitives.insert(prim);
    }
    else if (item.contains("arrow")) {
        const json& spec = item["arrow"];
        if (!spec.is_object() || !spec.contains("from") || !spec.contains("to"))
            throw std::runtime_error("\"arrow\" item needs {from: ..., to: ...}");
        static const std::set<std::string> arrow_keys =
            {"from","to","from_offset","to_offset","bend","thickness","color","head","margin"};
        for (const auto& [key, val] : spec.items())
            if (!arrow_keys.count(key))
                spdlog::warn("deck: ignored key \"{}\" on an \"arrow\" item", key);

        // an endpoint is [x,y], the name of a previous item (attached at its
        // boundary, following it live), or otherwise a persistent label
        auto endpoint = [&](const json& v) -> Arrow2D::Endpoint {
            if (v.is_array()) {
                Arrow2D::Endpoint e;
                e.fixed = readVec2(v, "arrow endpoint");
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
                return Arrow2D::Add(endpoint(spec["from"]), endpoint(spec["to"]));
            }));
        // endpoints may have been recreated, so re-resolve and restyle here
        prim->from = endpoint(spec["from"]);
        prim->to = endpoint(spec["to"]);
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
            prim->style.color = parseColor(spec["color"]);
        if (item.contains("id"))
            named[item["id"].get<std::string>()] = prim;
        StateInSlide sis;
        sis.alpha = item.value("alpha", 1.);
        show.addToLastSlide({prim, sis});
        used_primitives.insert(prim);
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
        used_primitives.insert(prim);

        // build the children as regular items, and englobe the screen
        // primitives they add (diffed through used_primitives)
        auto before = used_primitives;
        buildFrame(show, item["box"]);

        // targets may have been recreated, so re-resolve and restyle here
        std::vector<ScreenPrimitivePtr> targets;
        for (const auto& p : used_primitives)
            if (!before.count(p))
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
            prim->style.color = parseColor(item["color"]);
        if (item.contains("fill_color")) {
            prim->setFillColor(parseColor(item["fill_color"]));
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
        used_primitives.insert(prim);
        if (item.contains("id"))
            named[item["id"].get<std::string>()] = prim;

        buildStackChildren(show, prim, item["stack"]);
    }
    else if (item.contains("camera")) {
        std::string name = item["camera"];
        bool fly = item.value("fly", false);
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
    else if (item.contains("pause")) {
        show << Pause::Add(item["pause"].get<TimeTypeSec>());
    }
    else {
        auto [prim, name] = makeScreenPrimitive(item);
        placeScreenItem(show, prim, item, name);
    }
}

}
