#ifndef DECKLOADER_H
#define DECKLOADER_H

#include "slides/core/SlideManager.h"
#include "content/core/PrimitiveGroup.h"
#include "content/config/io.h"
#include <memory>
#include "extern/json.hpp"

namespace slope {

class Slideshow;
class Box2D;
using Box2DPtr = std::shared_ptr<Box2D>;
class Stack2D;
using Stack2DPtr = std::shared_ptr<Stack2D>;
class Shader;
using ShaderPtr = std::shared_ptr<Shader>;
class PolyscopePrimitive;
using PolyscopePrimitivePtr = std::shared_ptr<PolyscopePrimitive>;

/*
 * DeckLoader builds slides from a YAML file that is reloaded when it changes.
 * The composition of a slideshow, which is the list of primitives, the frame of each one and its anchor,
 * can then be edited while the show runs, without compiling.
 * Behavior that needs real code, such as updaters and computed meshes, stays in C++
 * and is made available to the deck with registerObject().
 *
 * The simplest use is the owned-slideshow mode, where the whole main file of a deck is as follows.
 *
 *   DeckLoader deck;
 *   int main(int argc, char** argv) {
 *       deck.init("my_talk", "deck.yaml", argc, argv);
 *       deck.registerObject("thing", ...);   // C++-defined content
 *       deck.run();
 *   }
 *
 * The LaTeX prefix file and the definitions file are set by the top-level keys "commands" and "latex".
 * They default to commands.tex and latex.json in the project folder when these exist.
 * Instead, build(show) and hotReload(show) can drive a Slideshow that is owned elsewhere.
 *
 * Manifest format (deck.yaml).
 *
 * commands: my_commands.tex           # optional, latex prefix file
 * preamble: \usepackage{...}          # optional, inline latex prefix (string or
 *                                     # list), on top of commands.tex
 * latex: my_definitions.json          # optional, latex source file
 * config:                             # optional, hot-reloaded. Drop a key to
 *   title_scale: 1.8                   # reset that knob to its compiled default
 *   latex_scale: 1.0
 *   box_roundness: 0.5
 *   margin: 0.05                       # number or [x, y], for the edge anchors
 *   top:    [0.5, 0.08]                # moves the TOP / CENTER / BOTTOM points
 *   center: [0.5, 0.5]
 *   bottom: [0.5, 0.92]
 * slides:
 *   - frame:
 *       - title: My title
 *       - load: key_in_latex_json       # content (and text/formula mode)
 *                                       # from latex.json, anchored at = key
 *       - latex: inline \LaTeX
 *         at: some_label
 *         scale: 1.2                    # width is the wrapping width in pt,
 *         width: 300                    # where the lines break
 *       - formula: e^{i\pi}+1=0
 *         at: [0.5, 0.4]
 *       - image: figure.png
 *         at: fig
 *         scale: 0.5                    # default size, before any state scale
 *       - gif: loop.gif                 # every frame held as a texture
 *         fps: 10                       # scale and loop as for an image
 *       - video: clip.mp4               # streamed from disk, one ffmpeg pipe
 *         at: vid                       # decode_width caps the decode (the
 *         decode_width: 960             # window width by default), loop and
 *         autoplay: false               # autoplay default true, a click on
 *         speed: 1                      # the clip plays or pauses it
 *       - webcam: /dev/video0           # a live camera, opened when the slide
 *         at: cam                       # is reached; width, height, fps and
 *         width: 1280                   # input_format must be ones the device
 *         height: 720                   # offers, it is told and not probed
 *       - shader: plasma.frag           # a single-pass fragment shader,
 *         at: fx                        # hot-reloaded like the deck itself;
 *         resolution: [900, 600]        # multi-pass / SSBOs stay in C++
 *         uniforms:                     # each uniform is a persistent, live
 *           sun: [0.3, 0.9, 0.2]        # tunable parameter (see below)
 *           speed: {default: 1, max: 5}
 *         textures:                     # bound to the sampler of the same
 *           noise: noise.png            # name, declared by the shader
 *       - object: registered_name       # C++-defined content (or group)
 *         uniforms:                     # when it is a shader, the same
 *           knob: {default: 0.4}        # "uniforms"/"textures"/"view" as
 *         textures: {noise: noise.png}  # above. Its parameters are named
 *                                       # "<object>/<uniform>", and the binds
 *                                       # its C++ owner set are left alone
 *       - shader: lattice.frag          # "view" is half the height it shows,
 *         id: lat                       # a number or a snippet name. The
 *         view: {half: zoom}            # .frag reads it back as iWorld().
 *       - formula: \omega_1             # rides a moving point, shifted from
 *         follow: lat.z1                # it by "offset"
 *         offset: [0.02, -0.03]
 *       - mesh: bunny.obj               # loads an obj file; optional
 *         smooth: true                  # smooth (default true), normalize,
 *         at: transform_label           # persistent transform label
 *       - surface: wave                 # a mesh from a snippet function of the
 *         u: [0, 6.2832]                # parameter square, vec2 -> vec3,
 *         v: [0, 6.2832]                # re-evaluated every frame. u/v are
 *         resolution: [96, 48]          # the domain (default [0,1]), and the
 *         closed: [true, true]          # resolution its grid, one number for
 *         at: transform_label           # a square one. "closed" welds a
 *                                       # periodic seam
 *       - curve: helix                  # a curve network from a snippet
 *         u: [0, 12.566]                # function of one number, scalar ->
 *         resolution: 400               # vec3, sampled over the segment "u"
 *         closed: false                 # (default [0,1]). "closed" joins the
 *         radius: 0.01                  # last node back to the first
 *       - arrow: {from: some_item, to: other_item, bend: 0.3,
 *                 from_offset: [0.02, 0], to_offset: [0, -0.01]}
 *                                       # endpoints follow their target,
 *                                       # item name, [x,y], or a label;
 *                                       # offsets shift the attach points
 *       - box:                           # rectangle englobing its items,
 *           - latex: framed content      # following them live; optional
 *           - image: fig.png             # padding, color, thickness,
 *         padding: 0.02                  # filled, fill_color, id, alpha;
 *         padx: 0.05                     # padx/pady override one axis,
 *         pad_left: 0.05                 # pad_left/right/top/bot override
 *         pad_top: 0.03                  # one side; padding stays the
 *                                        # default for any side left unset
 *       - stack:                         # children laid out below one
 *           - latex: first paragraph     # another, block centered on the
 *           - step                       # handle; layout reserves space
 *           - latex: appears later       # for children of later steps
 *         at: column_handle              # label handle, the id by default,
 *         spacing: 0.02                  # or [x,y] for a fixed block
 *         align: left                    # left | center | right
 *       - camera: view_name              # cuts to the view,
 *         fly: true                      # fly: true to glide there
 *       - pause: 3
 *       - keyframe: pipeline_done        # labels this frame, C++ updaters
 *                                        # branch on t.afterKeyframe("...")
 *                                        # instead of counting frames
 *       - latex: some content            # any item can join a tagged group;
 *         group: groupA                  # groups have no position, operations
 *                                        # (remove...) map over their members
 *       - step                           # = inNextFrame, every item after
 *       - latex: appears_later           # it belongs to the next step
 *       - set: some_id                   # re-places or restyles an existing
 *         at: new_label                  # item from this frame on, animated
 *         alpha: 0.3                     # by the transition. Any of the
 *         rot: -20                       # state fields below, and without a
 *         zoom: 1.5                      # placement it stays where it is
 *       - latex: any item                # alpha, rot (degrees) and zoom apply
 *         alpha: 0.5                     # to any screen item, they live in
 *         rot: 20                        # the slide state so they animate
 *         zoom: 1.5                      # between steps. zoom multiplies the
 *                                        # wheel-set anchor scale
 *       - latex: in front                # depth (integer, 0 by default) sets
 *         depth: 5                       # the draw order, higher is in front,
 *                                        # ties follow the order in the yaml
 *       - remove: [key_in_latex_json, registered_name, groupA]
 *       - replace: fig
 *         with: {image: other.png}
 *   - frame:
 *       - ...
 *     same_title: true                   # keep previous frame's title
 *     no_template: true                  # and skip the deck template
 *
 * A "template:" list next to "slides:" is added to the first step of every frame, behind its own items.
 * It is built once and the same primitives are reused, so a footer or a logo stays in place
 * during a slide change and does not fade out and in.
 * It cannot contain "step". A frame can leave it out with no_template.
 *
 *   template:
 *     - latex: \color{gray} my talk
 *       at: footer
 *
 * Groups
 * ------
 * Any other top-level list is a group, which a frame uses by its bare name.
 * A map with "params:" and "items:" gives the group arguments.
 *
 *   caption:                    # "- caption" puts both here
 *     - latex: parameter square
 *       at: caption_pos
 *
 *   figure:
 *     params: {yrange: [0, 1], ylabel: value}   # a param left empty is required
 *     items:
 *       - board: $id            # the whole value, its type is kept
 *         at: figure
 *         yrange: $yrange
 *       - latex: ${ylabel} (log)   # inside a string
 *         at: ylabel
 *
 *   - figure: conv              # in a frame, the value after the name is
 *     ylabel: residual          # the id, and the other keys are the args
 *
 * Only the declared params and "id" are substituted, so the "$" of LaTeX is left alone.
 * A "$name" that names no param gives a warning.
 * A key left empty in the call keeps its default.
 * What the call places is tagged with the group name and the id, so "remove: conv" removes the instance.
 * The target of a "set" inside the group is not tagged.
 * Ids inside the body are not prefixed, so write "id: ${id}_legend" when two instances share a slide.
 * A group is built again at each use, so every use is its own instance.
 * Give the body "id: ${id}_..." to keep one instance across frames.
 * Group and param names cannot be item types, and a param cannot have the name of a group.
 * A stack cannot call a group.
 *
 * A group can hold "- step". Its items after it go to the next step, and so do the items of the frame after the call.
 * The template cannot use such a group.
 *
 * Placement of a screen item is one of
 *   at: label                # persistent, drag-editable LabelAnchor
 *   at: [x, y]               # fixed position
 *   at: TOP | CENTER | BOTTOM
 *   at: TOP_LEFT | TOP_RIGHT | BOTTOM_LEFT | BOTTOM_RIGHT | LEFT | RIGHT
 *                            # flush to that screen edge/corner, size-aware,
 *                            # "config: margin" away
 *   below/above/right_of/left_of: other_item   (optional padding: p)
 * Any of them accepts "offset: [x, y]", a shift in screen units from that placement,
 * so two items can share a label without overlapping.
 * When the placement is left out, load and image items use a label made from their key or file name,
 * so everything can be dragged with no setup.
 *
 * "follow:" places an item on a moving point instead of a fixed one, shifted by "offset: [x, y]".
 * The point is a snippet variable or a parameter, which share one namespace.
 * Its number of components decides the space it lives in.
 *
 *   follow: apex          3 numbers, a world position in the 3D scene
 *   follow: cursor        2 numbers, a screen position in [0,1]^2
 *   follow: lat.z1        2 numbers read in shader "lat"'s world space
 *
 * The space is never guessed. A shader is used only when its item is named.
 *
 * Shader uniforms
 * ---------------
 * "uniforms:" on a shader item declares each uniform as a tunable parameter (Params).
 * It appears in the Tuner panel while the shader is on screen and can be dragged live.
 * Ctrl+S saves it to views/params.json and it is loaded again at the next run.
 * The shader follows it at every frame with no C++.
 *
 *   - shader: sky.frag
 *     uniforms:
 *       sun:   [0.3, 0.9, 0.2]                  # vec3  \  type read off
 *       tint:  "#ffcc88"                        # color  ) the literal
 *       steps: 64                               # int   /
 *       speed: {default: 1.0, min: 0, max: 5}   # bounded, so a slider
 *       mode:  {type: int, default: 0}          # explicit type
 *       grab:  {type: vec3, visible: handle}    # shown without the panel
 *
 * The types are float, int, bool, vec2, vec3 and color (vec4).
 * Bounds are optional. A parameter without bounds is dragged and not slid.
 * Bounds apply to every component of a vector at once.
 * "visible" shows a parameter on screen while the slide that reads it is up, with no panel open.
 * "handle" is its manipulator, a gizmo for a vec3 and a screen handle for a vec2.
 * "panel" is its widget in a small window, and "both" is the two of them.
 * Parameters are named "<item>/<uniform>", so two uses of the same .frag with different ids are tuned separately.
 * A uniform that is absent from the compiled shader is ignored, like in Shader::bind,
 * so an unfinished .frag never breaks the deck.
 *
 * Shader textures
 * ---------------
 * "textures:" binds an image file to the sampler with the same name, which the shader declares itself.
 *
 *   uniform sampler2D noise;        // in the .frag
 *   uniform vec2      noise_size;   // optional, its size in pixels
 *
 *   - shader: sky.frag
 *     textures:
 *       noise: noise.png
 *       grad:  {file: gradient.png, filter: nearest, wrap: repeat}
 *
 * filter is nearest or linear (default linear), and wrap is clamp or repeat (default clamp).
 * Declaring the same file again costs nothing, so a hot reload does not read the image again.
 * Removing an entry unbinds it.
 * Only image files work here. A texture fed by another pass needs an order that a deck cannot express,
 * so it stays on the C++ side with Shader::setTexture.
 *
 * Snippet objects
 * ---------------
 * "surface:" and "curve:" name a callable snippet section and sample it over a parameter domain.
 * The domain is a vec2 for a surface and one number for a curve.
 *
 *   --- wave                            # in snippets.lua
 *   return function(uv)
 *     return vec3(uv.x, uv.y, 0.2*math.sin(6*uv.x + t.from_begin))
 *   end
 *
 *   --- helix
 *   return function(s)
 *     return vec3(math.cos(s + t.from_begin), math.sin(s + t.from_begin), 0.15*s)
 *   end
 *
 * The section sees `t`, so the object is animated without any C++, and it is reloaded with the file.
 * "resolution" is the number of samples of the domain, and editing it in the deck rebuilds the object live.
 * Each sample calls Lua, which is slow. See examples/snippet_perf for the cost of each size.
 *
 * Other items refer to an item (in remove, replace, below and others) by its key,
 * which is the LaTeX key, the stem of the image file name, the object name or "title",
 * or by an explicit "id: name" field.
 * A reference resolves to the most recent item with that name, in the order of the deck.
 * An "id:" names a primitive like a C++ variable does, and every declaration with that id is the same primitive.
 * Without an "id:", each declaration builds its own primitive.
 * Writing the same content twice then makes two primitives, each one appearing and disappearing on its own.
 * To keep one primitive across frames, name it and place it again with "set:".
 * Use "same_title:" for a title and "template:" for what every frame carries.
 *
 * Reordering steps breaks the C++ updaters that use t.relative_frame_number.
 * Prefer marking the frames with "keyframe:" and testing t.afterKeyframe, atKeyframe or beforeKeyframe,
 * which follow the deck wherever the mark moves.
 */
class DeckLoader {
public:
    using ObjectFactory = std::function<PrimitiveInSlide()>;
    using PrimitiveFactory = std::function<PrimitivePtr()>;
    using GroupFactory = std::function<PrimitiveGroup()>;

