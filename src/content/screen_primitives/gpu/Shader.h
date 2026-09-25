#ifndef SHADER_H
#define SHADER_H

#include "content/screen_primitives/ScreenPrimitive.h"
#include "content/authoring/Snippet.h"
#include <filesystem>
#include <functional>
#include <set>
#include "extern/json.hpp"

namespace slope {

/*
 * A screen-space fragment shader, in the style of ShaderToy.
 * A full-screen triangle is drawn with the fragment source into an offscreen texture,
 * which is then copied into the slide like an Image.
 * It does not depend on the GL loader of polyscope, since the entry points are found with glfwGetProcAddress.
 *
 *   auto fx = Shader::FromFile("plasma.frag");
 *   show << fx->at("screen");
 *
 * When the source has no "#version" line, a prelude that declares the built-ins is added before it.
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
 * The fields of the TimeObject of the primitive are also declared, with their C++ names,
 * so a shader can follow the talk without any C++.
 *
 *   uniform float from_begin;            // seconds since the slideshow started
 *   uniform float from_action;           // seconds since the last slide change
 *   uniform float inner_time;            // = iTime
 *   uniform float delta_time;            // = iTimeDelta
 *   uniform int   absolute_frame_number; // current slide index in the deck
 *   uniform int   relative_frame_number; // slides since this shader appeared
 *   uniform float transition_parameter;  // 0 -> 1 across the intro / outro
 *   uniform float slide_progress;        // 0 -> 1 across the whole slide change
 *
 * Its queries are available too, with the same names and taking the name of a keyframe, as in C++ and Lua.
 * GLSL has no string type, so the name is replaced by its slide index before the compilation
 * and nothing is added to the namespace of the shader.
 *
 *   bool  afterKeyframe("reveal");      beforeKeyframe / atKeyframe
 *   int   slidesSinceKeyframe("reveal");
 *   float secondsSinceKeyframe("reveal");  // 0 until reached, never negative
 *   float slidePosition();                 // continuous, in slides
 *   float duringKeyframe("a");             // the 0..1..0 blend weight
 *   float duringKeyframe("a", "b");        // a window spanning two of them
 *   float duringKeyframe("a", true);       // sequential, no default args
 *
 * A name that the deck does not have is an error, and every query on it is false.
 *
 * A minimal shader.
 *
 *   void main() {
 *       vec2 uv = gl_FragCoord.xy / iResolution;
 *       fragColor = vec4(uv, 0.5 + 0.5*sin(iTime), 1.0);
 *   }
 *
 * A source with its own "#version" is used as it is.
 *
 * #include
 *   #include "sdf.glsl"       // next to the including file, else the project
 *   #include <palette.glsl>   //   data path; the two forms are equivalent
 *
 * The included text is inserted before the compilation. "#pragma once" is supported and cycles are refused.
 * Error messages still point to the right line.
 * The reload watches the included files too.
 *
 * Uniforms
 * Declare "uniform float radius;" in the shader, then from C++.
 *
 *   fx->set("radius", 0.3f);                              // fixed value
 *   fx->bind("radius", [&]{ return slider_value; });       // re-read every frame
 *   fx->bind("fade", [](const TimeObject& t){ return t.from_action; });
 *
 * set and bind accept float, int, vec2, vec (vec3) and RGBA (vec4).
 * Unknown names are ignored without any message, so nothing throws while you edit live.
 * bind() uploads every scalar as a float, so a "uniform int" needs bindInt().
 *
 * A "uniform float w[64];" is filled from a vector with set or bindArray, up to the length it declares.
 * For more than a few hundred values, use a texture or a buffer, because uniform storage is small
 * and shared by the whole shader.
 *
 * In a deck, "uniforms:" on a shader item declares them instead.
 * Each one is a Params entry, so it appears in the Tuner panel and is saved in params.json.
 *
 *   - shader: plasma.frag
 *     uniforms.
 *       sun: [0.3, 0.9, 0.2]     # vec3, dragged in the panel
 *       speed: {default: 1.0, min: 0, max: 5}
 *
 * Following the slides
 *   col *= clamp(from_action, 0.0, 1.0);           // fade in over 1s
 *   if (absolute_frame_number >= 4) col += glow;   // once slide 4 is reached
 *
 * Textures & multi-pass
 * Declare a sampler and give it a source. A texture can be an image file, an array from the CPU,
 * the output of another Shader, or the previous frame of this shader.
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
 * This feedback loop lets an iterative simulation run entirely on the GPU.
 * The number of textures that can be bound at once is the number of texture units of the driver
 * (at least 16, usually 32), minus one that is kept for the depth buffer of the scene.
 *
 * setChannel, setChannelSelf and setData do the same with the reserved names "iChannel0" to "iChannel3",
 * which the prelude declares. This is all the ShaderToy compatibility there is,
 * and a channel can do nothing that a texture cannot.
 *
 * Sharing the 3D scene
 * The camera uniforms (iView, iProj, iCamPos, iScreenRect) let a shader trace the same rays as polyscope.
 * useSceneDepth() also gives it the depth buffer of polyscope,
 * so a raymarched surface can be hidden by real geometry.
 *
 *   fx->useSceneDepth();
 *   #include <camera.glsl>
 *   vec3 ro, rd; polyscopeRay(ro, rd);
 *   float t = march(ro, rd);
 *   if (!visibleOverScene(ro + t*rd)) discard;   // real geometry is nearer
 *
 * See useSceneDepth() for its cost.
 */
class Shader;
using ShaderPtr = std::shared_ptr<Shader>;

class Shader : public ScreenPrimitive {
public:
    Shader() {}
    ~Shader();

