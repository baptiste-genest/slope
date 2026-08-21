#include "slides/deck/items/DeckItem.h"
#include "content/config/Options.h"
#include "content/screen_primitives/text/LateX.h"

namespace slope {

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

// scale and width shape the compiled latex, so both belong in the cache key
static std::string latexKey(const char* type, const json& item)
{
    return std::string(type) + ":" + item[type].get<std::string>() + ":"
         + std::to_string(item.value("scale", Options::DefaultLatexScale)) + ":"
         + std::to_string(item.value("width", -1));
}

std::vector<ItemSpec> textItemSpecs()
{
    std::vector<ItemSpec> specs;

    specs.push_back({
        "title", ItemSpec::Kind::Screen, {},
        [](const json& i) { return "title:" + i["title"].get<std::string>(); },
        [](const json& i) -> PrimitivePtr { return Title(i["title"].get<std::string>()); },
        nullptr,
        [](const json& i) { return titleLabel(i["title"].get<std::string>()); },
    });

    // content (and text/formula mode) from latex.json, anchored at its key
    specs.push_back({
        "load", ItemSpec::Kind::Screen, {},
        [](const json& i) { return "load:" + i["load"].get<std::string>(); },
        [](const json& i) -> PrimitivePtr { return LatexLoader::Load(i["load"].get<std::string>()); },
        nullptr,
        [](const json& i) { return i["load"].get<std::string>(); },
    });

    specs.push_back({
        "latex", ItemSpec::Kind::Screen, {"scale","width"},
        [](const json& i) { return latexKey("latex", i); },
        [](const json& i) -> PrimitivePtr {
            return Latex::Add(i["latex"].get<std::string>(),
                              i.value("scale", Options::DefaultLatexScale),
                              i.value("width", -1));
        },
        nullptr,
        [](const json&) { return std::string(); },
    });

    specs.push_back({
        "formula", ItemSpec::Kind::Screen, {"scale","width"},
        [](const json& i) { return latexKey("formula", i); },
        [](const json& i) -> PrimitivePtr {
            return Formula::Add(i["formula"].get<std::string>(),
                                i.value("scale", Options::DefaultLatexScale),
                                i.value("width", -1));
        },
        nullptr,
        [](const json&) { return std::string(); },
    });

    return specs;
}

}
