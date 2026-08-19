#ifndef DECKITEM_H
#define DECKITEM_H

#include "../../content/primitive.h"
#include "extern/json.hpp"
#include <functional>
#include <set>
#include <string>
#include <vector>

namespace slope {

/*
 * One record per deck item type, so adding a type is one entry in one file
 * instead of three edits spread over the loader.
 *
 * Screen and Scene items are built from the manifest alone : their factories
 * call the plain C++ API and never touch the loader, which is what keeps the
 * deck a thin layer over it (a primitive knows nothing about yaml). Items that
 * drive the slide rather than build a primitive (remove, set, box, stack...)
 * need the loader itself, so they keep their branch in DeckLoader::addItem and
 * register here only their field list, which is the one thing that must not
 * drift from the parsing code.
 */
struct ItemSpec {
    enum class Kind {
        Screen,   // a ScreenPrimitive, placed by placeScreenItem
        Scene,    // a PolyscopePrimitive, added at a transform label
        Custom,   // handled by DeckLoader::addItem, listed here for its fields
    };

    std::string type;             // the yaml key selecting this item
    Kind kind = Kind::Custom;

    // keys accepted besides "type" itself. Screen items get the shared
    // placement keys on top, so a new one cannot forget them
    std::set<std::string> fields;

    // content-addressed cache key, so editing an item makes a new primitive
    // and a hot reload reuses the untouched ones. The loader prefixes it with
    // the item's "id" for screen items
    std::function<std::string(const json&)> key;
    std::function<PrimitivePtr(const json&)> make;
    // re-applied to the cached primitive on every build, for the fields that
    // should not force a new one. It runs once the reference name is known,
    // which the shader's uniforms are named after
    std::function<void(const PrimitivePtr&, const json&, const std::string& name)> configure;
    // the name the manifest refers to this item by, before "id" overrides it
    std::function<std::string(const json&)> name;
};

// placement keys, shared by every screen item
const std::set<std::string>& placementFields();

// every known item type, in the order DeckLoader::addItem dispatches them
const std::vector<ItemSpec>& itemSpecs();
// the spec whose type key the item carries, or null
const ItemSpec* findItemSpec(const json& item);
// "title/load/latex/..." , for error messages
std::string screenItemTypes();

// warns about misspelled or misplaced fields, which yaml would otherwise
// silently ignore (the deck is hand-edited live, so mistakes must be loud)
void warnUnknownKeys(const json& item);

// the families, assembled by itemSpecs()
std::vector<ItemSpec> textItemSpecs();
std::vector<ItemSpec> mediaItemSpecs();
std::vector<ItemSpec> shaderItemSpecs();
std::vector<ItemSpec> sceneItemSpecs();
std::vector<ItemSpec> customItemSpecs();

// the "arrow" item carries its own fields inside its value, one level down
const std::set<std::string>& arrowFields();

}

#endif // DECKITEM_H
