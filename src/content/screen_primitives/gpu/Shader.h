#ifndef SHADER_H
#define SHADER_H

#include "content/screen_primitives/ScreenPrimitive.h"
#include "content/authoring/Snippet.h"
#include <filesystem>
#include <functional>
#include "extern/json.hpp"

namespace slope {

/*
 * A screen-space fragment shader, ShaderToy style. A full-screen triangle is
 * drawn through the fragment source into an offscreen texture, then blitted
 * into the slide like an Image. Independent of polyscope's GL loader (entry
 * points are resolved through glfwGetProcAddress).
 *
 *   auto fx = Shader::FromFile("plasma.frag");
 *   show << fx->at("screen");
 *
 * With no "#version" line, a prelude is prepended declaring the built-ins :
 *
 *   uniform vec2  iResolution;   // render target size, in pixels
 *   uniform float iAspect;       // iResolution.x / iResolution.y
 *   uniform float iTime;         // seconds since the primitive appeared
 *   uniform float iTimeDelta;    // seconds since last frame
 *   uniform int   iFrame;        // frames this shader has rendered
 *   uniform float iFrameRate;    // frames per second (smoothed)
 *   uniform vec4  iMouse;        // xy = cursor (px, y up); zw = last click,
 *                                //   z<0 while the button is not pressed
 *   uniform vec2  iMouseNorm;    // cursor in 0..1 across the rect, y up
 *   uniform float iHovered;      // 1.0 while the cursor is over the rect
 *   uniform vec4  iDate;         // year, month(1-12), day, seconds since midnight
 *   out vec4 fragColor;          // write your result here
 *
 * plus the primitive's TimeObject, field for field under its own C++ names,
 * so a shader can follow the talk with no C++ at all :
 *
 *   uniform float from_begin;            // seconds since the slideshow started
 *   uniform float from_action;           // seconds since the last slide change
 *   uniform float inner_time;            // = iTime
 *   uniform float delta_time;            // = iTimeDelta
 *   uniform int   absolute_frame_number; // current slide index in the deck
 *   uniform int   relative_frame_number; // slides since this shader appeared
 *   uniform float transition_parameter;  // 0 -> 1 across the intro / outro
 *
 * and its queries, under the same names and taking the keyframe's name, as
 * C++ and Lua do. GLSL has no string type, so the name is replaced by its
 * slide index before the compile and nothing enters the shader's namespace
 *
 *   bool  afterKeyframe("reveal");      beforeKeyframe / atKeyframe
 *   int   slidesSinceKeyframe("reveal");
 *   float secondsSinceKeyframe("reveal");  // 0 until reached, never negative
 *   float slidePosition();                 // continuous, in slides
 *   float duringKeyframe("a");             // the 0..1..0 blend weight
 *   float duringKeyframe("a", "b");        // a window spanning two of them
 *   float duringKeyframe("a", true);       // sequential, no default args
 *
 * A name the deck does not have is an error, and every query on it is false
 *
 * A minimal shader :
 *
 *   void main() {
 *       vec2 uv = gl_FragCoord.xy / iResolution;
 *       fragColor = vec4(uv, 0.5 + 0.5*sin(iTime), 1.0);
 *   }
 *
 * A source with its own "#version" is taken verbatim.
 *
 * ── #include ────────────────────────────────────────────────────────────────
 *   #include "sdf.glsl"       // next to the including file, else the project
 *   #include <palette.glsl>   //   data path; the two forms are equivalent
 *
 * Textual inclusion, expanded before compiling. "#pragma once" is honoured,
 * cycles are refused, and diagnostics stay on the right line. Hot reload
 * watches included files too.
 *
 * ── Uniforms ──────────────────────────────────────────────────────────────
 * Declare "uniform float radius;" in the shader, then from C++ :
 *
 *   fx->set("radius", 0.3f);                              // fixed value
 *   fx->bind("radius", [&]{ return slider_value; });       // re-read every frame
 *   fx->bind("fade", [](const TimeObject& t){ return t.from_action; });
 *
 * set/bind accept float, int, vec2, vec (vec3) and RGBA (vec4). Unknown names
 * are silently ignored, so this never throws while you are editing live.
 * bind() uploads every scalar as a float, an "uniform int" wants bindInt().
 *
 * From a deck manifest, "uniforms:" on a shader item declares them instead,
 * each backed by a persistent Params entry (Tuner panel, params.json) :
 *
 *   - shader: plasma.frag
 *     uniforms:
 *       sun: [0.3, 0.9, 0.2]     # vec3, dragged in the panel
 *       speed: {default: 1.0, min: 0, max: 5}
 *
 * ── Following the slides ────────────────────────────────────────────────────
 *   col *= clamp(from_action, 0.0, 1.0);           // fade in over 1s
 *   if (absolute_frame_number >= 4) col += glow;   // once slide 4 is reached
 *
 * ── Textures & multi-pass ───────────────────────────────────────────────────
 * Declare a sampler and hand it a source. A texture can be an image file, a
 * CPU array, the output of another Shader, or this shader's own previous frame:
 *
 *   uniform sampler2D noise;        // in the .frag
 *   fx->setTexture("noise", "noise.png");
 *
 *   auto sim  = Shader::FromFile("sim.frag");
 *   sim->setFloatBuffer();              // keep precision across iterations
 *   sim->setTextureSelf("previous");    // last frame (ping-pong)
 *   sim->setHidden();                   // compute-only, nothing on screen yet
 *
 *   auto view = Shader::FromFile("colorize.frag");
 *   view->setTexture("field", sim);     // = the simulation's output
 *   show << sim << view->at("screen");  // sim first, it feeds view
 *
 * This feedback loop is what lets an iterative simulation run entirely on the
 * GPU here. How many textures can be bound at once is the driver's texture
 * unit count (>= 16, usually 32), less one kept for the scene depth buffer.
 *
 * setChannel/setChannelSelf/setData are the same thing under the reserved
 * names "iChannel0".."iChannel3", which the prelude declares. That is all the
 * ShaderToy compatibility is, and a channel can do nothing a texture cannot.
 *
 * ── Sharing the 3D scene ────────────────────────────────────────────────────
 * The camera uniforms (iView/iProj/iCamPos, iScreenRect) let a shader trace
 * the same rays polyscope does. useSceneDepth() also hands it polyscope's
 * depth buffer, so a raymarched surface can be occluded by real geometry :
 *
 *   fx->useSceneDepth();
 *   #include <camera.glsl>
 *   vec3 ro, rd; polyscopeRay(ro, rd);
 *   float t = march(ro, rd);
 *   if (!visibleOverScene(ro + t*rd)) discard;   // real geometry is nearer
 *
 * See useSceneDepth() for what it costs.
 */
class Shader;
using ShaderPtr = std::shared_ptr<Shader>;

class Shader : public ScreenPrimitive {
public:
    Shader() {}
    ~Shader();