    // Builds a shader from source text. w and h set the resolution of the offscreen rendering.
    // Leave them out, or give 0 or less, to use the resolution of the window.
    static ShaderPtr Add(const std::string& fragment_source, int w = 0, int h = 0);
    // Builds a shader from a file, which is compiled again when it changes.
    static ShaderPtr FromFile(const path& file, int w = 0, int h = 0);

    // Sets the resolution of the offscreen rendering, which does not depend on the size on screen.
    // iResolution gives this value. The default is the resolution of the window.
    void setResolution(int w, int h);

    // uniforms
    // Sets a fixed value.
    void set(const std::string& name, float v);
    void set(const std::string& name, double v) { set(name, float(v)); }
    void set(const std::string& name, int v);
    void set(const std::string& name, const vec2& v);
    void set(const std::string& name, const vec& v);
    void set(const std::string& name, const RGBA& v);
    // Sets a live value, read again at every frame.
    // The return type of the callable (float, double, int, vec2, vec or RGBA) decides how it is uploaded.
    //
    // The callable can also take the TimeObject of the primitive,
    // so a uniform can follow the talk without capturing any outside state.
    //   fx->bind("fade", [](const TimeObject& t){ return t.from_action; });
    template <class F>
    void bind(const std::string& name, F f) {
        if constexpr (std::is_invocable_v<F&, const TimeObject&>) {
            using R = std::decay_t<std::invoke_result_t<F&, const TimeObject&>>;
            if constexpr (std::is_same_v<R, vec2>)
                bindV2(name, [f](const TimeObject& t) { return f(t); });
            else if constexpr (std::is_same_v<R, vec>)
                bindV3(name, [f](const TimeObject& t) { return f(t); });
            else if constexpr (std::is_same_v<R, RGBA>)
                bindV4(name, [f](const TimeObject& t) { return f(t); });
            else
                bindF(name, [f](const TimeObject& t) { return float(f(t)); });
        } else {
            using R = std::decay_t<std::invoke_result_t<F&>>;
            if constexpr (std::is_same_v<R, vec2>)
                bindV2(name, [f](const TimeObject&) { return f(); });
            else if constexpr (std::is_same_v<R, vec>)
                bindV3(name, [f](const TimeObject&) { return f(); });
            else if constexpr (std::is_same_v<R, RGBA>)
                bindV4(name, [f](const TimeObject&) { return f(); });
            else
                bindF(name, [f](const TimeObject&) { return float(f()); });
        }
    }
    // Sets a live int or bool uniform. bind() uploads every scalar as a float, which a "uniform int" rejects,
    // so integers use this function.
    //   uniform int steps;   fx->bindInt("steps", [=]{ return int(n); });
    void bindInt(const std::string& name, std::function<int()> f);
    void bindInt(const std::string& name, std::function<int(const TimeObject&)> f);
    // Sets a live uniform whose number of components is known only when it is read, like a snippet variable.
    // The callable fills up to four components and returns how many. Returning 0 uploads nothing.
    void bindDynamic(const std::string& name, std::function<int(scalar*)> f);
    // Sets a live uniform from the snippet variable or parameter with the same name.
    // A bare "uniforms: - reveal" entry of a deck does this.
    // Use it for a shader that the deck did not create, for example one with an updater.
    //
    //   fx->bind("reveal");                              // uniform <- "reveal"
    //   fx->bind({"show_field", "show_basin"});          // several at once
    //
    // The number of components follows the value, from 1 to 4, so the same call works for a float and a vec3.
    // A "uniform int" still needs bindInt().
    // A name that resolves to nothing uploads nothing and gives one warning.
    void bind(const std::string& name);
    void bind(std::initializer_list<const char*> names);
    // Sets an array uniform, declared in the shader with a length known at compilation.
    //
    //   uniform float energies[64];   // in the .frag
    //   uniform int   energies_count; // optional, how many are live
    //   fx->set("energies", e);
    //
    // The upload is limited to the declared length, and <name>_count receives the number of elements written
    // if the shader declares it. Uniform storage is a few thousand floats shared by the whole shader,
    // so use this for a few hundred values at most. Use setTexture for a grid and setBuffer for larger data.
    //
    // This is for data and not for tunable values. The "controls: vec3[8]" of a deck does the opposite,
    // with one tunable parameter per element. The two must not use the same uniform,
    // because the writes per element are applied last.
    void set(const std::string& name, const std::vector<float>& v);
    void set(const std::string& name, const std::vector<vec2>& v);
    void set(const std::string& name, const std::vector<vec>& v);
    // Same, read again at every frame like bind().
    void bindArray(const std::string& name, std::function<std::vector<float>()> f);
    void bindArray(const std::string& name, std::function<std::vector<vec2>()> f);
    void bindArray(const std::string& name, std::function<std::vector<vec>()> f);
    // Removes the value of a uniform.
    void unset(const std::string& name) { uniforms.erase(name); }
    // True when a value is attached to that name. A declarative owner checks it before dropping a bind that it may not own.
    bool isBound(const std::string& name) const { return uniforms.count(name) > 0; }
    // Removes every user uniform set with set or bind. The built-ins are kept.
    // A declarative owner such as the deck loader uses it to declare all uniforms again at a reload,
    // so a uniform deleted from the deck is really removed.
    void clearUniforms() { uniforms.clear(); }