    // Reads the deck file, for use with a Slideshow that is owned elsewhere.
    void init(path deck_file);
    // True once a deck file was read.
    bool isInitialized() const { return initialized; }

    // Owned-slideshow mode. It creates the slideshow and the LaTeX resources of the project when they exist,
    // then reads the deck file.
    void init(const std::string& project_name, path deck_file, int argc, char** argv);
    // Builds the slides, installs the watcher that reloads the deck, and runs the show.
    void run();
    // The slideshow that the loader owns.
    Slideshow& slideshow();

    DeckLoader();
    ~DeckLoader();

    // Makes C++ content available to the deck under a name, used by "object:".
    // The content is a function that builds a primitive with its state, a primitive with its state,
    // or a group of primitives.
    void registerObject(const std::string& name, const ObjectFactory& factory);
    void registerObject(const std::string& name, const PrimitiveInSlide& pis);
    // Same for objects that need no state. They get a default one.
    void registerObject(const std::string& name, const PrimitiveFactory& factory);
    void registerObject(const std::string& name, PrimitivePtr ptr);
    void registerObject(const std::string& name, const GroupFactory& factory);
    void registerObject(const std::string& name, const PrimitiveGroup& group);

    // Makes a position available to "follow" under a name.
    // "follow" finds shader points and scene points by itself, so use this only for a position computed in another way.
    // It takes priority over the "<item>.<snippet>" form.
    // The placer returns window coordinates, with y down.
    //   deck.registerPlacer("cursor", []{ return myTrackedThing(); });
    void registerPlacer(const std::string& name, const std::function<vec2()>& placer);