    // w/h set the offscreen render resolution up front. Omit, or pass <= 0,
    // to keep the default (the window's own resolution).
    static ShaderPtr Add(const std::string& fragment_source, int w = 0, int h = 0);
    static ShaderPtr FromFile(const path& file, int w = 0, int h = 0);

    // render resolution of the offscreen target (independent of on-screen
    // size). iResolution reports this. Defaults to the window's resolution.
    void setResolution(int w, int h);

    // ── uniforms ────────────────────────────────────────────────────────────
    // fixed values
    void set(const std::string& name, float v);
    void set(const std::string& name, double v) { set(name, float(v)); }
    void set(const std::string& name, int v);
    void set(const std::string& name, const vec2& v);
    void set(const std::string& name, const vec& v);
    void set(const std::string& name, const RGBA& v);
    // live values, re-read every frame. The callable's return type (float/
    // double/int, vec2, vec, RGBA) selects how it is uploaded.
    //
    // The callable may also take the primitive's TimeObject, so a uniform can
    // follow the talk without capturing any outside state :
    //   fx->bind("fade", [](const TimeObject& t){ return t.from_action; });
    template<class F>
    void bind(const std::string& name, F f) {
        if constexpr (std::is_invocable_v<F&, const TimeObject&>) {
            using R = std::decay_t<std::invoke_result_t<F&, const TimeObject&>>;
            if constexpr (std::is_same_v<R, vec2>)      bindV2(name, [f](const TimeObject& t){ return f(t); });
            else if constexpr (std::is_same_v<R, vec>)  bindV3(name, [f](const TimeObject& t){ return f(t); });
            else if constexpr (std::is_same_v<R, RGBA>) bindV4(name, [f](const TimeObject& t){ return f(t); });
            else                                        bindF (name, [f](const TimeObject& t){ return float(f(t)); });
        } else {
            using R = std::decay_t<std::invoke_result_t<F&>>;
            if constexpr (std::is_same_v<R, vec2>)      bindV2(name, [f](const TimeObject&){ return f(); });
            else if constexpr (std::is_same_v<R, vec>)  bindV3(name, [f](const TimeObject&){ return f(); });
            else if constexpr (std::is_same_v<R, RGBA>) bindV4(name, [f](const TimeObject&){ return f(); });
            else                                        bindF (name, [f](const TimeObject&){ return float(f()); });
        }
    }
    // int and bool uniforms. bind() uploads everything scalar as a float, which
    // an "uniform int" rejects, so integers go through their own entry point
    //   uniform int steps;   fx->bindInt("steps", [=]{ return int(n); });
    void bindInt(const std::string& name, std::function<int()> f);
    void bindInt(const std::string& name, std::function<int(const TimeObject&)> f);
    // A uniform whose width is only known when it is read, which is what a
    // snippet variable is. The callable fills up to four components and returns
    // how many, and the matching glUniform is used. 0 uploads nothing.
    void bindDynamic(const std::string& name, std::function<int(scalar*)> f);
    // The same, fed by the snippet variable or parameter of that name, which is
    // what a deck's bare "uniforms: - reveal" entry does. For a shader the deck
    // did not create, one with an updater say.
    //
    //   fx->bind("reveal");                              // uniform <- "reveal"
    //   fx->bind({"show_field", "show_basin"});          // several at once
    //
    // The width follows the value, 1 to 4 components, so the same call serves a
    // float and a vec3 ; an "uniform int" still wants bindInt(). A name that
    // resolves to nothing uploads nothing, and says so once.
    void bind(const std::string& name);
    void bind(std::initializer_list<const char*> names);
    void unset(const std::string& name) { uniforms.erase(name); }
    // whether a value is currently attached to that name, which a declarative
    // owner checks before dropping a bind it may not own
    bool isBound(const std::string& name) const { return uniforms.count(name) > 0; }
    // drops every user uniform (set/bind); the built-ins are unaffected. What
    // a declarative owner (the deck loader) uses to re-declare its whole set
    // on a hot reload, so a uniform deleted from the manifest really goes away.
    void clearUniforms() { uniforms.clear(); }