    // textures
    // Named inputs for sampling. Declare the sampler in the shader and give it a source with these functions.
    //
    //   uniform sampler2D noise;        // in the .frag
    //   uniform vec2      noise_size;   // optional, its size in pixels
    //
    //   fx->setTexture("noise", "noise.png");
    //
    // A name that the compiled program does not declare is ignored, like any other uniform.
    // The number of textures that can be bound at once is the number of texture units of the driver
    // (at least 16, usually 32), and one of them is kept for the depth buffer of the scene.
    // How a texture is sampled between texels, and outside [0,1].
    enum class Filter { Nearest,
                        Linear };
    enum class Wrap { Clamp,
                      Repeat };

    // Uses an image file as texture, loaded once. Setting the same file with the same filter and wrap again does nothing,
    // so a declarative owner can declare all its textures again at low cost (see retainTextures).
    void setTexture(const std::string& name, const path& image_file,
                    Filter f = Filter::Linear, Wrap w = Wrap::Clamp);
    // Uses the current output of another shader. That shader must be added to the slide before this one.
    void setTexture(const std::string& name, const ShaderPtr& src, int attachment = 0);
    // Uses the previous frame of this shader, with two buffers used in turn (ping-pong).
    // It is the base of iterative work on the GPU.
    void setTextureSelf(const std::string& name, int attachment = 0);
    // Removes one texture, or all of them.
    void clearTexture(const std::string& name);
    void clearTextures();
    // Removes every texture read from a file whose name is not in the list.
    // The deck loader uses it, so a texture removed from the deck is really removed
    // while the ones still declared keep their GL objects.
    // Data textures and textures between passes are kept, because they were set from code that the loader never saw.
    void retainTextures(const std::vector<std::string>& names);

