#include "slides/deck/items/DeckItem.h"
#include "spdlog/spdlog.h"
#include <map>

namespace slope {

const std::set<std::string>& placementFields()
{
    static const std::set<std::string> f =
        {"id","at","on","two_sided","follow","offset","alpha","rot","zoom",
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
        {"replace", "id"}, {"object", "C++ object name"}, {"arrow", "map, keys below"},
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
    for (const auto& spec : itemSpecs())
        if (item.contains(spec.type))
            return &spec;
    return nullptr;
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
    for (const auto& [key, val] : item.items())
        if (key != spec->type && !spec->fields.count(key))
            spdlog::warn("deck: ignored key \"{}\" on a \"{}\" item", key, spec->type);
}

}