    // ── textures ────────────────────────────────────────────────────────────
    // Sampling inputs, named. Declare the sampler in the shader and hand it a
    // source from here :
    //
    //   uniform sampler2D noise;        // in the .frag
    //   uniform vec2      noise_size;   // optional, its size in pixels
    //
    //   fx->setTexture("noise", "noise.png");
    //
    // A name the compiled program does not declare is ignored, like every
    // other uniform. How many can be bound at once is the driver's texture
    // unit count (16 at the very least, usually 32), one of which is kept for
    // the scene depth buffer.
    enum class Filter { Nearest, Linear };
    enum class Wrap   { Clamp, Repeat };

    // a static image, loaded once. Re-setting the same file with the same
    // filter/wrap is a no-op, so a declarative owner can re-declare its whole
    // set cheaply (see retainTextures)
    void setTexture(const std::string& name, const path& image_file,
                    Filter f = Filter::Linear, Wrap w = Wrap::Clamp);
    // another shader's current output (must be streamed before this one)
    void setTexture(const std::string& name, const ShaderPtr& src, int attachment = 0);
    // this shader's previous frame, double-buffered (ping-pong), the
    // backbone of iterative GPU work
    void setTextureSelf(const std::string& name, int attachment = 0);
    void clearTexture(const std::string& name);
    void clearTextures();
    // drop every *file-backed* texture whose name is not listed, which a
    // declarative owner (the deck loader) uses so a texture removed from the
    // manifest really goes away, while the ones still declared keep their GL
    // objects. Data textures and inter-pass ones are left alone, they were
    // set from code such an owner never saw.
    void retainTextures(const std::vector<std::string>& names);

