#ifndef DECKLOADER_H
#define DECKLOADER_H

#include "SlideManager.h"
#include "../content/PrimitiveGroup.h"
#include "../content/io.h"
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
 *       - arrow: {from: some_item, to: other_item, bend: 0.3,
 *                 from_offset: [0.02, 0], to_offset: [0, -0.01]}
 *                                       # endpoints follow their target,
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
 *         at: column_handle              # label handle, the id by default,
 *         spacing: 0.02                  # or [x,y] for a fixed block
 *         align: left                    # left | center | right
 *       - camera: view_name
 *         fly: true
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
 *       - remove: [key_in_latex_json, registered_name, groupA]
 *       - replace: fig
 *         with: {image: other.png}
 *   - frame:
 *       - ...
 *     same_title: true                   # keep previous frame's title
 *
 * Placement of a screen item is one of
 *   at: label                # persistent, drag-editable LabelAnchor
 *   at: [x, y]               # fixed position
 *   at: TOP | CENTER | BOTTOM
 *   below/above/right_of/left_of: other_item   (optional padding: p)
 * When omitted, load/image items default to a label derived from their
 * key/filename, so everything is drag-editable out of the box.
 *
 * "follow:" places an item on a moving point instead of a fixed one, shifted
 * by "offset: [x, y]". The point is a snippet variable or a parameter, which
 * share one namespace, and how wide it is picks the space it lives in.
 *
 *   follow: apex          3 numbers, a world position in the 3D scene
 *   follow: cursor        2 numbers, a screen position in [0,1]^2
 *   follow: lat.z1        2 numbers read in shader "lat"'s world space
 *
 * The space is never inferred. A shader is used only when its item is named.
 *
 * Shader uniforms
 * ---------------
 * "uniforms:" on a shader item declares each uniform as a persistent tunable
 * parameter (Params). It shows up in the Tuner panel while the shader is on
 * screen, is dragged live, saved with Ctrl+S to views/params.json and reloaded
 * on the next run, and the shader follows it every frame with no C++ at all.
 *
 *   - shader: sky.frag
 *     uniforms:
 *       sun:   [0.3, 0.9, 0.2]                  # vec3  \  type read off
 *       tint:  "#ffcc88"                        # color  ) the literal
 *       steps: 64                               # int   /
 *       speed: {default: 1.0, min: 0, max: 5}   # bounded, so a slider
 *       mode:  {type: int, default: 0}          # explicit type
 *
 * Types are float, int, bool, vec2, vec3 and color (vec4); bounds are optional
 * (unbounded parameters are dragged rather than slid) and apply to every
 * component of a vector at once. Parameters are named "<item>/<uniform>", so
 * two placements of the same .frag under different ids are tuned separately.
 * A uniform absent from the compiled shader is ignored, like Shader::bind, so
 * an unfinished .frag never breaks the deck.
 *
 * Shader textures
 * ---------------
 * "textures:" binds an image file to the sampler of the same name, which the
 * shader declares itself
 *
 *   uniform sampler2D noise;        // in the .frag
 *   uniform vec2      noise_size;   // optional, its size in pixels
 *
 *   - shader: sky.frag
 *     textures:
 *       noise: noise.png
 *       grad:  {file: gradient.png, filter: nearest, wrap: repeat}
 *
 * filter is nearest|linear (default linear), wrap is clamp|repeat (default
 * clamp). Re-declaring the same file is free, so a hot reload does not re-read
 * the image; removing an entry unbinds it. Only image files, a texture fed by
 * another pass needs a streaming order a manifest cannot express and stays on
 * the C++ side (Shader::setTexture).
 *
 * Items are referenced (by remove/replace/below/...) through their key
 * (latex key, image filename stem, object name, "title") or an explicit
 * "id: name" field. References resolve to the most recent item with that
 * name, in manifest order.
 *
 * Reordering steps desyncs C++ updaters that branch on
 * t.relative_frame_number. Prefer marking the relevant frames with
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

    // owned-slideshow mode, initializes the slideshow and the project's latex
    // resources when present, then parses the deck file
    void init(const std::string& project_name, path deck_file, int argc, char** argv);
    // builds the slides, installs the hot-reload watcher and runs the show
    void run();
    Slideshow& slideshow();

    DeckLoader();
    ~DeckLoader();

    void registerObject(const std::string& name, const ObjectFactory& factory);
    void registerObject(const std::string& name, const PrimitiveInSlide& pis);
    // for objects that need no state, registered with a default one
    void registerObject(const std::string& name, const PrimitiveFactory& factory);
    void registerObject(const std::string& name, PrimitivePtr ptr);
    void registerObject(const std::string& name, const GroupFactory& factory);
    void registerObject(const std::string& name, const PrimitiveGroup& group);

    // "follow" reaches shader and scene points on its own. This is the escape
    // hatch for a position computed some other way, and it shadows the
    // "<item>.<snippet>" form. The placer returns window coordinates, y down.
    //   deck.registerPlacer("cursor", []{ return myTrackedThing(); });
    void registerPlacer(const std::string& name, const std::function<vec2()>& placer);

    // runs the manifest through the SlideManager composition API
    void build(SlideManager& show);

    // call once per frame, recomposes the slideshow when the manifest changed
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
    // last LatexLoader::generation this deck was built against
    int latex_generation = 0;
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
    // named live positions, for "follow:"
    std::map<std::string, std::function<vec2()>> placer_registry;
    std::map<std::string, PrimitiveGroup> instantiated_groups;

    // name -> primitive references accumulated during build, in manifest order
    std::map<std::string, PrimitivePtr> named;

    // primitives used by the manifest at last build, to disable on rebuild
    std::set<PrimitivePtr> used_primitives;

    // what the step being built has placed already, two items of the same
    // content being one cached primitive a slide can only hold once
    std::set<PrimitivePtr> step_primitives;

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

    // declares each uniform as a persistent Params entry named "<ref>/<name>"
    // and returns those names. clear = false leaves the shader's other binds
    // alone, for a shader the deck did not create
    std::vector<std::string> declareShaderUniforms(const ShaderPtr& shader, const json& item,
                                                   const std::string& ref, bool clear = true);
    // the same pair of keys on an "object:" naming a shader
    void declareObjectShaderInputs(const ShaderPtr& shader, const std::string& object,
                                   const json& item);
    // what the last build declared on a C++-registered shader, so a reload
    // drops exactly that and leaves the owner's own binds standing
    std::map<std::string, std::pair<ShaderPtr, std::vector<std::string>>> object_uniforms;
    // binds an image file to each named sampler, dropping what is no longer
    // declared
    void declareShaderTextures(const ShaderPtr& shader, const json& item);
    // "view", the region of the plane a shader draws
    void declareShaderView(const ShaderPtr& shader, const json& item);
    // resolves a "follow" spec to a live screen position
    std::function<vec2()> resolveFollow(const std::string& spec);

    // applies at/below/above/right_of/left_of placement and adds to the slide
    // keep_placement leaves an item where it is when no placement is given
    void placeScreenItem(SlideManager& show, ScreenPrimitivePtr prim,
                         const json& item, const std::string& default_label,
                         bool keep_placement = false);

    PrimitivePtr resolve(const std::string& name) const;
    ScreenPrimitivePtr resolveScreen(const std::string& name) const;

    PrimitivePtr cached(const std::string& key, const std::function<PrimitivePtr()>& create);
};

}

#endif // DECKLOADER_H