    // Builds the slides of the deck in a SlideManager.
    void build(SlideManager& show);

    // Composes the slideshow again when the deck changed. Call it once per frame.
    void hotReload(Slideshow& show);

    // Checks the modification time of the deck, not at every call.
    // Returns true when the deck changed on disk and was parsed again without error.
    bool sourceModified();

private:
    // The first build places every item, so only what follows it is new.
    bool first_build_done = false;

public:
    // Primitives placed by the deck at the last build.
    const std::set<PrimitivePtr>& usedPrimitives() const { return used_primitives; }

private:
    path source_path;
    json source;
    // Last source that built without error, used when an edited source fails to build.
    json last_good_source;
    std::filesystem::file_time_type source_last_modified;
    // True when the file on disk cannot be parsed. The last good source is still used.
    bool source_unparsed = false;
    // Last LatexLoader::generation that this deck was built with.
    int latex_generation = 0;
    // The macro file that this deck put in the LaTeX prefix.
    std::string commands_file;
    bool initialized = false;

    // Primitives are kept between builds, so a hot reload reuses textures, compiled LaTeX and polyscope structures
    // and does not create them again.
    std::map<std::string, PrimitivePtr> primitive_cache;

    // Cameras are kept with the modification time of their view file.
    // An edited view file removes its entry and triggers a new composition.
    struct CameraEntry {
        CameraViewPtr cam;
        path file;
        std::filesystem::file_time_type last_modified;
    };
    std::map<std::string, CameraEntry> camera_cache;
    // True when a view file changed.
    bool camerasModified();
    std::map<std::string, ObjectFactory> object_registry;
    std::map<std::string, GroupFactory> group_registry;
    std::map<std::string, PrimitiveInSlide> instantiated_objects;
    // Named live positions, used by "follow:".
    std::map<std::string, std::function<vec2()>> placer_registry;
    std::map<std::string, PrimitiveGroup> instantiated_groups;