    // the same, ShaderToy-style (iChannel0..3)
    // The prelude declares four numbered samplers and their sizes.
    //   uniform sampler2D iChannel0;
    //   uniform vec3      iChannelResolution[4]; // (w, h, 1) per channel
    // so a shader written for ShaderToy runs here without changes.
    // These calls are the calls above with the reserved names "iChannel0" to "iChannel3".
    // Reserved name of channel i.
    static std::string ChannelName(int i);
    void setChannel(int i, const path& image_file,
                    Filter f = Filter::Linear, Wrap w = Wrap::Clamp) { setTexture(ChannelName(i), image_file, f, w); }
    void setChannel(int i, const ShaderPtr& src, int attachment = 0) { setTexture(ChannelName(i), src, attachment); }
    void setChannelSelf(int i, int attachment = 0) { setTextureSelf(ChannelName(i), attachment); }
    void clearChannel(int i) { clearTexture(ChannelName(i)); }

    // Uses RGBA32F targets instead of 8 bits, for values that must survive many feedback iterations without banding,
    // such as accumulations and physics.
    void setFloatBuffer(bool on = true);
    // Sets the sampling of the targets of this shader. Simulations usually need Nearest and Repeat.
    void setFilter(Filter f);
    void setWrap(Wrap w);
    // Makes the shader compute only. It keeps updating but is never copied onto the slide.
    void setHidden(bool on = true);

    // the 3D scene's depth buffer
    // Lets the meshes of polyscope hide parts of a shader scene, and not only share their camera.
    // A shader that uses the depth functions of <camera.glsl> turns this on for itself at compilation.
    // Call it yourself only from a helper header.
    //
    //   #include <camera.glsl>
    //   vec3 ro, rd; polyscopeRay(ro, rd);
    //   float t = raymarch(ro, rd);
    //   if (!visibleOverScene(ro + t*rd)) discard;   // a mesh is in front
    //
    // It has a cost, because polyscope is limited to one depth peeling pass while any shader needs the depth.
    // The depth is from the current frame for a visible shader, since its rendering is delayed until after the scene pass.
    // A hidden shader reads the depth of the previous frame.
    void useSceneDepth(bool on = true);

    // the shader's own world space
    // Sets which region of the plane the shader draws. The .frag reads it as iWorld(),
    // and screen primitives can be placed at a point of this plane,
    // so a label can follow a feature that only the shader knows how to find.
    //
    //   fx->setView({0, 0}, 3.2);          // 3.2 world units above the middle
    //   show << eq->at(fx->tracker(vec2(1, 0)));   // sits on the point z = 1
    //
    // bindView lets the view move like any uniform, and the label follows.
    // The horizontal extent is half_height times the aspect ratio of the rendering,
    // so a wider window shows more and does not stretch.
    void setView(const vec2& center, scalar half_height);
    // Same, with values read every frame.
    void bindView(std::function<vec2()> center, std::function<scalar()> half_height);
    // True when a view was set.
    bool hasView() const { return bool(view_half); }

    // Same, with the two axes scaled independently, for a plot where x and y are not the same quantity.
    //
    //   fx->setViewRect({0, -1}, {10, 3});   // x in 0..10, y in -1..3
    //
    // iPixelXY() then gives the two pixel sizes and iPixel() gives the vertical one.
    void setView(const vec2& center, const vec2& half);
    void bindView(std::function<vec2()> center, std::function<vec2()> half);
    // Sets the view from its lower and upper corners, or reads them every frame.
    void setViewRect(const vec2& lo, const vec2& hi);
    void bindViewRect(std::function<std::pair<vec2, vec2>()> rect);
    // Half extent as uploaded. For a view given by a scalar, x comes from the aspect ratio.
    vec2 viewHalf() const { return resolveViewHalf(); }
    vec2 viewCenter() const { return view_center ? view_center() : vec2::Zero(); }