    // ── the same, ShaderToy-style (iChannel0..3) ────────────────────────────
    // The prelude declares four numbered samplers and their resolutions :
    //   uniform sampler2D iChannel0;
    //   uniform vec3      iChannelResolution[4]; // (w, h, 1) per channel
    // so a shader written for ShaderToy runs here unchanged. These are exactly
    // the calls above under the reserved names "iChannel0".."iChannel3".
    static std::string ChannelName(int i);
    void setChannel(int i, const path& image_file,
                    Filter f = Filter::Linear, Wrap w = Wrap::Clamp)
    { setTexture(ChannelName(i), image_file, f, w); }
    void setChannel(int i, const ShaderPtr& src, int attachment = 0)
    { setTexture(ChannelName(i), src, attachment); }
    void setChannelSelf(int i, int attachment = 0)
    { setTextureSelf(ChannelName(i), attachment); }
    void clearChannel(int i) { clearTexture(ChannelName(i)); }

    // RGBA32F targets instead of 8-bit, for values that must survive many
    // feedback iterations without banding (accumulation, physics)
    void setFloatBuffer(bool on = true);
    // sampling of THIS shader's own target(s). Simulations usually want
    // Nearest + Repeat.
    void setFilter(Filter f);
    void setWrap(Wrap w);
    // compute-only pass, it keeps updating but is never blitted onto the slide
    void setHidden(bool on = true);

    // ── the 3D scene's depth buffer ─────────────────────────────────────────
    // Lets a shader scene be occluded by polyscope's meshes, not just share
    // their camera. A shader using <camera.glsl>'s depth API turns this on
    // for itself at compile time; call it yourself only from a helper header.
    //
    //   #include <camera.glsl>
    //   vec3 ro, rd; polyscopeRay(ro, rd);
    //   float t = raymarch(ro, rd);
    //   if (!visibleOverScene(ro + t*rd)) discard;   // a mesh is in front
    //
    // Not free, it pins polyscope's depth peeling to one pass while any shader
    // wants it. Depth is current-frame for a visible shader (rendering defers
    // past the scene pass); a hidden (compute-only) shader reads last frame's.
    void useSceneDepth(bool on = true);

    // ── the shader's own world space ────────────────────────────────────────
    // What region of the plane the shader draws. The .frag reads it back as
    // iWorld(), and screen primitives can be placed at a world point, so a
    // label rides a feature only the shader knows how to find.
    //
    //   fx->setView({0, 0}, 3.2);          // 3.2 world units above the middle
    //   show << eq->at(fx->tracker(vec2(1, 0)));   // sits on the point z = 1
    //
    // bindView lets the view move like any uniform, and the label follows.
    // Horizontal extent is half_height times the render aspect, so widening
    // the window shows more rather than stretching.
    void setView(const vec2& center, scalar half_height);
    void bindView(std::function<vec2()> center, std::function<scalar()> half_height);
    bool hasView() const { return bool(view_half); }

    // World to window position, relative [0,1]^2 with y down, the space anchors
    // live in. Uses the view and rect the shader was last drawn with, so a
    // tracked label agrees with the pixels under it. Before the first draw,
    // the rect it would occupy centered in the window.
    vec2 worldToScreen(const vec2& w) const;
    vec2 screenToWorld(const vec2& s) const;

