#ifndef DECKITEM_H
#define DECKITEM_H

#include "content/core/primitive.h"
#include "extern/json.hpp"
#include "spdlog/spdlog.h"
#include <functional>
#include <set>
#include <string>
#include <vector>

namespace slope {

/*
 * One record per type of deck item, so adding a type is one entry in one file
 * and not three edits spread over the loader.
 *
 * Screen and Scene items are built from the deck alone. Their factories call the plain C++ API
 * and never use the loader, so the deck stays a thin layer over the API and a primitive knows nothing about yaml.
 * Items that drive the slide and do not build a primitive, such as remove, set, box and stack,
 * need the loader itself. They keep their branch in DeckLoader::addItem and register here only their list of fields,
 * which must always agree with the parsing code.
 */
struct ItemSpec {
    enum class Kind {
        // A ScreenPrimitive, placed by placeScreenItem.
        Screen,
        // A PolyscopePrimitive, added at a transform label.
        Scene,
        // Handled by DeckLoader::addItem. It is listed here for its fields.
        Custom,
    };

    // The yaml key that selects this item.
    std::string type;
    Kind kind = Kind::Custom;

    // Keys accepted besides the type itself. Screen items also get the shared placement keys,
    // so a new item cannot forget them.
    std::set<std::string> fields;

    // Cache key computed from the content. Editing an item gives a new primitive,
    // and a hot reload reuses the ones that did not change.
    // For screen items the loader adds the "id" of the item in front of it.
    std::function<std::string(const json&)> key;
    // Builds the primitive.
    std::function<PrimitivePtr(const json&)> make;
    // Applied again to the cached primitive at every build, for the fields that should not force a new primitive.
    // It runs once the reference name is known, because the uniforms of a shader are named after it.
    std::function<void(const PrimitivePtr&, const json&, const std::string& name)> configure;
    // Name that the deck uses for this item, before "id" replaces it.
    std::function<std::string(const json&)> name;
};

// Placement keys, shared by every screen item.
const std::set<std::string>& placementFields();

// A deck key and a description of what it takes, in the order shown to the author.
using KeyDoc = std::vector<std::pair<std::string, std::string>>;

// Top-level keys of the deck. Any other top-level list is a named group.
const KeyDoc& deckTopLevelKeys();
// Keys of a "- frame:" entry in "slides".
const KeyDoc& frameKeys();
// Settings read from the top-level "config:" map.
const KeyDoc& deckConfigKeys();
// Keys that several scene items share.
const KeyDoc& sceneKeys();
// Description of what the key of an item type takes, for example "tex formula" for "formula:". Empty if unknown.
std::string itemValueHint(const std::string& type);

// Every known item type, in the order used by DeckLoader::addItem.
const std::vector<ItemSpec>& itemSpecs();
// The spec whose type key the item has, or null.
const ItemSpec* findItemSpec(const json& item);
// Names of the screen item types, such as "title/load/latex/...", for error messages.
std::string screenItemTypes();

// Warns about misspelled or misplaced fields, which yaml would otherwise ignore without a message.
// The deck is edited by hand while the show runs, so mistakes must be visible.
void warnUnknownKeys(const json& item);

// Item types of the plot family.
std::vector<ItemSpec> plotItemSpecs();

// The other families, assembled by itemSpecs().
std::vector<ItemSpec> textItemSpecs();
std::vector<ItemSpec> mediaItemSpecs();
std::vector<ItemSpec> shaderItemSpecs();
std::vector<ItemSpec> sceneItemSpecs();
std::vector<ItemSpec> customItemSpecs();

// Fields of the "arrow" item, which are written inside its value, one level down.
const std::set<std::string>& arrowFields();

// Line of the deck for the item being built, or 0 when unknown. An item created by a group call has none.
// Warnings write it with deckWhere(), which gives " (line 12)" or nothing.
int& deckLine();
std::string deckWhere();

// Sets the current line for its scope. When an exception leaves the scope,
// the innermost line is kept in deckErrorLine(), so the loader can tell where the deck failed.
class DeckLineScope {
    int prev, thrown;

public:
    // Sets the current line for the lifetime of the object.
    explicit DeckLineScope(int line);
    ~DeckLineScope();
};
int& deckErrorLine();

// Logs a warning about a deck problem, with the line of the item when it is known.
template <class... A>
void deckWarn(fmt::format_string<A...> f, A&&... a) {
    spdlog::warn("deck{}: {}", deckWhere(), fmt::format(f, std::forward<A>(a)...));
}

} // namespace slope

#endif // DECKITEM_H
