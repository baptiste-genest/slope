#ifndef DECKLOADER_H
#define DECKLOADER_H

#include "SlideManager.h"
#include "../content/PrimitiveGroup.h"
#include "../content/io.h"
#include <memory>

namespace slope {

class Slideshow;
class Box2D;
using Box2DPtr = std::shared_ptr<Box2D>;
class Stack2D;
using Stack2DPtr = std::shared_ptr<Stack2D>;

/*
 * DeckLoader builds slides from a hot-reloadable YAML manifest, so the
 * composition of a slideshow (which primitives, on which frame, at which
 * anchor) can be edited at runtime without recompiling. Behavior that needs
 * real code (updaters, computed meshes...) stays in C++ and is exposed to
 * the manifest through registerObject().
 *
 * The usual way to use it is the owned-slideshow mode, where a whole deck
 * main file reduces to :
 *
 *   DeckLoader deck;
 *   int main(int argc, char** argv) {
 *       deck.init("my_talk", "deck.yaml", argc, argv);
 *       deck.registerObject("thing", ...);   // C++-defined content
 *       deck.run();
 *   }
 *
 * The latex prefix and definitions files are chosen by the top-level
 * "commands" and "latex" keys, defaulting to commands.tex / latex.json from
 * the project directory when they exist. Alternatively,
 * build(show)/hotReload(show) drive an external Slideshow.
 *
 * Manifest format (deck.yaml):
 *
 * commands: my_commands.tex           # optional, latex prefix file
 * latex: my_definitions.json          # optional, latex source file
 * slides:
 *   - frame:
 *       - title: My title
 *       - load: key_in_latex_json       # content (and text/formula mode)
 *                                       # from latex.json, anchored at = key
 *       - latex: inline \LaTeX
 *         at: some_label
 *       - formula: e^{i\pi}+1=0
 *         at: [0.5, 0.4]
 *       - image: figure.png
 *         at: fig
 *         scale: 0.5
 *       - object: registered_name       # C++-defined content (or group)
 *       - mesh: bunny.obj               # loads an obj file; optional
 *         smooth: true                  # smooth (default true), normalize,
 *         at: transform_label           # persistent transform label
 *       - arrow: {from: some_item, to: other_item, bend: 0.3,
 *                 from_offset: [0.02, 0], to_offset: [0, -0.01]}
 *                                       # endpoints follow their target :
 *                                       # item name, [x,y], or a label;
 *                                       # offsets shift the attach points
 *       - box:                           # rectangle englobing its items,
 *           - latex: framed content      # following them live; optional
 *           - image: fig.png             # padding, color, thickness,
 *         padding: 0.02                  # filled, fill_color, id, alpha;
 *         padx: 0.05                     # padx/pady override one axis
 *       - stack:                         # children laid out below one
 *           - latex: first paragraph     # another, block centered on the
 *           - step                       # handle; layout reserves space
 *           - latex: appears later       # for children of later steps
 *         at: column_handle              # label handle (default : the id),
 *         spacing: 0.02                  # or [x,y] for a fixed block
 *         align: left                    # left | center | right
 *       - camera: view_name
 *         fly: true
 *       - pause: 3
 *       - keyframe: pipeline_done        # labels this frame : C++ updaters
 *                                        # branch on t.afterKeyframe("...")
 *                                        # instead of counting frames
 *       - latex: some content            # any item can join a tagged group;
 *         group: groupA                  # groups have no position, only
 *                                        # mapped operations (move, remove)
 *       - step                           # = inNextFrame : every item after
 *       - latex: appears_later           # it belongs to the next step
 *       - move: groupA                   # offsets every member of the group
 *         by: [0.1, -0.05]               # (or a single item by id), composed
 *                                        # on top of their own placement
 *       - set: some_id                   # re-places an existing item : new
 *         at: new_label                  # anchor (or below/above/...) from
 *                                        # this frame on, transition animated
 *       - remove: [key_in_latex_json, registered_name, groupA]
 *       - replace: fig
 *         with: {image: other.png}
 *   - frame:
 *       - ...
 *     same_title: true                   # keep previous frame's title
 *
 * Placement of a screen item is one of :
 *   at: label                # persistent, drag-editable LabelAnchor
 *   at: [x, y]               # fixed position
 *   at: TOP | CENTER | BOTTOM
 *   below/above/right_of/left_of: other_item   (optional padding: p)
 * When omitted, load/image items default to a label derived from their
 * key/filename, so everything is drag-editable out of the box.
 *
 * Items are referenced (by remove/replace/below/...) through their key
 * (latex key, image filename stem, object name, "title") or an explicit
 * "id: name" field. References resolve to the most recent item with that
 * name, in manifest order.
 *
 * Reordering steps desyncs C++ updaters that branch on
 * t.relative_frame_number : prefer marking the relevant frames with
 * "keyframe:" and branching on t.afterKeyframe / atKeyframe / beforeKeyframe,
 * which follow the manifest wherever the mark moves.
 */
class DeckLoader
{
public:
    using ObjectFactory = std::function<PrimitiveInSlide()>;
    using PrimitiveFactory = std::function<PrimitivePtr()>;
    using GroupFactory = std::function<PrimitiveGroup()>;