    // Converts a point of the plane of the shader to a window position, relative in [0,1]^2 with y down,
    // which is the space of anchors, and back.
    // It uses the view and rectangle of the last draw, so a tracked label agrees with the pixels under it.
    // Before the first draw, it uses the rectangle that the shader would occupy centered in the window.
    vec2 worldToScreen(const vec2& w) const;
    vec2 screenToWorld(const vec2& s) const;

    // Returns a placer for ScreenPrimitive::at(), so a primitive follows a point of the plane of the shader.
    // `offset` is added afterwards, in screen units, to keep the primitive away from the point.
    //   show << label->at(fx->tracker([]{ return Snippet::get("z1").v2(); }))
    std::function<vec2()> tracker(const vec2& world, const vec2& offset = vec2::Zero());
    std::function<vec2()> tracker(std::function<vec2()> world, vec2 offset = vec2::Zero());

    // multiple render targets (MRT)
    // Gives several outputs from one pass. fragColor (location 0) is already declared.
    // Add the other outputs with explicit locations, and do not declare location 0 again.
    //   layout(location = 1) out vec4 oPosition;
    // Then call setTargets(3). display() shows attachment 0.
    // setChannel(i, src, attachment) and the readback functions with an attachment reach the others. The maximum is 4.
    void setTargets(int n);
    // Number of outputs.
    int targets() const { return num_targets; }

    // upload arbitrary data as a texture
    // Uses an array from the CPU as a float texture. `comps` is the number of components per texel,
    // from 1 to 4 for R, RG, RGB or RGBA.
    // Call it again to update the texture. The GL texture is reused when the layout did not change.
    //   std::vector<float> field(w*h);
    //   fx->setTexture("field", field, w, h);
    void setTexture(const std::string& name, const float* data, int w, int h,
                    int comps = 1, Filter f = Filter::Linear, Wrap wrap = Wrap::Clamp);
    void setTexture(const std::string& name, const std::vector<float>& data,
                    int w, int h, int comps = 1,
                    Filter f = Filter::Linear, Wrap wrap = Wrap::Clamp) { setTexture(name, data.data(), w, h, comps, f, wrap); }
    // a snippet function as a texture
    // Samples a callable section on a grid and uses the result as texture.
    // A section that reads t is sampled again at every frame, and one that does not is sampled once.
    // See SnippetTexture for the cost and for changing this choice.
    //   SnippetTexture::Spec sp; sp.fn = "prior_mean"; sp.u = vec2(-6,6);
    //   fx->setTexture("prior", sp);
    void setTexture(const std::string& name, const SnippetTexture::Spec& spec,
                    Filter f = Filter::Linear, Wrap wrap = Wrap::Clamp);

    // Same, for a numbered channel.
    void setData(int i, const float* data, int w, int h, int comps = 1,
                 Filter f = Filter::Linear, Wrap wrap = Wrap::Clamp) { setTexture(ChannelName(i), data, w, h, comps, f, wrap); }
    void setData(int i, const std::vector<float>& data, int w, int h, int comps = 1,
                 Filter f = Filter::Linear, Wrap wrap = Wrap::Clamp) { setTexture(ChannelName(i), data.data(), w, h, comps, f, wrap); }

