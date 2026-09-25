#include "slides/deck/items/DeckItem.h"
#include <algorithm>
#include "spdlog/spdlog.h"
#include <exception>
#include <map>

namespace slope {

int& deckLine() { static int line = 0; return line; }
int& deckErrorLine() { static int line = 0; return line; }

std::string deckWhere()
{
    return deckLine() > 0 ? " (line " + std::to_string(deckLine()) + ")" : "";
}

DeckLineScope::DeckLineScope(int line) : prev(deckLine()), thrown(std::uncaught_exceptions())
{
    if (line > 0)
        deckLine() = line;
}

DeckLineScope::~DeckLineScope()
{
    if (std::uncaught_exceptions() > thrown && deckErrorLine() == 0)
        deckErrorLine() = deckLine();
    deckLine() = prev;
}

const std::set<std::string>& placementFields()
{
    static const std::set<std::string> f =
        {"id","at","on","two_sided","follow","offset","alpha","rot","zoom","depth",
         "below","above","right_of","left_of","padding","group"};
    return f;
}

const KeyDoc& deckTopLevelKeys()
{
    static const KeyDoc k = {
        {"slides",   "the frames, in order"},
        {"template", "items put on every frame"},
        {"config",   "layout settings"},
        {"snippets", "Lua file, or a list of them"},
        {"preamble", "latex lines put before every latex and formula"},
        {"commands", "tex file of macros, commands.tex by default"},
        {"latex",    "json of latex for load:, latex.json by default"},
    };
    return k;
}

const KeyDoc& frameKeys()
{
    static const KeyDoc k = {
        {"frame",       "the items of one slide"},
        {"same_title",  "true keeps the previous title"},
        {"no_template", "true leaves the template out"},
    };
    return k;
}

const KeyDoc& deckConfigKeys()
{
    static const KeyDoc k = {
        {"title_scale",   "title size, 1.5 by default"},
        {"latex_scale",   "text size, 1 by default"},
        {"box_roundness", "box corners, 1 by default"},
        {"margin",        "gap kept by TOP_LEFT and the other corners, a number or [x, y]"},
        {"top",           "[x, y] of the TOP label, 0 to 1"},
        {"center",        "[x, y] of the CENTER label"},
        {"bottom",        "[x, y] of the BOTTOM label"},
    };
    return k;
}

const KeyDoc& sceneKeys()
{
    static const KeyDoc k = {
        {"transform",  "map of the keys below, or a param or snippet name"},
        {"  pos",      "[x, y, z], or name"},
        {"  scale",    "a number, or [x, y, z], or name"},
        {"  axis",     "[x, y, z] or name, rotation axis, z by default"},
        {"  angle",    "a number, or name, in degrees"},
        {"color",      "[r, g, b], \"#rrggbb\", or name"},
        {"normalize",  "scale and reduce mesh or cloud"},
};
    return k;
}

std::string itemValueHint(const std::string& type)
{
    static const std::map<std::string, std::string> hints = {
        {"title", "text"}, {"latex", "latex text"}, {"formula", "tex formula"},
        {"load", "latex.json key"}, {"code", "source file"}, {"algo", "algorithm file"},
        {"image", "image file"}, {"gif", "gif file"}, {"video", "video file"},
        {"webcam", "device"}, {"shader", "fragment shader file"},
        {"board", "board name"}, {"plot", "curve name"}, {"scatter", "cloud name"},
        {"legend", "board name"},
        {"mesh", "mesh file"}, {"surface", "snippet function"}, {"curve", "snippet function"},
        {"point", "snippet or [x, y, z]"}, {"cloud", "point cloud file, .ply or .obj"},
        {"keyframe", "keyframe name"}, {"remove", "id or [ids]"}, {"set", "id"},
        {"replace", "id, with: id or an item"}, {"object", "C++ object name"}, {"arrow", "map, keys below"},
        {"box", "[items]"}, {"stack", "[items]"}, {"camera", "camera view name"},
        {"pause", "seconds"},
    };
    auto it = hints.find(type);
    return it == hints.end() ? std::string() : it->second;
}

const std::vector<ItemSpec>& itemSpecs()
{
    // built in the order DeckLoader::addItem dispatches : the items that drive
    // the slide first, then the scene, and the screen items last, which is
    // also the loader's fallback branch
    static const std::vector<ItemSpec> specs = [] {
        std::vector<ItemSpec> all;
        for (auto* family : {customItemSpecs, sceneItemSpecs,
                             textItemSpecs, mediaItemSpecs, shaderItemSpecs,
                             plotItemSpecs})
            for (auto& spec : family()) {
                // screen items share the placement keys rather than repeat them
                if (spec.kind == ItemSpec::Kind::Screen)
                    spec.fields.insert(placementFields().begin(), placementFields().end());
                all.push_back(std::move(spec));
            }
        return all;
    }();
    return specs;
}

const ItemSpec* findItemSpec(const json& item)
{
    std::vector<const ItemSpec*> found;
    for (const auto& spec : itemSpecs())
        if (item.contains(spec.type))
            found.push_back(&spec);
    if (found.size() <= 1)
        return found.empty() ? nullptr : found.front();
    // a type key that another match takes as a field is that field, like "board" on a plot
    std::vector<const ItemSpec*> kept;
    for (const auto* s : found)
        if (std::none_of(found.begin(), found.end(),
                         [&](const ItemSpec* o) { return o != s && o->fields.count(s->type); }))
            kept.push_back(s);
    if (kept.size() == 1)
        return kept.front();
    std::string types;
    for (const auto* s : found)
        types += (types.empty() ? "" : ", ") + s->type;
    throw std::runtime_error("an item cannot be several things at once (" + types + "), keep one");
}

std::string screenItemTypes()
{
    std::string list;
    for (const auto& spec : itemSpecs())
        if (spec.kind == ItemSpec::Kind::Screen)
            list += (list.empty() ? "" : "/") + spec.type;
    return list;
}

void warnUnknownKeys(const json& item)
{
    const ItemSpec* spec = findItemSpec(item);
    if (!spec)
        return;
    // "arrow: id" or a bare "arrow:" takes its fields beside it, "arrow: {...}" inside
    const bool flat_arrow = spec->type == "arrow" && !item["arrow"].is_object();
    for (const auto& [key, val] : item.items())
        if (key != spec->type && !spec->fields.count(key) && !(flat_arrow && arrowFields().count(key)))
            deckWarn("ignored key \"{}\" on a \"{}\" item", key, spec->type);
}

}