    void init(path deck_file);
    bool isInitialized() const {return initialized;}

    // owned-slideshow mode : initializes the slideshow (and the project's
    // latex.json / commands.tex when present), then parses the deck file
    void init(const std::string& project_name, path deck_file, int argc, char** argv);
    // builds the slides, installs the hot-reload watcher and runs the show
    void run();
    Slideshow& slideshow();

    DeckLoader();
    ~DeckLoader();

    void registerObject(const std::string& name, const ObjectFactory& factory);
    void registerObject(const std::string& name, const PrimitiveInSlide& pis);
    // for objects that don't need a state : registered with a default one
    void registerObject(const std::string& name, const PrimitiveFactory& factory);
    void registerObject(const std::string& name, PrimitivePtr ptr);
    void registerObject(const std::string& name, const GroupFactory& factory);
    void registerObject(const std::string& name, const PrimitiveGroup& group);

    // runs the manifest through the SlideManager composition API
    void build(SlideManager& show);

    // call once per frame (e.g. from Slideshow::onFrame) : when the manifest
    // changed on disk, recomposes the slideshow from it
    void hotReload(Slideshow& show);

    // throttled mtime check; true when the manifest changed on disk and
    // was successfully re-parsed
    bool sourceModified();

    // primitives placed by the manifest at the last build
    const std::set<PrimitivePtr>& usedPrimitives() const {return used_primitives;}

private:
    path source_path;
    json source;
    std::filesystem::file_time_type source_last_modified;
    bool initialized = false;

    // primitives are cached across rebuilds so a hot reload reuses textures,
    // compiled latex and polyscope structures instead of recreating them
    std::map<std::string, PrimitivePtr> primitive_cache;

    // cameras are cached with their view file's mtime, so an edited view
    // file drops its entry and triggers a recompose (hot reload)
    struct CameraEntry {
        CameraViewPtr cam;
        path file;
        std::filesystem::file_time_type last_modified;
    };
    std::map<std::string, CameraEntry> camera_cache;
    bool camerasModified();
    std::map<std::string, ObjectFactory> object_registry;
    std::map<std::string, GroupFactory> group_registry;
    std::map<std::string, PrimitiveInSlide> instantiated_objects;
    std::map<std::string, PrimitiveGroup> instantiated_groups;

    // name -> primitive references accumulated during build, in manifest order
    std::map<std::string, PrimitivePtr> named;

    // primitives used by the manifest at last build, to disable on rebuild
    std::set<PrimitivePtr> used_primitives;

    std::unique_ptr<Slideshow> owned_show;

    void parse();
    void loadLatexResources();
    void buildFrame(SlideManager& show, const json& items);
    void addItem(SlideManager& show, const json& item);
    void buildStackChildren(SlideManager& show, const Stack2DPtr& stack, const json& items);
    AnchorPtr makeHandleAnchor(const json& item);

    // creates (or retrieves from cache) the screen primitive described by a
    // title/latex/text/formula/image item, and its reference name
    std::pair<ScreenPrimitivePtr,std::string> makeScreenPrimitive(const json& item);

    // applies at/below/above/right_of/left_of placement and adds to the slide
    void placeScreenItem(SlideManager& show, ScreenPrimitivePtr prim,
                         const json& item, const std::string& default_label);

    PrimitivePtr resolve(const std::string& name) const;
    ScreenPrimitivePtr resolveScreen(const std::string& name) const;

    PrimitivePtr cached(const std::string& key, const std::function<PrimitivePtr()>& create);
};

}

#endif // DECKLOADER_H