    // Primitive of each name, collected during the build in the order of the deck.
    std::map<std::string, PrimitivePtr> named;
    // Deck line of every item of the parsed source, found by address, used in warnings and errors.
    std::unordered_map<const json*, int> line_of;
    int lineOf(const json& item) const {
        auto it = line_of.find(&item);
        return it == line_of.end() ? 0 : it->second;
    }
    // Builds the slides.
    void buildImpl(SlideManager& show);
    // Ids already reported as clashing, so each is reported once.
    std::set<std::string> warned_names;
    // Records an item under its id, with a warning when the id already means something else.
    void nameItem(const std::string& name, const PrimitivePtr& prim, bool explicit_id);

    // Primitives used by the deck at the last build, which are disabled at a rebuild.
    std::set<PrimitivePtr> used_primitives;

    // Primitives placed by the step being built, used to detect an "id:" placed twice.
    std::set<PrimitivePtr> step_primitives;

    // Number of times each content was declared, so a rebuild finds its primitives.
    std::map<std::string, int> occurrences;

    std::unique_ptr<Slideshow> owned_show;

    // Reads the deck file.
    void parse();
    // Applies the top-level "config:" and "preamble:", once per build.
    void applyDeckConfig();
    // Loads the LaTeX prefix file and the definitions file.
    void loadLatexResources();
    // Builds one frame from its items.
    void buildFrame(SlideManager& show, const json& items);

