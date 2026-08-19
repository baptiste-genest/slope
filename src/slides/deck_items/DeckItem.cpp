#include "DeckItem.h"
#include "spdlog/spdlog.h"

namespace slope {

const std::set<std::string>& placementFields()
{
    static const std::set<std::string> f =
        {"id","at","on","two_sided","follow","offset","alpha","rot","zoom",
         "below","above","right_of","left_of","padding","group"};
    return f;
}

const std::vector<ItemSpec>& itemSpecs()
{
    // built in the order DeckLoader::addItem dispatches : the items that drive
    // the slide first, then the scene, and the screen items last, which is
    // also the loader's fallback branch
    static const std::vector<ItemSpec> specs = [] {
        std::vector<ItemSpec> all;
        for (auto* family : {customItemSpecs, sceneItemSpecs,
                             textItemSpecs, mediaItemSpecs, shaderItemSpecs})
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
