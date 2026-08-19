#include "DeckItem.h"

namespace slope {

// these items drive the slide (or need the loader's registries) rather than
// build a primitive from the manifest alone, so DeckLoader::addItem keeps
// their branch. Only their field list lives here, next to every other one, so
// it cannot drift from what the branch reads
std::vector<ItemSpec> customItemSpecs()
{
    auto custom = [](const char* type, std::set<std::string> fields) {
        ItemSpec spec;
        spec.type = type;
        spec.kind = ItemSpec::Kind::Custom;
        spec.fields = std::move(fields);
        return spec;
    };

    // in the order DeckLoader::addItem tests them
    return {
        custom("keyframe", {}),
        custom("remove",   {}),
        custom("set",      {"at","on","two_sided","alpha","rot","zoom",
                            "below","above","right_of","left_of","padding"}),
        custom("replace",  {"with"}),
        custom("object",   {"id","at","follow","offset","alpha","rot","zoom","view",
                            "group","uniforms","textures"}),
        custom("arrow",    {"id","alpha","group"}),
        custom("box",      {"id","alpha","padding","padx","pady","thickness","color",
                            "fill_color","filled","group"}),
        custom("stack",    {"id","at","spacing","align","group"}),
        custom("camera",   {"fly"}),
        custom("pause",    {}),
    };
}

// the "arrow" item carries its own fields inside its value, one level down
const std::set<std::string>& arrowFields()
{
    static const std::set<std::string> f =
        {"from","to","from_offset","to_offset","bend","thickness","color","head","margin"};
    return f;
}

}
