#ifndef SHADER_H
#define SHADER_H

#include "ScreenPrimitive.h"
#include <filesystem>
#include <functional>

namespace slope {

/*
 * A screen-space fragment shader, ShaderToy-style : a full-screen triangle is
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
 *
 * set/bind accept float, int, vec2, vec (vec3) and RGBA (vec4). Unknown names
 * are silently ignored, so this never throws while you are editing live.
 *
 * ── Following the slides ────────────────────────────────────────────────────
 *   col *= clamp(from_action, 0.0, 1.0);           // fade in over 1s
 *   if (absolute_frame_number >= 4) col += glow;   // once slide 4 is reached
 *
 * ── Channels & multi-pass ───────────────────────────────────────────────────
 * iChannel0..3 are sampler inputs (setChannel). A channel can be an image, the
 * output of another Shader, or this shader's own previous frame :
 *
 *   auto sim  = Shader::FromFile("sim.frag");
 *   sim->setFloatBuffer();          // keep precision across iterations
 *   sim->setChannelSelf(0);         // iChannel0 = previous frame (ping-pong)
 *   sim->setHidden();               // compute-only, nothing on screen yet
 *
 *   auto view = Shader::FromFile("colorize.frag");
 *   view->setChannel(0, sim);       // iChannel0 = the simulation's output
 *   show << sim << view->at("screen");   // sim first : it feeds view
 *
 * This feedback loop is what lets an iterative simulation run entirely on the
 * GPU here.
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

    // ── the easy part : uniforms ────────────────────────────────────────────
    // fixed values
    void set(const std::string& name, float v);
    void set(const std::string& name, double v) { set(name, float(v)); }
    void set(const std::string& name, int v);
    void set(const std::string& name, const vec2& v);
    void set(const std::string& name, const vec& v);
    void set(const std::string& name, const RGBA& v);
    // live values, re-read every frame. The callable's return type (float/
    // double/int, vec2, vec, RGBA) selects how it is uploaded.
    template<class F>
    void bind(const std::string& name, F f) {
        using R = std::decay_t<std::invoke_result_t<F&>>;
        if constexpr (std::is_same_v<R, vec2>)      bindV2(name, [f]{ return f(); });
        else if constexpr (std::is_same_v<R, vec>)  bindV3(name, [f]{ return f(); });
        else if constexpr (std::is_same_v<R, RGBA>) bindV4(name, [f]{ return f(); });
        else                                        bindF (name, [f]{ return float(f()); });
    }
    void unset(const std::string& name) { uniforms.erase(name); }

    // ── texture channels (iChannel0..3) ─────────────────────────────────────
    // sampling inputs, ShaderToy-style :
    //   uniform sampler2D iChannel0;             // declared by the prelude
    //   uniform vec3      iChannelResolution[4]; // (w, h, 1) per channel
    //   vec4 c = texture(iChannel0, uv);
    enum class Filter { Nearest, Linear };
    enum class Wrap   { Clamp, Repeat };

    // a static image, loaded once
    void setChannel(int i, const path& image_file,
                    Filter f = Filter::Linear, Wrap w = Wrap::Clamp);
    // another shader's current output (must be streamed before this one)
    void setChannel(int i, const ShaderPtr& src, int attachment = 0);
    // this shader's previous frame : double-buffering (ping-pong), the
    // backbone of iterative GPU work
    void setChannelSelf(int i, int attachment = 0);
    void clearChannel(int i);

    // RGBA32F targets instead of 8-bit : for values that must survive many
    // feedback iterations without banding (accumulation, physics)
    void setFloatBuffer(bool on = true);
    // sampling of THIS shader's own target(s). Simulations usually want
    // Nearest + Repeat.
    void setFilter(Filter f);
    void setWrap(Wrap w);
    // compute-only pass : keeps updating but is never blitted onto the slide
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
    // Not free : pins polyscope's depth peeling to one pass while any shader
    // wants it. Depth is current-frame for a visible shader (rendering defers
    // past the scene pass); a hidden (compute-only) shader reads last frame's.
    void useSceneDepth(bool on = true);

    // ── multiple render targets (MRT) ───────────────────────────────────────
    // Emit several outputs from one pass. fragColor (location 0) is already
    // declared; add extras with explicit locations (do NOT redeclare 0) :
    //   layout(location = 1) out vec4 oPosition;
    // Then setTargets(3). display() shows attachment 0; setChannel(i, src,
    // attachment) and readback*(…, attachment) reach the others. Up to 4.
    void setTargets(int n);
    int  targets() const { return num_targets; }

    // ── CPU → GPU : upload arbitrary data as a channel ──────────────────────
    // Binds a CPU array as a float data texture on iChannel i. `comps` is
    // components per texel (1..4 -> R/RG/RGB/RGBA). Call again to refresh :
    //   std::vector<float> field(w*h);
    //   fx->setData(0, field, w, h);     // iChannel0 = the field
    void setData(int i, const float* data, int w, int h, int comps = 1,
                 Filter f = Filter::Linear, Wrap wrap = Wrap::Clamp);
    void setData(int i, const std::vector<float>& data, int w, int h, int comps = 1,
                 Filter f = Filter::Linear, Wrap wrap = Wrap::Clamp)
    { setData(i, data.data(), w, h, comps, f, wrap); }

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

    // ── GPU → CPU : read the rendered result back ───────────────────────────
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
    // current value : unifies fixed (set) and live (bind) uniforms
    using UniformSetter = std::function<void(int /*location*/)>;
    std::map<std::string, UniformSetter> uniforms;

    // .cpp-side helpers building the GL upload closures for bind()
    void bindF (const std::string& name, std::function<float()> f);
    void bindV2(const std::string& name, std::function<vec2()> f);
    void bindV3(const std::string& name, std::function<vec()> f);
    void bindV4(const std::string& name, std::function<RGBA()> f);

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
    bool hidden = false;        // compute-only : update but never blit
    unsigned int self_filter = 0x2601 /*LINEAR*/;
    unsigned int self_wrap   = 0x812F /*CLAMP_TO_EDGE*/;

    // an iChannel binding
    struct Channel {
        // NB: "None" is an X11 macro, so the empty state is "Off"
        enum class Kind { Off, Image, ShaderOut, Self } kind = Kind::Off;
        unsigned int image_tex = 0; // Kind::Image (image file *or* setData texture), owned
        int w = 0, h = 0;           // resolution reported through iChannelResolution
        int comps = 0;              // >0 when it is a float data texture (setData)
        // Kind::ShaderOut. Weak : the source may be dropped while we still
        // hold this channel.
        std::weak_ptr<Shader> src;
        int attachment = 0;         // Kind::ShaderOut : which MRT output to read
    };
    static constexpr int kChannels = 4;
    Channel channel[kChannels];

    // polyscope's depth buffer goes on the unit just past the iChannels
    static constexpr int kSceneDepthUnit = kChannels;
    bool bound_scene_depth = false;
    bool wants_scene_depth = false;   // useSceneDepth()

    // how many live shaders asked for scene depth; restores the original peel
    // pass count once the last one goes away
    inline static int scene_depth_users = 0;
    inline static int saved_peel_passes = -1;
    static void applySceneDepthMode();
    // true when `src` uses the scene-depth API (own text only, see .cpp)
    static bool referencesSceneDepth(const std::string& src);

    // reset channel i, freeing the texture it owns (if any)
    void releaseChannel(int i);

    // SSBOs, keyed by binding point
    struct StorageBuffer { unsigned int id = 0; std::size_t bytes = 0; };
    std::map<int, StorageBuffer> ssbos;

    // uniform locations, resolved once per link rather than per frame
    // (glGetUniformLocation is a string lookup, ~250ns each)
    struct BuiltinLocs {
        int iResolution = -1, iAspect = -1, iTime = -1, iTimeDelta = -1;
        int iFrame = -1, iFrameRate = -1;
        int from_begin = -1, from_action = -1, inner_time = -1, delta_time = -1;
        int absolute_frame_number = -1, relative_frame_number = -1;
        int transition_parameter = -1;
        int iView = -1, iViewInv = -1, iProj = -1, iProjInv = -1;
        int iCamPos = -1, iCamFov = -1, iScreenRect = -1, iWindowSize = -1;
        int iSceneDepth = -1, iSceneDepthValid = -1, iSceneDepthSize = -1;
        int iMouse = -1, iMouseNorm = -1, iHovered = -1, iDate = -1;
        int iChannel[kChannels] = {-1, -1, -1, -1};
        int iChannelRes[kChannels] = {-1, -1, -1, -1};
    };
    BuiltinLocs uloc;
    // user uniforms are named at runtime, so these fill in lazily
    std::map<std::string, int> user_uniform_loc;
    void cacheUniformLocations();

    int res_x = int(Options::ScreenResolutionWidth), res_y = int(Options::ScreenResolutionHeight);
    // false at the screen-size default : getSize() then shows it at that size
    // 1:1, rather than through the 1920x1080-relative scaling an explicit
    // resolution goes through
    bool explicit_resolution = false;
    bool gl_ready = false;      // resources created (needs a live context)
    bool compiled = false;      // last compile succeeded
    bool needs_recompile = true;

    // mouse click latch, for iMouse.zw (position of the last press over the rect)
    float last_click_x = 0, last_click_y = 0;
    bool mouse_was_down = false;

    // iFrame : how many times *this* shader has rendered, distinct from the
    // slide index (iSlide)
    int frames_rendered = 0;

    // the texture display()/downstream should read (color attachment `a`)
    unsigned int currentTexture(int a = 0) const { return buf[cur].tex[a]; }

    // file backing, for hot reload (mirrors Code)
    path source_file;
    std::filesystem::file_time_type last_modified;
    bool from_file = false;

    // files reached through #include when the source was last expanded, with
    // their timestamps; hot reload watches these too
    std::vector<std::pair<std::string, std::filesystem::file_time_type>> include_deps;
    // source-string index -> file, to make a failed compile's "N:line" readable
    std::vector<std::string> source_units;

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

    // One recorded placement, waiting for its callback : a shader can be
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