    // a placer for ScreenPrimitive::at(), so a primitive tracks a world point.
    // `offset` is added afterwards, in screen units, to clear the point itself.
    //   show << label->at(fx->tracker([]{ return Snippet::get("z1").v2(); }))
    std::function<vec2()> tracker(const vec2& world, const vec2& offset = vec2::Zero());
    std::function<vec2()> tracker(std::function<vec2()> world, vec2 offset = vec2::Zero());

    // ── multiple render targets (MRT) ───────────────────────────────────────
    // Emit several outputs from one pass. fragColor (location 0) is already
    // declared; add extras with explicit locations (do NOT redeclare 0) :
    //   layout(location = 1) out vec4 oPosition;
    // Then setTargets(3). display() shows attachment 0; setChannel(i, src,
    // attachment) and readback*(…, attachment) reach the others. Up to 4.
    void setTargets(int n);
    int  targets() const { return num_targets; }

    // ── upload arbitrary data as a texture ──────────────────────────────────
    // Binds a CPU array as a float texture. `comps` is components per texel
    // (1..4 -> R/RG/RGB/RGBA). Call again to refresh; the GL texture is reused
    // in place when the layout has not changed :
    //   std::vector<float> field(w*h);
    //   fx->setTexture("field", field, w, h);
    void setTexture(const std::string& name, const float* data, int w, int h,
                    int comps = 1, Filter f = Filter::Linear, Wrap wrap = Wrap::Clamp);
    void setTexture(const std::string& name, const std::vector<float>& data,
                    int w, int h, int comps = 1,
                    Filter f = Filter::Linear, Wrap wrap = Wrap::Clamp)
    { setTexture(name, data.data(), w, h, comps, f, wrap); }
    // ── a snippet function as a texture ─────────────────────────────────────
    // Samples a callable section onto a grid and binds the result. A section
    // that reads t is resampled every frame, one that does not is sampled once.
    // See SnippetTexture for the cost and for overriding that verdict :
    //   SnippetTexture::Spec sp; sp.fn = "prior_mean"; sp.u = vec2(-6,6);
    //   fx->setTexture("prior", sp);
    void setTexture(const std::string& name, const SnippetTexture::Spec& spec,
                    Filter f = Filter::Linear, Wrap wrap = Wrap::Clamp);

    // the same, onto a numbered channel
    void setData(int i, const float* data, int w, int h, int comps = 1,
                 Filter f = Filter::Linear, Wrap wrap = Wrap::Clamp)
    { setTexture(ChannelName(i), data, w, h, comps, f, wrap); }
    void setData(int i, const std::vector<float>& data, int w, int h, int comps = 1,
                 Filter f = Filter::Linear, Wrap wrap = Wrap::Clamp)
    { setTexture(ChannelName(i), data.data(), w, h, comps, f, wrap); }

    // ── shader storage buffers (SSBO) ───────────────────────────────────────
    // Large, structured buffers the shader reads *and writes* :
    //   layout(std430, binding = 0) buffer Seeds { vec4 seed[]; };
    // fx->setBuffer(0, seeds) uploads a CPU array verbatim (mind std430
    // packing when types are interleaved: vec3 aligns to 16 bytes).
    void setBuffer(int binding, const void* data, std::size_t bytes);
    template<class T>
    void setBuffer(int binding, const std::vector<T>& v)
    { setBuffer(binding, v.data(), v.size() * sizeof(T)); }
    // allocate a zeroed buffer of `bytes` (a scratch / output / atomic target)
    void allocBuffer(int binding, std::size_t bytes);
    // read an SSBO back after the shader has run (a barrier follows every draw)
    bool readBuffer(int binding, void* dst, std::size_t bytes) const;
    template<class T>
    bool readBuffer(int binding, std::vector<T>& v) const
    { return readBuffer(binding, v.data(), v.size() * sizeof(T)); }
    void clearBuffer(int binding);   // release the buffer at this binding
    // zero a buffer in place, on the GPU, which a pass accumulating into it
    // with atomics needs at the top of every frame, without the round trip a
    // setBuffer of zeros would cost
    void clearBufferData(int binding, unsigned int value = 0);