    // shader storage buffers (SSBO)
    // Large structured buffers that the shader can read and write.
    //   layout(std430, binding = 0) buffer Seeds { vec4 seed[]; };
    // fx->setBuffer(0, seeds) uploads an array from the CPU as it is.
    // Take care of the std430 packing when types are mixed, since a vec3 is aligned to 16 bytes.
    void setBuffer(int binding, const void* data, std::size_t bytes);
    template <class T>
    void setBuffer(int binding, const std::vector<T>& v) { setBuffer(binding, v.data(), v.size() * sizeof(T)); }
    // Allocates a buffer of `bytes` filled with zeros, for scratch data, output or atomic operations.
    void allocBuffer(int binding, std::size_t bytes);
    // Reads a buffer back after the shader has run. A barrier follows every draw. Returns false on failure.
    bool readBuffer(int binding, void* dst, std::size_t bytes) const;
    template <class T>
    bool readBuffer(int binding, std::vector<T>& v) const { return readBuffer(binding, v.data(), v.size() * sizeof(T)); }
    // Releases the buffer at this binding.
    void clearBuffer(int binding);
    // Sets a buffer to a value on the GPU. A pass that accumulates into it with atomics needs this at the start of every frame,
    // and it avoids the transfer that setBuffer with zeros would cost.
    void clearBufferData(int binding, unsigned int value = 0);

    // Binds the buffer that `src` holds at `src_binding` to `binding` of this shader too.
    // Two passes then share one buffer without a copy.
    // The producer must be added before the consumer (show << producer << consumer), as for setChannel.
    // A barrier after every draw makes the writes visible within the frame.
    // Both keep the buffer alive, and the last one to be dropped releases it.
    void shareBuffer(int binding, const ShaderPtr& src, int src_binding);

    // read the rendered result back
    // Reads a color attachment as RGBA floats, row by row, starting from the bottom.
    // Targets of 8 bits are given normalized to [0,1], and float targets are exact.
    // Returns false if nothing has been rendered yet.
    bool readback(std::vector<float>& out, int attachment = 0) const;
    // Mean color of an attachment.
    RGBA readbackMean(int attachment = 0) const;
    // Color of one pixel of an attachment.
    RGBA readbackPixel(int x, int y, int attachment = 0) const;
    // Size of the rendering in pixels.
    int bufferWidth() const { return res_x; }
    int bufferHeight() const { return res_y; }

    // Reads and compiles again every shader from a file whose source changed,
    // and compiles every shader again when the keyframes of the deck move.
    static void HotReloadIfModified();

    // Absolute paths of the files watched for reload, without duplicates.
    // They are the source files and the headers they include.
    static std::vector<path> WatchedFiles();

    // Size in pixels.
    vec2 getSize() const override;

    void draw(const TimeObject& t, const StateInSlide& sis) override;
    void playIntro(const TimeObject& t, const StateInSlide& sis) override;
    void playOutro(const TimeObject& t, const StateInSlide& sis) override;

protected:
    // For subclasses that generate their own source, such as Plot.
    // Installs a source and schedules the compilation. It is the part of Add() that is not static.
    // registerLive() adds the shader to the list that HotReloadIfModified goes through.
    // A factory of a subclass calls both.
    void setFragmentSource(std::string src);
    // Same from a file, which is then watched like any other shader from a file.
    void setFragmentFile(const path& file);
    // Adds the shader to the list of live shaders.
    void registerLive();
    // Gives the rectangle where this shader is drawn, in ImGui units with y down.
    // A subclass that paints over the copy needs the rectangle that the copy used.
    void screenRect(const StateInSlide& sis, ImVec2& pmin, ImVec2& pmax) const;
    // Copies the offscreen texture onto the slide. This is all the drawing of the base class.
    void display(const StateInSlide& sis, float global_alpha);

private:
    // a uniform is a closure that, given its resolved location, pushes its
    // current value, unifying fixed (set) and live (bind) uniforms
    using UniformSetter = std::function<void(int /*location*/, const TimeObject&)>;
    std::map<std::string, UniformSetter> uniforms;

    // array uniforms, clamped to the length the shader declared
    void setArray(const std::string& name, std::vector<float> data, int comps);
    void uploadArray(const std::string& name, int loc, const float* v, int count, int comps);
    int arrayCapacity(const std::string& name);
    int uniformLocation(const std::string& name);

    // .cpp-side helpers building the GL upload closures for bind()
    void bindF(const std::string& name, std::function<float(const TimeObject&)> f);
    void bindV2(const std::string& name, std::function<vec2(const TimeObject&)> f);
    void bindV3(const std::string& name, std::function<vec(const TimeObject&)> f);
    void bindV4(const std::string& name, std::function<RGBA(const TimeObject&)> f);

