#include "slides/deck/items/DeckItem.h"
#include <filesystem>
#include "content/config/Options.h"
#include "content/screen_primitives/text/LateX.h"
#include "content/screen_primitives/text/Code.h"

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

// the file and the slice make the listing, everything else is restyling
static std::string codeKey(const json& i)
{
    std::string k = "code:" + i["code"].get<std::string>();
    if (i.contains("lines"))
        k += ":" + i["lines"].dump();
    return k;
}

// without the dot, as CodeLanguage::ForExtension wants it
static std::string codeExtension(const json& i)
{
    auto e = std::filesystem::path(i["code"].get<std::string>()).extension().string();
    if (!e.empty() && e.front() == '.')
        e.erase(0, 1);
    return e;
}

static CodePtr makeCode(const json& i)
{
    const std::string file = i["code"].get<std::string>();
    if (i.contains("lines")) {
        const json& l = i["lines"];
        if (!l.is_array() || l.size() != 2 || !l[0].is_number_integer())
            throw std::runtime_error("\"lines\" must be [first, last], 1 based");
        return Code::FromFile(file, l[0].get<int>(), l[1].get<int>());
    }
    return Code::FromFile(file);
}

std::vector<ItemSpec> textItemSpecs()
{
    std::vector<ItemSpec> specs;

    specs.push_back({
        "code", ItemSpec::Kind::Screen,
        {"lines","language","font","line_numbers","font_scale","tracking",
         "line_spacing","padding","dim","reveal","focus"},
        codeKey,
        [](const json& i) -> PrimitivePtr { return makeCode(i); },
        [](const PrimitivePtr& p, const json& i, const std::string&) {
            auto c = std::static_pointer_cast<Code>(p);
            const CodeStyle d;   // cached primitive, so a dropped field reverts
            c->style.font         = i.contains("font")
                                  ? Code::LoadFont(i["font"].get<std::string>()) : d.font;
            c->setLanguage(i.contains("language")
                           ? CodeLanguage::ForName(i["language"].get<std::string>())
                           : CodeLanguage::ForExtension(codeExtension(i)));
            // line_numbers: true | false | absolute, the last one numbering a
            // slice by the file it came from
            c->style.line_numbers          = d.line_numbers;
            c->style.absolute_line_numbers = d.absolute_line_numbers;
            if (i.contains("line_numbers")) {
                const json& n = i["line_numbers"];
                if (n.is_boolean())
                    c->style.line_numbers = n.get<bool>();
                else if (n.is_string() && n.get<std::string>() == "absolute")
                    c->style.line_numbers = c->style.absolute_line_numbers = true;
                else if (n.is_string() && n.get<std::string>() == "relative")
                    c->style.line_numbers = true;
                else
                    throw std::runtime_error("\"line_numbers\" takes true, false, "
                                             "absolute or relative");
            }
            c->style.font_scale   = i.value("font_scale", d.font_scale);
            c->style.tracking     = i.value("tracking", d.tracking);
            c->style.line_spacing = i.value("line_spacing", d.line_spacing);
            c->style.padding      = i.value("padding", d.padding);
            c->style.dim_factor   = i.value("dim", d.dim_factor);
        },
        [](const json& i) {
            return std::filesystem::path(i["code"].get<std::string>()).stem().string();
        },
    });

    // scale shapes the compiled latex, so it goes in the cache key below
    auto titleScale = [](const json& i) {
        return i.value("scale", Options::TitleScale);
    };
    specs.push_back({
        "title", ItemSpec::Kind::Screen, {"scale"},
        [titleScale](const json& i) {
            return "title:" + i["title"].get<std::string>() + ":"
                 + std::to_string(titleScale(i));
        },
        [titleScale](const json& i) -> PrimitivePtr {
            return Title(i["title"].get<std::string>(), true, titleScale(i));
        },
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