    // Bind the buffer `src` holds at `src_binding` to our `binding` as well,
    // one buffer for two passes with no copy. The producer must be streamed
    // the consumer (show << producer << consumer), same as setChannel. A
    // barrier after every draw makes the writes visible within the frame.
    // Both keep it alive; whichever is dropped last releases it.
    void shareBuffer(int binding, const ShaderPtr& src, int src_binding);

    // ── read the rendered result back ───────────────────────────────────────
    // Reads color attachment `attachment` as RGBA floats, row-major and
    // bottom-up. 8-bit targets come back normalised to 0..1, float targets
    // exact. False if nothing has been rendered yet.
    bool readback(std::vector<float>& out, int attachment = 0) const;
    RGBA readbackMean(int attachment = 0) const;
    RGBA readbackPixel(int x, int y, int attachment = 0) const;
    int  bufferWidth()  const { return res_x; }
    int  bufferHeight() const { return res_y; }

    // re-reads/recompiles any file-backed shader whose source changed, and
    // recompiles every shader when the deck's keyframes move
    static void HotReloadIfModified();

    vec2 getSize() const override;

    void draw(const TimeObject& t, const StateInSlide& sis) override;
    void playIntro(const TimeObject& t, const StateInSlide& sis) override;
    void playOutro(const TimeObject& t, const StateInSlide& sis) override;

private:
    // a uniform is a closure that, given its resolved location, pushes its
    // current value, unifying fixed (set) and live (bind) uniforms
    using UniformSetter = std::function<void(int /*location*/, const TimeObject&)>;
    std::map<std::string, UniformSetter> uniforms;

    // .cpp-side helpers building the GL upload closures for bind()
    void bindF (const std::string& name, std::function<float(const TimeObject&)> f);
    void bindV2(const std::string& name, std::function<vec2(const TimeObject&)> f);
    void bindV3(const std::string& name, std::function<vec(const TimeObject&)> f);
    void bindV4(const std::string& name, std::function<RGBA(const TimeObject&)> f);

    std::string fragment_src;
    unsigned int program = 0;   // GLuint; kept opaque to avoid a GL include here
    unsigned int vao = 0;

    // one target for a plain shader, two (ping-pong) for feedback. buf[cur]
    // always holds the latest output.
    static constexpr int kMaxTargets = 4;
    struct Target { unsigned int tex[kMaxTargets] = {0,0,0,0}; unsigned int fbo = 0; };
    Target buf[2];
    int num_targets = 1;        // number of color outputs (MRT)
    int cur = 0;                // buf[cur] = latest output
    bool feedback = false;      // some channel samples our previous frame
    bool float_buffer = false;  // RGBA32F targets
    bool hidden = false;        // compute-only, updates but never blits
    unsigned int self_filter = 0x2601 /*LINEAR*/;
    unsigned int self_wrap   = 0x812F /*CLAMP_TO_EDGE*/;

    // one bound texture, whatever it is sourced from
    struct Texture {
        // NB: "None" is an X11 macro, so the empty state is "Off"
        enum class Kind { Off, Image, ShaderOut, Self } kind = Kind::Off;
        unsigned int image_tex = 0; // Kind::Image (image file *or* data texture), owned
        int w = 0, h = 0;           // size, reported through <name>_size
        int comps = 0;              // >0 when it is a float data texture
        // Kind::ShaderOut. Weak, the source may be dropped while we still
        // hold this texture.
        std::weak_ptr<Shader> src;
        int attachment = 0;         // Kind::ShaderOut, which MRT output to read

        // what it was loaded from, so re-setting the same image is a no-op
        std::string file;
        Filter filter = Filter::Linear;
        Wrap   wrap   = Wrap::Clamp;