    std::string fragment_src;
    unsigned int program = 0; // GLuint; kept opaque to avoid a GL include here
    unsigned int vao = 0;

    // one target for a plain shader, two (ping-pong) for feedback. buf[cur]
    // always holds the latest output.
    static constexpr int kMaxTargets = 4;
    struct Target {
        unsigned int tex[kMaxTargets] = {0, 0, 0, 0};
        unsigned int fbo = 0;
    };
    Target buf[2];
    int num_targets = 1;       // number of color outputs (MRT)
    int cur = 0;               // buf[cur] = latest output
    bool feedback = false;     // some channel samples our previous frame
    bool float_buffer = false; // RGBA32F targets
    bool hidden = false;       // compute-only, updates but never blits
    unsigned int self_filter = 0x2601 /*LINEAR*/;
    unsigned int self_wrap = 0x812F /*CLAMP_TO_EDGE*/;

    // one bound texture, whatever it is sourced from
    struct Texture {
        // NB: "None" is an X11 macro, so the empty state is "Off"
        enum class Kind { Off,
                          Image,
                          ShaderOut,
                          Self } kind = Kind::Off;
        unsigned int image_tex = 0; // Kind::Image (image file *or* data texture), owned
        int w = 0, h = 0;           // size, reported through <name>_size
        int comps = 0;              // >0 when it is a float data texture
        // Kind::ShaderOut. Weak, the source may be dropped while we still
        // hold this texture.
        std::weak_ptr<Shader> src;
        int attachment = 0; // Kind::ShaderOut, which MRT output to read

        // what it was loaded from, so re-setting the same image is a no-op
        std::string file;
        Filter filter = Filter::Linear;
        Wrap wrap = Wrap::Clamp;

        // >= 0 for the four ShaderToy channels, which also feed
        // iChannelResolution[i], which a named texture has no part in
        int legacy_channel = -1;

        // resolved once per link, like every other uniform location. The
        // program they belong to is stamped so a relink re-resolves them.
        int sampler_loc = -1, size_loc = -1;
        unsigned int loc_program = 0;
        int unit = -1; // texture unit assigned at bind time
    };
    static constexpr int kChannels = 4; // how many iChannelN the prelude declares
    std::map<std::string, Texture> textures;

    // sampled snippets, re-uploaded on the frames they actually change
    struct SnippetTex {
        std::shared_ptr<SnippetTexture> tex;
        Filter filter = Filter::Linear;
        Wrap wrap = Wrap::Clamp;
    };
    std::map<std::string, SnippetTex> snippet_textures;
    void refreshSnippetTextures();

    // polyscope's depth buffer goes on the unit just past the textures, so it
    // depends on how many are bound this frame
    int scene_depth_unit = kChannels;
    bool bound_scene_depth = false;
    // how many units were handed out on the last draw, to unbind exactly those
    int bound_units = 0;
    bool wants_scene_depth = false; // useSceneDepth()

    // Number of live shaders that asked for the scene depth. The original number of peeling passes is restored
    // when the last one goes away.
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
    struct StorageBuffer {
        unsigned int id = 0;
        std::size_t bytes = 0;
    };
    using StorageBufferPtr = std::shared_ptr<StorageBuffer>;
    std::map<int, StorageBufferPtr> ssbos;

