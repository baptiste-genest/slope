#include "content/authoring/Snippet.h"
#include "slides/deck/DeckLoader.h"
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
                buildFrame(show, *tmpl);
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
        used_primitives.insert(prim);
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

// mesh, surface and curve are built from the manifest alone, so one branch
// serves all three
static const ItemSpec* sceneSpecOf(const json& item)
{
    const ItemSpec* spec = findItemSpec(item);
    return spec && spec->kind == ItemSpec::Kind::Scene ? spec : nullptr;
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
    else if (const ItemSpec* spec = sceneSpecOf(item)) {
        auto prim = cached(spec->key(item), [&] { return spec->make(item); });
        std::string name = item.value("id", spec->name(item));
        if (spec->configure)
            spec->configure(prim, item, name);
        named[name] = prim;
        scalar alpha = item.value("alpha", 1.);
        auto poly = std::static_pointer_cast<PolyscopePrimitive>(prim);
        auto pis = item.contains("at") && item["at"].is_string()
            ? poly->at(item["at"].get<std::string>(), alpha)
            : poly->at(alpha);
        show.addToLastSlide(pis);
        used_primitives.insert(prim);
    }
    else if (item.contains("arrow")) {
        const json& spec = item["arrow"];
        if (!spec.is_object() || !spec.contains("from") || !spec.contains("to"))
            throw std::runtime_error("\"arrow\" item needs {from: ..., to: ...}");
        for (const auto& [key, val] : spec.items())
            if (!arrowFields().count(key))
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
        bool fly = item.value("fly", true);
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