        // >= 0 for the four ShaderToy channels, which also feed
        // iChannelResolution[i], which a named texture has no part in
        int legacy_channel = -1;

        // resolved once per link, like every other uniform location. The
        // program they belong to is stamped so a relink re-resolves them.
        int sampler_loc = -1, size_loc = -1;
        unsigned int loc_program = 0;
        int unit = -1;              // texture unit assigned at bind time
    };
    static constexpr int kChannels = 4;   // how many iChannelN the prelude declares
    std::map<std::string, Texture> textures;

    // sampled snippets, re-uploaded on the frames they actually change
    struct SnippetTex {
        std::shared_ptr<SnippetTexture> tex;
        Filter filter = Filter::Linear;
        Wrap   wrap   = Wrap::Clamp;
    };
    std::map<std::string, SnippetTex> snippet_textures;
    void refreshSnippetTextures();

    // polyscope's depth buffer goes on the unit just past the textures, so it
    // depends on how many are bound this frame
    int  scene_depth_unit = kChannels;
    bool bound_scene_depth = false;
    // how many units were handed out on the last draw, to unbind exactly those
    int bound_units = 0;
    bool wants_scene_depth = false;   // useSceneDepth()

    // how many live shaders asked for scene depth; restores the original peel
    // pass count once the last one goes away
    inline static int scene_depth_users = 0;
    inline static int saved_peel_passes = -1;
    static void applySceneDepthMode();
    // true when `src` uses the scene-depth API (own text only, see .cpp)
    static bool referencesSceneDepth(const std::string& src);

    // drop a texture, freeing the GL object it owns (if any)
    void releaseTexture(const std::string& name);
    // tag the four reserved "iChannelN" names, which also feed
    // iChannelResolution[N]
    static void markLegacyChannel(const std::string& name, Texture& t);
    // true when some texture samples our own previous frame, which the
    // ping-pong second target exists for
    void refreshFeedback();

    // SSBOs, keyed by binding point
    // Shared, so that shareBuffer() can hand the same buffer to another pass.
    // The GL name is deliberately not freed on destruction, a Shader can
    // outlive the context (see ~Shader), and the driver reclaims it then.
    struct StorageBuffer { unsigned int id = 0; std::size_t bytes = 0; };
    using StorageBufferPtr = std::shared_ptr<StorageBuffer>;
    std::map<int, StorageBufferPtr> ssbos;

    // uniform locations, resolved once per link rather than per frame
    // (glGetUniformLocation is a string lookup, ~250ns each)
    struct BuiltinLocs {
        int iResolution = -1, iAspect = -1, iTime = -1, iTimeDelta = -1;
        int iFrame = -1, iFrameRate = -1;
        int from_begin = -1, from_action = -1, inner_time = -1, delta_time = -1;
        int absolute_frame_number = -1, relative_frame_number = -1;
        int transition_parameter = -1;
        int iSlideTime = -1;   // float[KF_SLIDE_COUNT], for secondsSinceKeyframe
        int iView = -1, iViewInv = -1, iProj = -1, iProjInv = -1;
        int iCamPos = -1, iCamFov = -1, iScreenRect = -1, iWindowSize = -1;
        int iViewCenter = -1, iViewHalf = -1;
        int iSceneDepth = -1, iSceneDepthValid = -1, iSceneDepthSize = -1;
        int iMouse = -1, iMouseNorm = -1, iHovered = -1, iDate = -1;
        // the samplers themselves are resolved per texture (they are named at
        // runtime); only the ShaderToy resolution array is a fixed built-in
        int iChannelRes[kChannels] = {-1, -1, -1, -1};
    };
    BuiltinLocs uloc;
    // user uniforms are named at runtime, so these fill in lazily
    std::map<std::string, int> user_uniform_loc;
    void cacheUniformLocations();

