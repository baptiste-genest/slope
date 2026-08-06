#include "DeckLoader.h"
#include "Slideshow.h"
#include "../content/screen_primitives/LateX.h"
#include "../content/screen_primitives/Shape2D.h"
#include "../content/screen_primitives/Stack2D.h"
#include "../content/screen_primitives/Shader.h"
#include "../content/Params.h"
#include "../content/polyscope_primitives/Mesh.h"
#include "spdlog/spdlog.h"
#include "yaml-cpp/yaml.h"

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
        source = yamlToJson(YAML::LoadFile(source_path));
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
    if (!deck_changed && !cams_changed)
        return;
    spdlog::info("{} changed, rebuilding slides...",
                 deck_changed ? "deck file" : "camera view");
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
        if (key != "slides" && key != "commands" && key != "latex")
            spdlog::warn("deck: ignored top-level key \"{}\"", key);

    used_primitives.clear();
    named.clear();
    show.clearGroups();
    show.clearKeyframes();

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
            continue;
        }
        if (!item.is_object())
            throw std::runtime_error("deck items must be yaml maps (or the bare \"- step\" marker)");
        if (item.contains("step"))
            throw std::runtime_error("\"step:\" subtrees were replaced by the flat "
                                     "\"- step\" marker : items after it belong to the next step");
        if (item.contains("group")) {
            // membership is declared per item : every primitive the item
            // adds (a box subtree included) joins the tagged group
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

// anchor labels double as .pos filenames, so a title is named after its text :
// every title used to be called "title", silently sharing one anchor, hence one
// position and one scale
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

// The type of a uniform written as a bare value : the yaml literal says it.
//   1.0 -> float   3 -> int   true -> bool
//   [x,y] -> vec2  [x,y,z] -> vec3  [r,g,b,a] / "#rrggbb" -> color
static std::string inferUniformType(const json& v)
{
    if (v.is_boolean())        return "bool";
    if (v.is_number_integer()) return "int";
    if (v.is_number())         return "float";
    if (v.is_string())         return "color";     // "#rrggbb"
    if (v.is_array())
        switch (v.size()) {
        case 2: return "vec2";
        case 3: return "vec3";
        case 4: return "color";
        }
    return "";
}

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

// "uniforms:" on a shader item. Each entry becomes a persistent, runtime
// tunable Params entry (Tuner panel, saved to views/params.json) bound to the
// shader uniform of the same name and re-read every frame — so the value a
// shader reacts to is dragged live and survives the session, with no C++.
//
//   uniforms:
//     sun:   [0.3, 0.9, 0.2]                  # type read off the literal
//     tint:  "#ffcc88"
//     speed: {default: 1.0, min: 0, max: 5}   # long form : bounds -> slider
//     steps: {type: int, default: 64, max: 200}
//
// The parameter is named "<item>/<uniform>", which is also how the Tuner panel
// groups it. A uniform the compiled program does not declare is ignored, like
// every other Shader::bind : editing the .frag live never breaks the deck.
void DeckLoader::declareShaderUniforms(const ShaderPtr& shader, const json& item,
                                       const std::string& ref)
{
    // the shader is cached across rebuilds : re-declaring the whole set is
    // what makes a uniform deleted from the manifest actually disappear
    shader->clearUniforms();
    if (!item.contains("uniforms"))
        return;
    const json& us = item["uniforms"];
    if (!us.is_object())
        throw std::runtime_error("\"uniforms\" must be a map of "
                                 "name: default (or name: {type, default, min, max})");

    for (const auto& [name, spec] : us.items()) {
        if (!validGLSLName(name)) {
            spdlog::warn("deck: \"{}\" is not a usable GLSL uniform name, ignored", name);
            continue;
        }
        json def = spec;
        std::string type;
        scalar mn = 0, mx = 0;
        if (spec.is_object()) {
            def = spec.value("default", json());
            type = spec.value("type", "");
            mn = spec.value("min", scalar(0));
            mx = spec.value("max", scalar(0));
            if (type.empty())
                type = inferUniformType(def);
            if (type.empty())
                throw std::runtime_error("uniform \"" + name + "\" needs a \"type\" "
                                         "(float/int/bool/vec2/vec3/color) or a "
                                         "\"default\" to read it from");
        } else {
            type = inferUniformType(spec);
            if (type.empty())
                throw std::runtime_error("uniform \"" + name + "\" : cannot tell the type "
                                         "of " + spec.dump() + ", use the long form "
                                         "{type: ..., default: ...}");
        }

        const std::string pname = ref + "/" + name;
        if (type == "float") {
            auto p = Params::Add(pname, def.is_null() ? 0. : def.get<scalar>(), mn, mx);
            shader->bind(name, [p] { return scalar(p); });
        } else if (type == "int") {
            auto p = Params::AddInt(pname, def.is_null() ? 0 : def.get<int>(),
                                    int(mn), int(mx));
            shader->bindInt(name, [p] { return int(p); });
        } else if (type == "bool") {
            auto p = Params::AddBool(pname, def.is_null() ? false : def.get<bool>());
            shader->bindInt(name, [p] { return bool(p) ? 1 : 0; });
        } else if (type == "vec2") {
            auto p = Params::AddVec2(pname, def.is_null() ? vec2::Zero() : parseVec2(def), mn, mx);
            shader->bind(name, [p] { return vec2(p); });
        } else if (type == "vec3") {
            auto p = Params::AddVec(pname, def.is_null() ? vec::Zero() : parseVec3(def), mn, mx);
            shader->bind(name, [p] { return vec(p); });
        } else if (type == "color") {
            auto p = Params::AddColor(pname, def.is_null() ? RGBA(1.f,1.f,1.f,1.f)
                                                           : parseColor(def));
            shader->bind(name, [p] { return RGBA(p); });
        } else {
            throw std::runtime_error("uniform \"" + name + "\" : unknown type \"" + type
                                     + "\" (float/int/bool/vec2/vec3/color)");
        }
    }
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
// Only image files : a texture fed by another shader's output, or by a
// previous frame, needs a streaming order the manifest cannot express, and
// stays on the C++ side.
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
    ShaderPtr shader;   // set by the shader branch : its uniforms come last,
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
        prim = cached(id + "latex:" + txt + ":" + std::to_string(scale),
                      [&] { return Latex::Add(txt, scale); });
        name = "";
    }
    else if (item.contains("formula")) {
        std::string txt = item["formula"];
        scalar scale = item.value("scale", Options::DefaultLatexScale);
        prim = cached(id + "formula:" + txt + ":" + std::to_string(scale),
                      [&] { return Formula::Add(txt, scale); });
        name = "";
    }
    else if (item.contains("image")) {
        std::string file = item["image"];
        scalar scale = item.value("scale", 1.);
        prim = cached(id + "image:" + file + ":" + std::to_string(scale),
                      [&] { return Image::Add(file, scale); });
        name = std::filesystem::path(file).stem().string();
    }
    else if (item.contains("shader")) {
        // a single-pass fragment shader, its uniforms declared right here.
        // Multi-pass, channels and SSBOs stay on the C++ side : they need an
        // ordering and inputs a manifest cannot express.
        std::string file = item["shader"];
        int w = 0, h = 0;
        if (item.contains("resolution")) {
            const json& r = item["resolution"];
            if (!r.is_array() || r.size() != 2)
                throw std::runtime_error("\"resolution\" must be [width, height]");
            w = r[0].get<int>();
            h = r[1].get<int>();
        }
        // the resolution is part of the key : the same .frag shown at two sizes
        // is two primitives, and a hot reload reuses the GL resources of each
        prim = cached(id + "shader:" + file + ":" + std::to_string(w) + "x" + std::to_string(h),
                      [&]() -> PrimitivePtr { return Shader::FromFile(file, w, h); });
        name = std::filesystem::path(file).stem().string();
        shader = std::static_pointer_cast<Shader>(prim);
    }
    else
        throw std::runtime_error("expected a screen item "
                                 "(title/load/latex/formula/image/shader), got: "
                                 + item.dump());

    name = item.value("id", name);
    // uniforms are named after the item, so two placements of the same .frag
    // under different ids get their own tunable set
    if (shader) {
        declareShaderUniforms(shader, item, name);
        declareShaderTextures(shader, item);
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
            continue;
        }
        if (!item.is_object())
            throw std::runtime_error("stack items must be yaml maps (or the bare \"- step\" marker)");
        if (item.contains("step"))
            throw std::runtime_error("\"step:\" subtrees were replaced by the flat "
                                     "\"- step\" marker : items after it belong to the next step");
        if (item.contains("at")) { // explicit placement : escapes the layout
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

// handle anchor of a stack item : "at" as [x,y] gives a fixed handle,
// as a string a drag-editable label (falling back to the id)
AnchorPtr DeckLoader::makeHandleAnchor(const json& item)
{
    if (item.contains("at") && item["at"].is_array())
        return AbsoluteAnchor::Add(vec2(item["at"][0].get<scalar>(),
                                        item["at"][1].get<scalar>()));
    if (item.contains("at"))
        return LabelAnchor::Add(item["at"].get<std::string>());
    return LabelAnchor::Add(item.value("id", "stack"));
}

// applies the placement fields of a screen item :
// at (label, [x,y] or named position) or below/above/right_of/left_of
void DeckLoader::placeScreenItem(SlideManager& show, ScreenPrimitivePtr prim,
                                 const json& item, const std::string& default_label)
{
    scalar alpha = item.value("alpha", 1.);

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
        ScreenPrimitivePtr other = nullptr; // null : relative to last inserted
        if (item[rel.key].is_string())
            other = resolveScreen(item[rel.key]);
        show << PlaceRelative(prim, other, rel.X, rel.Y, padding, padding);
        used_primitives.insert(prim);
        return;
    }

    ScreenPrimitiveInSlide pis;
    if (item.contains("at") && item["at"].is_array())
        pis = prim->at(vec2(item["at"][0].get<scalar>(), item["at"][1].get<scalar>()), alpha);
    else if (item.contains("at")) {
        std::string at = item["at"];
        if (at == "TOP") pis = prim->at(TOP, alpha);
        else if (at == "CENTER") pis = prim->at(CENTER, alpha);
        else if (at == "BOTTOM") pis = prim->at(BOTTOM, alpha);
        else pis = prim->at(at, alpha);
    }
    else if (default_label != "")
        pis = prim->at(default_label, alpha);
    else {
        // no placement given : center, like `show << primitive`
        show << std::static_pointer_cast<Primitive>(prim);
        used_primitives.insert(prim);
        return;
    }
    show.addToLastSlide(pis);
    used_primitives.insert(pis.first);
}

static RGBA parseColor(const json& c)
{
    if (c.is_array())
        return RGBA((float)c[0], (float)c[1], (float)c[2],
                    c.size() > 3 ? (float)c[3] : 1.f);
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
        {"id","at","alpha","below","above","right_of","left_of","padding","group"};
    auto with = [](std::set<std::string> s, std::initializer_list<std::string> more) {
        s.insert(more); return s;
    };
    static const std::map<std::string, std::set<std::string>> allowed = {
        {"title",   placement},
        {"load",    placement},
        {"latex",   with(placement, {"scale"})},
        {"formula", with(placement, {"scale"})},
        {"image",   with(placement, {"scale"})},
        {"shader",  with(placement, {"resolution","uniforms","textures"})},
        {"object",  {"id","at","alpha","group"}},
        {"mesh",    {"id","at","alpha","smooth","normalize","group"}},
        {"arrow",   {"id","alpha","group"}},
        {"box",     {"id","alpha","padding","padx","pady","thickness","color","fill_color","filled","group"}},
        {"stack",   {"id","at","spacing","align","group"}},
        {"camera",  {"fly"}},
        {"pause",   {}},
        {"keyframe",{}},
        {"remove",  {}},
        {"replace", {"with"}},
        {"set",     {"at","alpha","below","above","right_of","left_of","padding"}},
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
        // re-places an already defined item (referenced by id or content)
        // without redefining it : same placement fields as a regular item
        auto prim = resolveScreen(item["set"]);
        placeScreenItem(show, prim, item, "");
    }
    else if (item.contains("replace")) {
        if (!item.contains("with"))
            throw std::runtime_error("\"replace\" item needs a \"with\" sub-item");
        std::string replaced = item["replace"];
        auto old = resolveScreen(replaced);
        auto [prim, name] = makeScreenPrimitive(item["with"]);
        show << Replace(prim, old);
        // the name now refers to the replacement : without this a second
        // "replace: <name>" (or a later "set:") would still resolve to the
        // primitive that has just been taken off the slide
        named[replaced] = prim;
        used_primitives.insert(prim);
    }
    else if (item.contains("object")) {
        std::string name = item["object"];
        if (group_registry.count(name) && !instantiated_groups.count(name))
            instantiated_groups[name] = group_registry[name]();
        if (instantiated_groups.count(name)) {
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
                e.fixed = vec2(v[0].get<scalar>(), v[1].get<scalar>());
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
        // referenced primitives may have been recreated : re-resolve on
        // every build, and re-apply the (hot-editable) style
        prim->from = endpoint(spec["from"]);
        prim->to = endpoint(spec["to"]);
        auto offset = [&](const char* key) {
            return spec.contains(key)
                ? vec2(spec[key][0].get<scalar>(), spec[key][1].get<scalar>())
                : vec2(0, 0);
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

        // inserted before its content : the box stays behind what it
        // englobes, but covers items added before it in the manifest
        StateInSlide sis;
        sis.alpha = item.value("alpha", 1.);
        show.addToLastSlide({prim, sis});
        used_primitives.insert(prim);

        // build the children as regular items, and englobe the screen
        // primitives they add (diffed through used_primitives)
        auto before = used_primitives;
        buildFrame(show, item["box"]);

        // englobed primitives may have been recreated : re-resolve on
        // every build, and re-apply the (hot-editable) style
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