    // Top-level groups, expanded where "- name" or "- name: value" appears.
    struct DeckGroup {
        json items;
        // Default of each param, or null when the param is required.
        json params = json::object();
    };
    std::map<std::string, DeckGroup> deck_groups;
    // Chain of group calls, used to detect a group that calls itself.
    std::vector<std::string> expanding;
    // Registers a top-level group.
    void declareGroup(const std::string& name, const json& val);
    // Name of the group that an item calls, or null for a plain item.
    const std::string* groupCallOf(const json& item) const;
    // Expands a group call and returns what it placed, for its tags.
    std::set<PrimitivePtr> expandGroup(SlideManager& show, const std::string& name,
                                       const json& call);

    // Every primitive that build() places goes into used_primitives and into any collector that is open.
    std::vector<std::set<PrimitivePtr>*> collectors;
    // Records a primitive as used by the deck.
    void markUsed(const PrimitivePtr& ptr);
    // Runs a function and returns what it placed, including a primitive reused from an earlier slide.
    std::set<PrimitivePtr> collect(const std::function<void()>& run);
    // Builds one item and adds it to the slide.
    void addItem(SlideManager& show, const json& item);
    // Builds the children of a stack.
    void buildStackChildren(SlideManager& show, const Stack2DPtr& stack, const json& items);
    // Builds the anchor of the handle of a stack.
    AnchorPtr makeHandleAnchor(const json& item);