    int res_x = int(Options::ScreenResolutionWidth), res_y = int(Options::ScreenResolutionHeight);
    // false at the screen-size default, getSize() then shows it at that size
    // 1:1, rather than through the 1920x1080-relative scaling an explicit
    // resolution goes through
    bool explicit_resolution = false;
    bool gl_ready = false;      // resources created (needs a live context)
    bool compiled = false;      // last compile succeeded
    bool needs_recompile = true;

    // mouse click latch, for iMouse.zw (position of the last press over the rect)
    float last_click_x = 0, last_click_y = 0;
    bool mouse_was_down = false;

    // iFrame, how many times *this* shader has rendered, distinct from the
    // slide index (iSlide)
    int frames_rendered = 0;

    // the texture display()/downstream should read (color attachment `a`)
    unsigned int currentTexture(int a = 0) const { return buf[cur].tex[a]; }

    // file backing, for hot reload (mirrors Code). source_path is what the
    // caller asked for; source_file is it resolved against the project data
    // path, which is only known once the deck is initialised, hence the
    // re-resolve on every reload.
    path source_path;
    path source_file;
    std::filesystem::file_time_type last_modified;
    bool from_file = false;
    bool load_failed = false;         // reading failed, retried on every tick
    bool load_error_reported = false; // ... but logged once, not 5x a second

    // files reached through #include when the source was last expanded, with
    // their timestamps; hot reload watches these too
    std::vector<std::pair<std::string, std::filesystem::file_time_type>> include_deps;
    // source-string index -> file, to make a failed compile's "N:line" readable
    std::vector<std::string> source_units;

    // ── world space (setView) ───────────────────────────────────────────────
    std::function<vec2()>   view_center;
    std::function<scalar()> view_half;
    // What was last uploaded to iViewCenter/iViewHalf, and the rect it was
    // drawn into (window relative, y down). worldToScreen inverts these rather
    // than re-reading the callables, so it cannot disagree with the image.
    mutable vec2   drawn_view_center = vec2::Zero();
    mutable scalar drawn_view_half   = 1;
    mutable vec2   drawn_rect_min = vec2::Zero(), drawn_rect_max = vec2(1, 1);
    mutable bool   rect_recorded  = false;
    bool bad_view_reported = false;   // a degenerate view is said once, not per frame

    // where this shader is drawn, in ImGui display units (y down). Shared by
    // the blit and the camera uniforms, which must agree exactly.
    void screenRect(const StateInSlide& sis, ImVec2& pmin, ImVec2& pmax) const;

    void ensureResources();     // lazy GL init, on first draw
    void recompile();
    void renderToTexture(const TimeObject& t, const StateInSlide& sis);
    void display(const StateInSlide& sis, float global_alpha);
    void reloadFromFile();

    // the shared body of draw/playIntro/playOutro, which differ only in alpha
    void drawWith(const TimeObject& t, const StateInSlide& sis, float alpha);

    // A shader reading scene depth renders from an ImGui draw callback instead
    // of inline, so it runs after polyscope draws the scene (this frame's
    // depth, not last frame's). See runNextPendingRender().
    static void ImGuiRenderCallback(const ImDrawList*, const ImDrawCmd* cmd);
    void runNextPendingRender();
    void reportRenderError(const std::string& what);

    // One recorded placement, waiting for its callback. A shader can be
    // placed more than once per slide, so these queue rather than overwrite.
    // The rect is captured here since the callback has no ImGui window stack.
    struct PendingRender {
        TimeObject   time;
        StateInSlide sis;
        ImVec2       pmin, pmax;
    };
    std::vector<PendingRender> pending;
    std::size_t pending_next = 0;
    int  record_frame = -1;             // ImGui frame the queue was built for
    bool render_error_reported = false; // throttles the exception log to once

    // the rect the callback is currently rendering for, taken from its job
    ImVec2 pending_pmin, pending_pmax;
    bool   use_pending_rect = false;

    // every live Shader, so keyframe/#include invalidation reaches all of them
    inline static std::vector<Shader*> all_shaders;
};

}

#endif // SHADER_H