    // uniform locations, resolved once per link rather than per frame
    // (glGetUniformLocation is a string lookup, ~250ns each)
    struct BuiltinLocs {
        int iResolution = -1, iAspect = -1, iTime = -1, iTimeDelta = -1;
        int iFrame = -1, iFrameRate = -1;
        int from_begin = -1, from_action = -1, inner_time = -1, delta_time = -1;
        int absolute_frame_number = -1, relative_frame_number = -1;
        int transition_parameter = -1, slide_progress = -1;
        int iSlideTime = -1; // float[KF_SLIDE_COUNT], for secondsSinceKeyframe
        int iView = -1, iViewInv = -1, iProj = -1, iProjInv = -1;
        int iCamPos = -1, iCamFov = -1, iScreenRect = -1, iWindowSize = -1;
        int iViewCenter = -1, iViewHalf = -1;
        int iSceneDepth = -1, iSceneDepthValid = -1, iSceneDepthSize = -1;
        int iMouse = -1, iMouseNorm = -1, iHovered = -1, iDate = -1;
        // The samplers are resolved for each texture, since they are named at runtime.
        // Only the ShaderToy resolution array is a fixed built-in.
        int iChannelRes[kChannels] = {-1, -1, -1, -1};
    };
    BuiltinLocs uloc;
    // user uniforms are named at runtime, so these fill in lazily
    std::map<std::string, int> user_uniform_loc;
    // declared length of an array uniform, resolved per link
    std::map<std::string, int> array_capacity;
    std::set<std::string> array_overflow_said;
    void cacheUniformLocations();

    int res_x = int(Options::ScreenResolutionWidth), res_y = int(Options::ScreenResolutionHeight);
    // false at the screen-size default, getSize() then shows it at that size
    // 1:1, rather than through the 1920x1080-relative scaling an explicit
    // resolution goes through
    bool explicit_resolution = false;
    bool gl_ready = false; // resources created (needs a live context)
    bool compiled = false; // last compile succeeded
    bool needs_recompile = true;

    // mouse click latch, for iMouse.zw (position of the last press over the rect)
    float last_click_x = 0, last_click_y = 0;
    bool mouse_was_down = false;

    // iFrame, how many times *this* shader has rendered, distinct from the
    // slide index (iSlide)
    int frames_rendered = 0;

    // the texture display()/downstream should read (color attachment `a`)
    unsigned int currentTexture(int a = 0) const { return buf[cur].tex[a]; }

    // File backing, used by the reload like in Code.
    // source_path is what the caller asked for, and source_file is the same path resolved from the project data path.
    // That path is known only once the deck is initialized, so it is resolved again at every reload.
    path source_path;
    path source_file;
    std::filesystem::file_time_type last_modified;
    bool from_file = false;
    bool load_failed = false;         // reading failed, retried on every tick
    bool load_error_reported = false; // ... but logged once, not 5x a second

    // Files reached through #include when the source was last expanded, with their timestamps.
    // The reload watches them too.
    std::vector<std::pair<std::string, std::filesystem::file_time_type>> include_deps;
    // File of each source string index, to make the "N:line" of a failed compile readable.
    std::vector<std::string> source_units;

    // world space (setView)
    std::function<vec2()> view_center;
    std::function<vec2()> view_half;
    // set by the scalar setView/bindView, whose width comes from the aspect
    bool view_x_from_aspect = true;
    vec2 resolveViewHalf() const;
    // What was last uploaded to iViewCenter/iViewHalf, and the rect it was
    // drawn into (window relative, y down). worldToScreen inverts these rather
    // than re-reading the callables, so it cannot disagree with the image.
    mutable vec2 drawn_view_center = vec2::Zero();
    mutable vec2 drawn_view_half = vec2(1, 1);
    mutable vec2 drawn_rect_min = vec2::Zero(), drawn_rect_max = vec2(1, 1);
    mutable bool rect_recorded = false;
    bool bad_view_reported = false; // a degenerate view is said once, not per frame

    void ensureResources(); // lazy GL init, on first draw
    void recompile();
    void renderToTexture(const TimeObject& t, const StateInSlide& sis);
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
        TimeObject time;
        StateInSlide sis;
        ImVec2 pmin, pmax;
    };
    std::vector<PendingRender> pending;
    std::size_t pending_next = 0;
    int record_frame = -1;              // ImGui frame the queue was built for
    bool render_error_reported = false; // throttles the exception log to once

    // the rect the callback is currently rendering for, taken from its job
    ImVec2 pending_pmin, pending_pmax;
    bool use_pending_rect = false;

    // every live Shader, so keyframe/#include invalidation reaches all of them
    inline static std::vector<Shader*> all_shaders;
};

} // namespace slope

#endif // SHADER_H