    // Creates the screen primitive of a title, latex, text, formula or image item, or takes it from the cache.
    // Returns it with its reference name.
    std::pair<ScreenPrimitivePtr, std::string> makeScreenPrimitive(const json& item);

    // Handles "uniforms" and "textures" on an "object:" that names a shader.
    // The declarations are in deck/items/ShaderItem.h, and this function only remembers what it declared.
    void declareObjectShaderInputs(const ShaderPtr& shader, const std::string& object,
                                   const json& item);
    // What the last build declared on a shader registered from C++.
    // A reload removes exactly that and keeps the binds of the owner.
    std::map<std::string, std::pair<ShaderPtr, std::vector<std::string>>> object_uniforms;
    // Turns a "follow" spec into a function that gives a screen position.
    std::function<vec2()> resolveFollow(const std::string& spec);
    // Same, and it also accepts {object: name, vertex: i}.
    std::function<vec2()> resolveFollow(const json& spec);
    // The scene primitive that a name places now, or null.
    PolyscopePrimitivePtr findSceneObject(const std::string& name) const;
    // True when the name is a registered object or group.
    bool knowsObject(const std::string& name) const;

    // Applies the placement (at, below, above, right_of, left_of) and adds the item to the slide.
    // With keep_placement, an item without placement stays where it is.
    void placeScreenItem(SlideManager& show, ScreenPrimitivePtr prim,
                         const json& item, const std::string& default_label,
                         bool keep_placement = false, const StateInSlide* own = nullptr);

    // Primitive that a name refers to, and the same as a screen primitive.
    PrimitivePtr resolve(const std::string& name) const;
    ScreenPrimitivePtr resolveScreen(const std::string& name) const;

    // Takes a primitive from the cache, or creates it.
    PrimitivePtr cached(const std::string& key, const std::function<PrimitivePtr()>& create);

    // Removes from every cache the primitives that a rebuild stopped using.
    void forgetPrimitives(const std::set<PrimitivePtr>& gone);

    // Every item goes through this function, which decides whether it is a new primitive or one that already exists.
    PrimitivePtr cachedItem(const json& item, const std::string& key,
                            const std::function<PrimitivePtr()>& create);
};

} // namespace slope

#endif // DECKLOADER_H
