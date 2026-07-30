#include "Shader.h"
#include "GLFW/glfw3.h"
#include "polyscope/view.h"
#include "polyscope/polyscope.h"
#include "polyscope/render/engine.h"
#include <glm/gtc/matrix_inverse.hpp>
#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>
#include <cmath>
#include <ctime>
#include <cstdio>
#include <cstddef>
#include <cctype>
#include <set>
#include "../../extern/stb_image.h"

namespace slope {

// Minimal, self-contained GL loader : rather than depend on whether glad is on
// the include path, we resolve the handful of entry points we need through
// glfwGetProcAddress (GLFW is already initialised by polyscope by then).
namespace {

// GL types (identical redefinitions are harmless if a gl.h is already included)
typedef unsigned int SLGLenum;
typedef unsigned int SLGLuint;
typedef int          SLGLint;
typedef int          SLGLsizei;
typedef float        SLGLfloat;
typedef char         SLGLchar;
typedef std::ptrdiff_t SLGLsizeiptr;   // GLsizeiptr
typedef std::ptrdiff_t SLGLintptr;     // GLintptr

// constants (2.0+ ones a classic gl.h would not define)
constexpr SLGLenum SL_FRAGMENT_SHADER      = 0x8B30;
constexpr SLGLenum SL_VERTEX_SHADER        = 0x8B31;
constexpr SLGLenum SL_COMPILE_STATUS       = 0x8B81;
constexpr SLGLenum SL_LINK_STATUS          = 0x8B82;
constexpr SLGLenum SL_INFO_LOG_LENGTH      = 0x8B84;
constexpr SLGLenum SL_FRAMEBUFFER          = 0x8D40;
constexpr SLGLenum SL_COLOR_ATTACHMENT0    = 0x8CE0;
constexpr SLGLenum SL_FRAMEBUFFER_COMPLETE = 0x8CD5;
constexpr SLGLenum SL_FRAMEBUFFER_BINDING  = 0x8CA6;
constexpr SLGLenum SL_CURRENT_PROGRAM      = 0x8B8D;
constexpr SLGLenum SL_VERTEX_ARRAY_BINDING = 0x85B5;
constexpr SLGLenum SL_TEXTURE_2D           = 0x0DE1;
constexpr SLGLenum SL_TEXTURE_BINDING_2D   = 0x8069;
constexpr SLGLenum SL_TEXTURE_MIN_FILTER   = 0x2801;
constexpr SLGLenum SL_TEXTURE_MAG_FILTER   = 0x2800;
constexpr SLGLenum SL_TEXTURE_WRAP_S       = 0x2802;
constexpr SLGLenum SL_TEXTURE_WRAP_T       = 0x2803;
constexpr SLGLenum SL_LINEAR               = 0x2601;
constexpr SLGLenum SL_CLAMP_TO_EDGE        = 0x812F;
constexpr SLGLenum SL_RED                  = 0x1903;
constexpr SLGLenum SL_RG                   = 0x8227;
constexpr SLGLenum SL_RGB                  = 0x1907;
constexpr SLGLenum SL_RGBA                 = 0x1908;
constexpr SLGLenum SL_R32F                 = 0x822E;
constexpr SLGLenum SL_RG32F                = 0x8230;
constexpr SLGLenum SL_RGB32F               = 0x8815;
constexpr SLGLenum SL_RGBA32F              = 0x8814;
constexpr SLGLenum SL_UNSIGNED_BYTE        = 0x1401;
constexpr SLGLenum SL_FLOAT                = 0x1406;
constexpr SLGLenum SL_NEAREST              = 0x2600;
constexpr SLGLenum SL_REPEAT               = 0x2901;
constexpr SLGLenum SL_TEXTURE0             = 0x84C0;
constexpr SLGLenum SL_COLOR_BUFFER_BIT     = 0x00004000;
constexpr SLGLenum SL_TRIANGLES            = 0x0004;
constexpr SLGLenum SL_VIEWPORT             = 0x0BA2;
constexpr SLGLenum SL_DEPTH_TEST           = 0x0B71;
constexpr SLGLenum SL_SHADER_STORAGE_BUFFER      = 0x90D2;
constexpr SLGLenum SL_SHADER_STORAGE_BARRIER_BIT = 0x00002000;
constexpr SLGLenum SL_DYNAMIC_DRAW               = 0x88E8;
constexpr SLGLenum SL_MAJOR_VERSION              = 0x821B;
constexpr SLGLenum SL_MINOR_VERSION              = 0x821C;
constexpr SLGLenum SL_COLOR_CLEAR_VALUE          = 0x0C22;

struct GL {
    // shaders / programs
    SLGLuint (*CreateShader)(SLGLenum) = nullptr;
    void (*ShaderSource)(SLGLuint, SLGLsizei, const SLGLchar* const*, const SLGLint*) = nullptr;
    void (*CompileShader)(SLGLuint) = nullptr;
    void (*GetShaderiv)(SLGLuint, SLGLenum, SLGLint*) = nullptr;
    void (*GetShaderInfoLog)(SLGLuint, SLGLsizei, SLGLsizei*, SLGLchar*) = nullptr;
    void (*DeleteShader)(SLGLuint) = nullptr;
    SLGLuint (*CreateProgram)() = nullptr;
    void (*AttachShader)(SLGLuint, SLGLuint) = nullptr;
    void (*LinkProgram)(SLGLuint) = nullptr;
    void (*GetProgramiv)(SLGLuint, SLGLenum, SLGLint*) = nullptr;
    void (*GetProgramInfoLog)(SLGLuint, SLGLsizei, SLGLsizei*, SLGLchar*) = nullptr;
    void (*DeleteProgram)(SLGLuint) = nullptr;
    void (*UseProgram)(SLGLuint) = nullptr;
    SLGLint (*GetUniformLocation)(SLGLuint, const SLGLchar*) = nullptr;
    void (*Uniform1f)(SLGLint, SLGLfloat) = nullptr;
    void (*Uniform2f)(SLGLint, SLGLfloat, SLGLfloat) = nullptr;
    void (*Uniform3f)(SLGLint, SLGLfloat, SLGLfloat, SLGLfloat) = nullptr;
    void (*Uniform4f)(SLGLint, SLGLfloat, SLGLfloat, SLGLfloat, SLGLfloat) = nullptr;
    void (*Uniform1i)(SLGLint, SLGLint) = nullptr;
    void (*UniformMatrix4fv)(SLGLint, SLGLsizei, unsigned char, const SLGLfloat*) = nullptr;
    // vertex arrays
    void (*GenVertexArrays)(SLGLsizei, SLGLuint*) = nullptr;
    void (*BindVertexArray)(SLGLuint) = nullptr;
    void (*DeleteVertexArrays)(SLGLsizei, const SLGLuint*) = nullptr;
    // framebuffers
    void (*GenFramebuffers)(SLGLsizei, SLGLuint*) = nullptr;
    void (*BindFramebuffer)(SLGLenum, SLGLuint) = nullptr;
    void (*FramebufferTexture2D)(SLGLenum, SLGLenum, SLGLenum, SLGLuint, SLGLint) = nullptr;
    void (*DeleteFramebuffers)(SLGLsizei, const SLGLuint*) = nullptr;
    SLGLenum (*CheckFramebufferStatus)(SLGLenum) = nullptr;
    void (*DrawBuffers)(SLGLsizei, const SLGLenum*) = nullptr;
    void (*ReadBuffer)(SLGLenum) = nullptr;
    // textures / fixed pipeline state (also fetched through the loader, so we
    // never rely on these being directly linked)
    void (*GenTextures)(SLGLsizei, SLGLuint*) = nullptr;
    void (*BindTexture)(SLGLenum, SLGLuint) = nullptr;
    void (*ActiveTexture)(SLGLenum) = nullptr;
    void (*TexImage2D)(SLGLenum, SLGLint, SLGLint, SLGLsizei, SLGLsizei, SLGLint,
                       SLGLenum, SLGLenum, const void*) = nullptr;
    void (*TexSubImage2D)(SLGLenum, SLGLint, SLGLint, SLGLint, SLGLsizei, SLGLsizei,
                          SLGLenum, SLGLenum, const void*) = nullptr;
    void (*TexParameteri)(SLGLenum, SLGLenum, SLGLint) = nullptr;
    void (*DeleteTextures)(SLGLsizei, const SLGLuint*) = nullptr;
    void (*ReadPixels)(SLGLint, SLGLint, SLGLsizei, SLGLsizei, SLGLenum, SLGLenum, void*) = nullptr;
    void (*Viewport)(SLGLint, SLGLint, SLGLsizei, SLGLsizei) = nullptr;
    void (*DrawArrays)(SLGLenum, SLGLint, SLGLsizei) = nullptr;
    void (*Clear)(SLGLenum) = nullptr;
    void (*ClearColor)(SLGLfloat, SLGLfloat, SLGLfloat, SLGLfloat) = nullptr;
    void (*GetIntegerv)(SLGLenum, SLGLint*) = nullptr;
    void (*GetFloatv)(SLGLenum, SLGLfloat*) = nullptr;
    void (*Enable)(SLGLenum) = nullptr;
    void (*Disable)(SLGLenum) = nullptr;
    unsigned char (*IsEnabled)(SLGLenum) = nullptr;
    // buffers (SSBO)
    void (*GenBuffers)(SLGLsizei, SLGLuint*) = nullptr;
    void (*BindBuffer)(SLGLenum, SLGLuint) = nullptr;
    void (*BufferData)(SLGLenum, SLGLsizeiptr, const void*, SLGLenum) = nullptr;
    void (*BufferSubData)(SLGLenum, SLGLintptr, SLGLsizeiptr, const void*) = nullptr;
    void (*BindBufferBase)(SLGLenum, SLGLuint, SLGLuint) = nullptr;
    void (*GetBufferSubData)(SLGLenum, SLGLintptr, SLGLsizeiptr, void*) = nullptr;
    void (*DeleteBuffers)(SLGLsizei, const SLGLuint*) = nullptr;
    void (*MemoryBarrier)(SLGLenum) = nullptr;

    bool ok = false;      // every *required* entry point resolved
    int  major = 0, minor = 0;   // context version, for the #version we emit
};

template<class Fn>
bool loadOne(Fn& slot, const char* name, bool required)
{
    slot = reinterpret_cast<Fn>(glfwGetProcAddress(name));
    if (!slot && required)
        spdlog::error("[shader] could not resolve GL entry point {}", name);
    return slot != nullptr;
}

GL& gl()
{
    static GL g;
    static bool tried = false;
    if (tried)
        return g;
    tried = true;
    bool ok = true;
    // L  : required, its absence disables the whole primitive
    // LO : optional (a feature newer than the guaranteed 3.3 baseline), guarded
    //      at every call site so the rest of the primitive keeps working
#define L(fn, name)  ok &= loadOne(g.fn, name, true)
#define LO(fn, name) loadOne(g.fn, name, false)
    L(CreateShader,"glCreateShader"); L(ShaderSource,"glShaderSource");
    L(CompileShader,"glCompileShader"); L(GetShaderiv,"glGetShaderiv");
    L(GetShaderInfoLog,"glGetShaderInfoLog"); L(DeleteShader,"glDeleteShader");
    L(CreateProgram,"glCreateProgram"); L(AttachShader,"glAttachShader");
    L(LinkProgram,"glLinkProgram"); L(GetProgramiv,"glGetProgramiv");
    L(GetProgramInfoLog,"glGetProgramInfoLog"); L(DeleteProgram,"glDeleteProgram");
    L(UseProgram,"glUseProgram"); L(GetUniformLocation,"glGetUniformLocation");
    L(Uniform1f,"glUniform1f"); L(Uniform2f,"glUniform2f");
    L(Uniform3f,"glUniform3f"); L(Uniform4f,"glUniform4f"); L(Uniform1i,"glUniform1i");
    L(UniformMatrix4fv,"glUniformMatrix4fv");
    L(GenVertexArrays,"glGenVertexArrays"); L(BindVertexArray,"glBindVertexArray");
    L(DeleteVertexArrays,"glDeleteVertexArrays");
    L(GenFramebuffers,"glGenFramebuffers"); L(BindFramebuffer,"glBindFramebuffer");
    L(FramebufferTexture2D,"glFramebufferTexture2D");
    L(DeleteFramebuffers,"glDeleteFramebuffers");
    L(CheckFramebufferStatus,"glCheckFramebufferStatus");
    L(DrawBuffers,"glDrawBuffers"); L(ReadBuffer,"glReadBuffer");
    L(GenTextures,"glGenTextures"); L(BindTexture,"glBindTexture");
    L(ActiveTexture,"glActiveTexture");
    L(TexImage2D,"glTexImage2D"); L(TexSubImage2D,"glTexSubImage2D");
    L(TexParameteri,"glTexParameteri");
    L(DeleteTextures,"glDeleteTextures"); L(ReadPixels,"glReadPixels");
    L(Viewport,"glViewport"); L(DrawArrays,"glDrawArrays");
    L(Clear,"glClear"); L(ClearColor,"glClearColor"); L(GetIntegerv,"glGetIntegerv");
    L(GetFloatv,"glGetFloatv");
    L(Enable,"glEnable"); L(Disable,"glDisable"); L(IsEnabled,"glIsEnabled");
    L(GenBuffers,"glGenBuffers"); L(BindBuffer,"glBindBuffer");
    L(BufferData,"glBufferData"); L(BufferSubData,"glBufferSubData");
    L(BindBufferBase,"glBindBufferBase"); L(GetBufferSubData,"glGetBufferSubData");
    L(DeleteBuffers,"glDeleteBuffers");
    LO(MemoryBarrier,"glMemoryBarrier");   // GL 4.2 : absent on a 3.3/4.1 context
#undef LO
#undef L
    g.ok = ok;
    if (ok) {
        g.GetIntegerv(SL_MAJOR_VERSION, &g.major);
        g.GetIntegerv(SL_MINOR_VERSION, &g.minor);
    }
    return g;
}

// GLSL version the prelude declares. polyscope asks GLFW for 3.3 core;
// drivers hand back newer (SSBOs available), but macOS caps at 4.1 where
// "#version 430" fails to compile, so ask the context rather than assume.
const char* versionLine()
{
    auto& g = gl();
    const bool has430 = (g.major > 4) || (g.major == 4 && g.minor >= 3);
    return has430 ? "#version 430 core\n" : "#version 330 core\n";
}

// a fullscreen triangle, no vertex buffer needed (versionLine() prepended at
// compile time)
const char* kVertexBody = R"glsl(
void main() {
    vec2 v[3] = vec2[3](vec2(-1.0,-1.0), vec2(3.0,-1.0), vec2(-1.0,3.0));
    gl_Position = vec4(v[gl_VertexID], 0.0, 1.0);
}
)glsl";

// Everything here is core 3.3; 430 is used only when the context offers it
// (see versionLine()), letting shaders declare SSBOs.
const char* kFragPreludeBody = R"glsl(
uniform vec2  iResolution;   // render target size, in pixels
uniform float iAspect;       // iResolution.x / iResolution.y
uniform float iTime;         // seconds since the primitive appeared
uniform float iTimeDelta;    // seconds since last frame
uniform int   iFrame;        // frames this shader has rendered
uniform float iFrameRate;    // frames per second (smoothed)
// the primitive's TimeObject, field for field, under its own C++ names
uniform float from_begin;             // seconds since the slideshow started
uniform float from_action;            // seconds since the last slide change
uniform float inner_time;             // seconds since this primitive appeared (= iTime)
uniform float delta_time;             // seconds since last frame (= iTimeDelta)
uniform int   absolute_frame_number;  // current slide index in the deck
uniform int   relative_frame_number;  // slides elapsed since this shader appeared
uniform float transition_parameter;   // 0 -> 1 across the current intro / outro
// polyscope's camera. iScreenRect is this shader's rect in the window (x, y,
// w, h, ImGui display units, y down), letting a fragment map back to a pixel.
uniform mat4  iView;         // world -> camera
uniform mat4  iViewInv;      // camera -> world
uniform mat4  iProj;         // camera -> clip
uniform mat4  iProjInv;      // clip -> camera
uniform vec3  iCamPos;       // camera position, world space
uniform float iCamFov;       // vertical field of view, degrees
uniform vec4  iScreenRect;   // this shader's rectangle in the window
uniform vec2  iWindowSize;   // the window, same units as iScreenRect
uniform sampler2D iSceneDepth;    // polyscope's depth buffer for the 3D scene
uniform float iSceneDepthValid;   // 1.0 when iSceneDepth holds a rendered scene
uniform vec2  iSceneDepthSize;    // its resolution, in framebuffer pixels
uniform vec4  iMouse;        // xy = cursor (px, y up); zw = last click, z<0 when not pressed
uniform vec2  iMouseNorm;    // cursor in 0..1 across the rect, y up
uniform float iHovered;      // 1.0 while the cursor is over the rect, else 0.0
uniform vec4  iDate;         // year, month(1-12), day(1-31), seconds since midnight
uniform sampler2D iChannel0; // texture inputs : image, another shader, or
uniform sampler2D iChannel1; //   this shader's previous frame (feedback).
uniform sampler2D iChannel2; //   sample with texture(iChannelN, uv).
uniform sampler2D iChannel3;
uniform vec3  iChannelResolution[4]; // (width, height, 1) of each channel
layout(location = 0) out vec4 fragColor;  // output 0 (MRT: add location=1,2,...)
// the TimeObject's keyframe queries, same names, taking a keyframe #define
bool afterKeyframe (int kf) { return absolute_frame_number >= kf; }
bool beforeKeyframe(int kf) { return absolute_frame_number <  kf; }
bool atKeyframe    (int kf) { return absolute_frame_number == kf; }
int  slidesSinceKeyframe(int kf) { return absolute_frame_number - kf; }
)glsl";

// Keyframes are name -> slide index, emitted as a #define so a shader can say
// afterKeyframe(reveal) instead of a hardcoded slide number. Names that are
// not valid GLSL identifiers, or that would shadow a prelude name, are
// skipped (warned once) rather than breaking every shader's compile.
bool usableKeyframeName(const std::string& n)
{
    static const std::set<std::string> taken = {
        "iResolution","iAspect","iTime","iTimeDelta","iFrame","iFrameRate",
        "iMouse","iMouseNorm","iHovered","iDate","iChannelResolution",
        "iChannel0","iChannel1","iChannel2","iChannel3","fragColor","main",
        "from_begin","from_action","inner_time","delta_time",
        "absolute_frame_number","relative_frame_number","transition_parameter",
        "afterKeyframe","beforeKeyframe","atKeyframe","slidesSinceKeyframe",
    };
    if (n.empty() || taken.count(n)) return false;
    if (!std::isalpha((unsigned char)n[0]) && n[0] != '_') return false;
    for (char c : n)
        if (!std::isalnum((unsigned char)c) && c != '_') return false;
    return n.compare(0, 3, "gl_") != 0;
}

std::string keyframeDefines()
{
    std::string out;
    if (!TimeObject::keyframes)
        return out;
    static std::set<std::string> warned;
    for (const auto& [name, slide] : *TimeObject::keyframes) {
        if (!usableKeyframeName(name)) {
            if (warned.insert(name).second)
                spdlog::warn("[shader] keyframe \"{}\" is not usable as a GLSL "
                             "identifier and is not exposed to shaders", name);
            continue;
        }
        out += "#define " + name + " " + std::to_string(slide) + "\n";
    }
    return out;
}

// source string 0 is the shader's own file; #include'd files get 1, 2, ...
const char* kLineReset = "#line 1 0\n";

// #include "other.frag" (or <other.frag>) : plain textual inclusion, resolved
// against the including file then the project data path. Each included file
// gets its own source-string index, tracked via "#line n idx" so diagnostics
// point at the right line; "#pragma once" is honoured and stripped.
struct IncludeExpansion {
    std::string source;
    std::vector<std::pair<std::string, std::filesystem::file_time_type>> deps;
    std::vector<std::string> units;   // source-string index -> file
    std::set<std::string> once;       // files that asked not to be re-expanded
};

bool readWholeFile(const std::string& file, std::string& out)
{
    std::ifstream f(file);
    if (!f.is_open())
        return false;
    std::stringstream b;
    b << f.rdbuf();
    out = b.str();
    return true;
}

std::vector<std::string> splitLines(const std::string& src)
{
    std::vector<std::string> lines;
    size_t i = 0;
    while (true) {
        size_t e = src.find('\n', i);
        if (e == std::string::npos) { lines.push_back(src.substr(i)); break; }
        lines.push_back(src.substr(i, e - i));
        i = e + 1;
    }
    return lines;
}

// the file an #include resolves to, or "" : <file> is the shader stdlib,
// "file" is the deck's own, falling back to the stdlib.
std::string resolveInclude(const std::string& target, const path& dir, bool bank_only)
{
    std::error_code ec;
    auto tryPath = [&ec](const path& p) -> std::string {
        return std::filesystem::exists(p, ec) ? p.string() : std::string();
    };
    if (path(target).is_absolute())
        return tryPath(target);
    if (!bank_only) {
        if (auto p = tryPath(dir / target); !p.empty())
            return p;
        if (auto p = tryPath(formatPath(target)); !p.empty())
            return p;
    }
    return tryPath(path(Options::ShaderPath) / target);
}

// "#include "x"" -> x, "" if the line is not a well-formed include.
// `bank_only` reports the <angle-bracket> form.
std::string parseIncludeTarget(const std::string& rest, const std::string& where, int line,
                               bool& bank_only)
{
    bank_only = false;
    size_t i = 0;
    while (i < rest.size() && (rest[i] == ' ' || rest[i] == '\t')) ++i;
    if (i >= rest.size() || (rest[i] != '"' && rest[i] != '<')) {
        spdlog::error("[shader] {}:{}: #include expects \"file\" or <file>", where, line);
        return "";
    }
    bank_only = (rest[i] == '<');
    const char close = bank_only ? '>' : '"';
    size_t end = rest.find(close, i + 1);
    if (end == std::string::npos) {
        spdlog::error("[shader] {}:{}: unterminated #include", where, line);
        return "";
    }
    return rest.substr(i + 1, end - i - 1);
}

void expandInto(const std::string& src, const path& dir, const std::string& self,
                int unit, std::vector<std::string>& stack, IncludeExpansion& X, int depth)
{
    static constexpr int kMaxDepth = 32;
    const auto lines = splitLines(src);
    for (size_t k = 0; k < lines.size(); ++k) {
        const std::string& line = lines[k];
        size_t j = 0;
        while (j < line.size() && (line[j] == ' ' || line[j] == '\t')) ++j;

        if (line.compare(j, 12, "#pragma once") == 0) {
            X.once.insert(self);
            X.source += "\n";           // keep the parent's line numbering
            continue;
        }
        if (line.compare(j, 8, "#include") != 0) {
            X.source += line;
            X.source += "\n";
            continue;
        }

        // a blank line stands in for every include we do not expand, so the
        // parent's numbering survives without an extra #line directive
        X.source += "\n";
        bool bank_only = false;
        const std::string target =
            parseIncludeTarget(line.substr(j + 8), self, int(k) + 1, bank_only);
        if (target.empty())
            continue;
        const std::string file = resolveInclude(target, dir, bank_only);
        if (file.empty()) {
            spdlog::error("[shader] {}:{}: cannot find #include {} (stdlib is {})",
                          self, k + 1,
                          bank_only ? "<" + target + ">" : "\"" + target + "\"",
                          Options::ShaderPath);
            continue;
        }
        std::error_code ec;
        std::string canon = std::filesystem::weakly_canonical(file, ec).string();
        if (ec || canon.empty())
            canon = file;

        if (X.once.count(canon))
            continue;                   // already pulled in, and asked to be once
        if (std::find(stack.begin(), stack.end(), canon) != stack.end()) {
            spdlog::error("[shader] {}:{}: circular #include of \"{}\"", self, k + 1, target);
            continue;
        }
        if (depth >= kMaxDepth) {
            spdlog::error("[shader] #include nested deeper than {} levels, giving up "
                          "at \"{}\"", kMaxDepth, target);
            continue;
        }
        std::string content;
        if (!readWholeFile(canon, content)) {
            spdlog::error("[shader] could not read #include \"{}\"", canon);
            continue;
        }

        const int sub = int(X.units.size());
        X.units.push_back(canon);
        auto stamp = std::filesystem::last_write_time(canon, ec);
        if (!ec)
            X.deps.emplace_back(canon, stamp);

        X.source += "#line 1 " + std::to_string(sub) + "\n";
        stack.push_back(canon);
        expandInto(content, path(canon).parent_path(), canon, sub, stack, X, depth + 1);
        stack.pop_back();
        // resume the parent, on the line after the #include
        X.source += "#line " + std::to_string(k + 2) + " " + std::to_string(unit) + "\n";
    }
}

IncludeExpansion expandIncludes(const std::string& src, const path& dir,
                                const std::string& self)
{
    IncludeExpansion X;
    X.units.push_back(self);
    std::vector<std::string> stack{self};
    expandInto(src, dir, self, 0, stack, X, 0);
    return X;
}

// true if the source has a genuine "#version" directive : the first token,
// ignoring blanks/comments, on some line is "#version" — so a comment merely
// mentioning the word does not fool us into dropping the prelude.
bool hasVersionDirective(const std::string& src)
{
    const size_t n = src.size();
    bool in_block = false;   // inside a /* ... */
    bool in_line  = false;   // inside a // ..., until the newline
    bool fresh    = true;    // nothing but blanks/comments since the last newline
    for (size_t i = 0; i < n; ++i) {
        const char c = src[i];
        if (c == '\n') { in_line = false; fresh = true; continue; }
        if (in_line) continue;
        if (in_block) {                       // a "#version" in here is just prose
            if (c == '*' && i + 1 < n && src[i+1] == '/') { in_block = false; ++i; }
            continue;
        }
        if (c == '/' && i + 1 < n) {
            // comments do not consume "freshness" : the spec lets a real
            // directive be preceded by them (/* header */ #version 330)
            if (src[i+1] == '/') { in_line  = true; ++i; continue; }
            if (src[i+1] == '*') { in_block = true; ++i; continue; }
        }
        if (c == ' ' || c == '\t' || c == '\r') continue;
        if (fresh && src.compare(i, 8, "#version") == 0)
            return true;
        fresh = false;                        // a real token opened this line
    }
    return false;
}

SLGLuint compileStage(SLGLenum type, const std::string& src, const char* label)
{
    auto& g = gl();
    SLGLuint s = g.CreateShader(type);
    const SLGLchar* p = src.c_str();
    g.ShaderSource(s, 1, &p, nullptr);
    g.CompileShader(s);
    SLGLint ok = 0;
    g.GetShaderiv(s, SL_COMPILE_STATUS, &ok);
    if (!ok) {
        SLGLint len = 0;
        g.GetShaderiv(s, SL_INFO_LOG_LENGTH, &len);
        std::string log(std::max(len, 1), '\0');
        g.GetShaderInfoLog(s, len, nullptr, log.data());
        // surfaced to the terminal so a live shader edit that fails to compile
        // is visible immediately (the slide keeps the last good program)
        fprintf(stderr, "[shader] %s compile failed:\n%s\n", label, log.c_str());
        g.DeleteShader(s);
        return 0;
    }
    return s;
}

// load an image file into a GL texture (flipped to y-up, so texture(iChannelN,
// uv) with uv.y=0 samples the bottom row, like ShaderToy)
SLGLuint loadImageTexture(const std::string& file, SLGLenum filter, SLGLenum wrap,
                          int& w, int& h)
{
    auto& g = gl();
    if (!g.ok)
        return 0;
    int comp = 0;
    stbi_set_flip_vertically_on_load(1);
    unsigned char* px = stbi_load(file.c_str(), &w, &h, &comp, 4);
    stbi_set_flip_vertically_on_load(0); // restore the global (shared with Image)
    if (!px) {
        spdlog::error("[shader] channel image not found: {}", file);
        return 0;
    }
    SLGLint prev_tex = 0;
    g.GetIntegerv(SL_TEXTURE_BINDING_2D, &prev_tex);
    SLGLuint tex = 0;
    g.GenTextures(1, &tex);
    g.BindTexture(SL_TEXTURE_2D, tex);
    g.TexImage2D(SL_TEXTURE_2D, 0, SL_RGBA, w, h, 0, SL_RGBA, SL_UNSIGNED_BYTE, px);
    g.TexParameteri(SL_TEXTURE_2D, SL_TEXTURE_MIN_FILTER, SLGLint(filter));
    g.TexParameteri(SL_TEXTURE_2D, SL_TEXTURE_MAG_FILTER, SLGLint(filter));
    g.TexParameteri(SL_TEXTURE_2D, SL_TEXTURE_WRAP_S, SLGLint(wrap));
    g.TexParameteri(SL_TEXTURE_2D, SL_TEXTURE_WRAP_T, SLGLint(wrap));
    g.BindTexture(SL_TEXTURE_2D, SLGLuint(prev_tex));
    stbi_image_free(px);
    return tex;
}

} // namespace

ShaderPtr Shader::Add(const std::string& fragment_source, int w, int h)
{
    auto s = NewPrimitive<Shader>();
    s->fragment_src = fragment_source;
    s->needs_recompile = true;
    all_shaders.push_back(s.get());
    if (w > 0 && h > 0)
        s->setResolution(w, h);
    return s;
}

ShaderPtr Shader::FromFile(const path& file, int w, int h)
{
    auto s = NewPrimitive<Shader>();
    s->source_file = formatPath(file);
    s->from_file = true;
    s->reloadFromFile();
    all_shaders.push_back(s.get());
    if (w > 0 && h > 0)
        s->setResolution(w, h);
    return s;
}

void Shader::reloadFromFile()
{
    std::ifstream f(source_file);
    if (!f.is_open()) {
        spdlog::error("[shader] could not open {}", source_file.string());
        fragment_src = "void main(){ fragColor = vec4(1,0,1,1); }"; // loud magenta
    } else {
        std::stringstream buf;
        buf << f.rdbuf();
        fragment_src = buf.str();
    }
    needs_recompile = true;
    std::error_code ec;
    auto t = std::filesystem::last_write_time(source_file, ec);
    if (!ec)
        last_modified = t;
}

Shader::~Shader()
{
    // GL objects are intentionally not freed here : a Shader can outlive the GL
    // context (static teardown ordering), and the driver reclaims them then.
    auto ia = std::find(all_shaders.begin(), all_shaders.end(), this);
    if (ia != all_shaders.end())
        all_shaders.erase(ia);

    if (wants_scene_depth) {
        --scene_depth_users;
        applySceneDepthMode();
    }
}

// Does this source use the scene-depth API? Whole identifiers only, blind to
// comments, and scanned on the shader's own text (not the expanded #include
// tree) so that merely including <camera.glsl> does not switch this on deck-wide.
bool Shader::referencesSceneDepth(const std::string& src)
{
    static const char* kNames[] = {
        "iSceneDepth", "iSceneDepthValid", "iSceneDepthSize",
        "sceneDepthHere", "visibleOverScene", "sceneEyeDistance",
        "sceneClearance", "sceneOcclusion", "sceneWorldPos",
    };
    auto ident = [](char c) { return std::isalnum((unsigned char)c) || c == '_'; };

    // strip comments first, so a mention in prose does not count
    std::string code;
    code.reserve(src.size());
    for (std::size_t i = 0; i < src.size(); ++i) {
        if (src[i] == '/' && i + 1 < src.size() && src[i + 1] == '/') {
            while (i < src.size() && src[i] != '\n') ++i;
            code.push_back('\n');
        } else if (src[i] == '/' && i + 1 < src.size() && src[i + 1] == '*') {
            i += 2;
            while (i + 1 < src.size() && !(src[i] == '*' && src[i + 1] == '/')) ++i;
            ++i;
            code.push_back(' ');
        } else {
            code.push_back(src[i]);
        }
    }

    for (const char* n : kNames) {
        const std::size_t len = std::strlen(n);
        for (std::size_t p = code.find(n); p != std::string::npos;
             p = code.find(n, p + 1)) {
            const bool left  = p > 0 && ident(code[p - 1]);
            const bool right = p + len < code.size() && ident(code[p + len]);
            if (!left && !right)
                return true;
        }
    }
    return false;
}

// polyscope's depth peeling clears its buffer at the top of every peel pass,
// so a multi-pass frame leaves only the last peeled layer. Pinning to one
// pass keeps the true nearest-surface depth; only ordering across several
// transparent layers is lost.
void Shader::applySceneDepthMode()
{
    if (scene_depth_users > 0) {
        if (saved_peel_passes < 0)
            saved_peel_passes = polyscope::options::transparencyRenderPasses;
        polyscope::options::transparencyRenderPasses = 1;
    } else if (saved_peel_passes >= 0) {
        polyscope::options::transparencyRenderPasses = saved_peel_passes;
        saved_peel_passes = -1;
    }
}

void Shader::useSceneDepth(bool on)
{
    if (on == wants_scene_depth)
        return;
    wants_scene_depth = on;
    scene_depth_users += on ? 1 : -1;
    applySceneDepthMode();
}

void Shader::setResolution(int w, int h)
{
    explicit_resolution = true;
    if (w == res_x && h == res_y)
        return;
    res_x = std::max(1, w);
    res_y = std::max(1, h);
    gl_ready = false; // force the texture/fbo to be recreated at the new size
}

// ── uniforms ────────────────────────────────────────────────────────────────
void Shader::set(const std::string& n, float v)
{ uniforms[n] = [v](int l){ gl().Uniform1f(l, v); }; }
void Shader::set(const std::string& n, int v)
{ uniforms[n] = [v](int l){ gl().Uniform1i(l, v); }; }
void Shader::set(const std::string& n, const vec2& v)
{ float a=float(v(0)),b=float(v(1)); uniforms[n]=[a,b](int l){ gl().Uniform2f(l,a,b); }; }
void Shader::set(const std::string& n, const vec& v)
{ float a=float(v(0)),b=float(v(1)),c=float(v(2)); uniforms[n]=[a,b,c](int l){ gl().Uniform3f(l,a,b,c); }; }
void Shader::set(const std::string& n, const RGBA& v)
{ ImVec4 c=v.Value; uniforms[n]=[c](int l){ gl().Uniform4f(l,c.x,c.y,c.z,c.w); }; }

void Shader::bindF(const std::string& n, std::function<float()> f)
{ uniforms[n] = [f](int l){ gl().Uniform1f(l, f()); }; }
void Shader::bindV2(const std::string& n, std::function<vec2()> f)
{ uniforms[n] = [f](int l){ auto v=f(); gl().Uniform2f(l, float(v(0)), float(v(1))); }; }
void Shader::bindV3(const std::string& n, std::function<vec()> f)
{ uniforms[n] = [f](int l){ auto v=f(); gl().Uniform3f(l, float(v(0)), float(v(1)), float(v(2))); }; }
void Shader::bindV4(const std::string& n, std::function<RGBA()> f)
{ uniforms[n] = [f](int l){ ImVec4 c=f().Value; gl().Uniform4f(l, c.x, c.y, c.z, c.w); }; }

// ── channels & buffers ──────────────────────────────────────────────────────
// every rebind of a channel goes through here : an Image channel owns its
// texture, and overwriting the slot without freeing it would leak it
void Shader::releaseChannel(int i)
{
    if (i < 0 || i >= kChannels) return;
    auto& g = gl();
    if (g.ok && channel[i].kind == Channel::Kind::Image && channel[i].image_tex)
        g.DeleteTextures(1, &channel[i].image_tex);
    channel[i] = Channel{};
    // dropping the last Self channel also drops the need for a ping-pong target
    feedback = false;
    for (auto& c : channel)
        if (c.kind == Channel::Kind::Self) feedback = true;
}

void Shader::setChannel(int i, const path& image_file, Filter f, Wrap w)
{
    if (i < 0 || i >= kChannels) return;
    const SLGLenum filt = (f == Filter::Nearest) ? SL_NEAREST : SL_LINEAR;
    const SLGLenum wr   = (w == Wrap::Repeat)    ? SL_REPEAT  : SL_CLAMP_TO_EDGE;
    int iw = 0, ih = 0;
    SLGLuint tex = loadImageTexture(formatPath(image_file), filt, wr, iw, ih);
    releaseChannel(i);
    channel[i].kind = Channel::Kind::Image;
    channel[i].image_tex = tex;
    channel[i].w = iw;
    channel[i].h = ih;
}

void Shader::setChannel(int i, const ShaderPtr& src, int attachment)
{
    if (i < 0 || i >= kChannels || !src) return;
    // a shader pointed at itself would read and write the same texture in one
    // pass : that is exactly what the ping-pong path is for
    if (src.get() == this) {
        setChannelSelf(i, attachment);
        return;
    }
    releaseChannel(i);
    channel[i].kind = Channel::Kind::ShaderOut;
    channel[i].src = src;
    channel[i].attachment = std::max(0, std::min(attachment, kMaxTargets - 1));
}

void Shader::setChannelSelf(int i, int attachment)
{
    if (i < 0 || i >= kChannels) return;
    releaseChannel(i);
    channel[i].kind = Channel::Kind::Self;
    channel[i].attachment = std::max(0, std::min(attachment, kMaxTargets - 1));
    feedback = true;
    gl_ready = false; // needs the second (ping-pong) target
}

void Shader::clearChannel(int i)
{
    if (i < 0 || i >= kChannels) return;
    releaseChannel(i);
}

void Shader::setFloatBuffer(bool on)
{
    if (float_buffer == on) return;
    float_buffer = on;
    gl_ready = false;
}

void Shader::setFilter(Filter f)
{
    self_filter = (f == Filter::Nearest) ? SL_NEAREST : SL_LINEAR;
    gl_ready = false;
}

void Shader::setWrap(Wrap w)
{
    self_wrap = (w == Wrap::Repeat) ? SL_REPEAT : SL_CLAMP_TO_EDGE;
    gl_ready = false;
}

void Shader::setHidden(bool on) { hidden = on; }

void Shader::setTargets(int n)
{
    n = std::max(1, std::min(n, kMaxTargets));
    if (n == num_targets) return;
    num_targets = n;
    gl_ready = false; // rebuild the FBO(s) with the new attachment count
}

// ── CPU → GPU : data textures ────────────────────────────────────────────────
void Shader::setData(int i, const float* data, int w, int h, int comps,
                     Filter f, Wrap wrapMode)
{
    if (i < 0 || i >= kChannels || !data || w <= 0 || h <= 0)
        return;
    auto& g = gl();
    if (!g.ok)
        return;
    comps = std::max(1, std::min(4, comps));

    SLGLint  internal;
    SLGLenum fmt;
    switch (comps) {
        case 1:  internal = SLGLint(SL_R32F);    fmt = SL_RED;  break;
        case 2:  internal = SLGLint(SL_RG32F);   fmt = SL_RG;   break;
        case 3:  internal = SLGLint(SL_RGB32F);  fmt = SL_RGB;  break;
        default: internal = SLGLint(SL_RGBA32F); fmt = SL_RGBA; break;
    }
    const SLGLenum filter = (f == Filter::Nearest)     ? SL_NEAREST : SL_LINEAR;
    const SLGLenum wrap   = (wrapMode == Wrap::Repeat) ? SL_REPEAT  : SL_CLAMP_TO_EDGE;

    Channel& c = channel[i];
    SLGLint prev_tex = 0;
    g.GetIntegerv(SL_TEXTURE_BINDING_2D, &prev_tex);

    // reuse the texture (update in place) when nothing about its layout changed
    const bool reuse = (c.kind == Channel::Kind::Image && c.image_tex &&
                        c.comps == comps && c.w == w && c.h == h);
    if (reuse) {
        g.BindTexture(SL_TEXTURE_2D, c.image_tex);
        g.TexSubImage2D(SL_TEXTURE_2D, 0, 0, 0, w, h, fmt, SL_FLOAT, data);
    } else {
        releaseChannel(i);
        c.kind = Channel::Kind::Image;
        g.GenTextures(1, &c.image_tex);
        g.BindTexture(SL_TEXTURE_2D, c.image_tex);
        g.TexImage2D(SL_TEXTURE_2D, 0, internal, w, h, 0, fmt, SL_FLOAT, data);
        g.TexParameteri(SL_TEXTURE_2D, SL_TEXTURE_MIN_FILTER, SLGLint(filter));
        g.TexParameteri(SL_TEXTURE_2D, SL_TEXTURE_MAG_FILTER, SLGLint(filter));
        g.TexParameteri(SL_TEXTURE_2D, SL_TEXTURE_WRAP_S, SLGLint(wrap));
        g.TexParameteri(SL_TEXTURE_2D, SL_TEXTURE_WRAP_T, SLGLint(wrap));
        c.w = w; c.h = h; c.comps = comps;
    }
    g.BindTexture(SL_TEXTURE_2D, SLGLuint(prev_tex));
}

// ── GPU → CPU : readback ─────────────────────────────────────────────────────
bool Shader::readback(std::vector<float>& out, int attachment) const
{
    auto& g = gl();
    if (!g.ok || !compiled || !buf[cur].fbo ||
        attachment < 0 || attachment >= num_targets || !buf[cur].tex[attachment])
        return false;
    out.resize(size_t(res_x) * size_t(res_y) * 4);
    SLGLint prev_fbo = 0;
    g.GetIntegerv(SL_FRAMEBUFFER_BINDING, &prev_fbo);
    g.BindFramebuffer(SL_FRAMEBUFFER, buf[cur].fbo);
    g.ReadBuffer(SL_COLOR_ATTACHMENT0 + attachment);
    g.ReadPixels(0, 0, res_x, res_y, SL_RGBA, SL_FLOAT, out.data());
    g.BindFramebuffer(SL_FRAMEBUFFER, SLGLuint(prev_fbo));
    return true;
}

RGBA Shader::readbackMean(int attachment) const
{
    std::vector<float> px;
    if (!readback(px, attachment) || px.empty())
        return RGBA(0.f, 0.f, 0.f, 0.f);
    double r = 0, gg = 0, b = 0, a = 0;
    const size_t n = px.size() / 4;
    for (size_t k = 0; k < n; ++k) {
        r += px[4*k]; gg += px[4*k+1]; b += px[4*k+2]; a += px[4*k+3];
    }
    return RGBA(float(r/n), float(gg/n), float(b/n), float(a/n));
}

RGBA Shader::readbackPixel(int x, int y, int attachment) const
{
    auto& g = gl();
    if (!g.ok || !compiled || !buf[cur].fbo ||
        attachment < 0 || attachment >= num_targets || !buf[cur].tex[attachment] ||
        x < 0 || y < 0 || x >= res_x || y >= res_y)
        return RGBA(0.f, 0.f, 0.f, 0.f);
    float p[4] = {0, 0, 0, 0};
    SLGLint prev_fbo = 0;
    g.GetIntegerv(SL_FRAMEBUFFER_BINDING, &prev_fbo);
    g.BindFramebuffer(SL_FRAMEBUFFER, buf[cur].fbo);
    g.ReadBuffer(SL_COLOR_ATTACHMENT0 + attachment);
    g.ReadPixels(x, y, 1, 1, SL_RGBA, SL_FLOAT, p);
    g.BindFramebuffer(SL_FRAMEBUFFER, SLGLuint(prev_fbo));
    return RGBA(p[0], p[1], p[2], p[3]);
}

// ── shader storage buffers ───────────────────────────────────────────────────
void Shader::setBuffer(int binding, const void* data, std::size_t bytes)
{
    auto& g = gl();
    if (!g.ok || binding < 0 || bytes == 0)
        return;
    StorageBuffer& sb = ssbos[binding];
    if (!sb.id)
        g.GenBuffers(1, &sb.id);
    g.BindBuffer(SL_SHADER_STORAGE_BUFFER, sb.id);
    if (sb.bytes == bytes && data) {
        g.BufferSubData(SL_SHADER_STORAGE_BUFFER, 0, SLGLsizeiptr(bytes), data);
    } else {
        g.BufferData(SL_SHADER_STORAGE_BUFFER, SLGLsizeiptr(bytes), data, SL_DYNAMIC_DRAW);
        sb.bytes = bytes;
    }
    g.BindBuffer(SL_SHADER_STORAGE_BUFFER, 0);
}

void Shader::allocBuffer(int binding, std::size_t bytes)
{
    if (bytes == 0)
        return;
    std::vector<unsigned char> zeros(bytes, 0);
    setBuffer(binding, zeros.data(), bytes);
}

bool Shader::readBuffer(int binding, void* dst, std::size_t bytes) const
{
    auto& g = gl();
    auto it = ssbos.find(binding);
    if (!g.ok || it == ssbos.end() || !it->second.id || bytes == 0 || !dst)
        return false;
    bytes = std::min(bytes, it->second.bytes);
    g.BindBuffer(SL_SHADER_STORAGE_BUFFER, it->second.id);
    g.GetBufferSubData(SL_SHADER_STORAGE_BUFFER, 0, SLGLsizeiptr(bytes), dst);
    g.BindBuffer(SL_SHADER_STORAGE_BUFFER, 0);
    return true;
}

void Shader::clearBuffer(int binding)
{
    auto it = ssbos.find(binding);
    if (it == ssbos.end())
        return;
    auto& g = gl();
    if (g.ok && it->second.id)
        g.DeleteBuffers(1, &it->second.id);
    ssbos.erase(it);
}

// ── GL resources ──────────────────────────────────────────────────────────────
void Shader::ensureResources()
{
    if (gl_ready)
        return;
    auto& g = gl();
    if (!g.ok)
        return;

    if (!vao)
        g.GenVertexArrays(1, &vao);

    // the viewport is part of what we stomp below (clearing each target) : the
    // caller captures GL state *after* this runs, so it must be restored here or
    // polyscope's own viewport is lost on every frame that (re)creates resources
    SLGLint prev_fbo = 0, prev_tex = 0, prev_vp[4] = {0,0,0,0};
    SLGLfloat prev_clear[4] = {0,0,0,0};
    g.GetIntegerv(SL_FRAMEBUFFER_BINDING, &prev_fbo);
    g.GetIntegerv(SL_TEXTURE_BINDING_2D, &prev_tex);
    g.GetIntegerv(SL_VIEWPORT, prev_vp);
    g.GetFloatv(SL_COLOR_CLEAR_VALUE, prev_clear);

    // one target for a plain shader, two for a feedback (ping-pong) shader.
    // Each target's FBO stays attached to its own texture, so ping-pong is
    // just a swap of which one we read / write.
    const int n = feedback ? 2 : 1;
    const SLGLint internal = float_buffer ? SLGLint(SL_RGBA32F) : SLGLint(SL_RGBA);
    const SLGLenum type    = float_buffer ? SL_FLOAT : SL_UNSIGNED_BYTE;

    const int nt = std::max(1, std::min(num_targets, kMaxTargets));
    SLGLenum draw_bufs[kMaxTargets];
    for (int a = 0; a < nt; ++a)
        draw_bufs[a] = SL_COLOR_ATTACHMENT0 + a;

    for (int b = 0; b < n; ++b) {
        Target& T = buf[b];
        if (!T.fbo)
            g.GenFramebuffers(1, &T.fbo);
        g.BindFramebuffer(SL_FRAMEBUFFER, T.fbo);

        // (re)create one texture per color attachment
        for (int a = 0; a < kMaxTargets; ++a) {
            if (a < nt) {
                if (T.tex[a]) g.DeleteTextures(1, &T.tex[a]);
                g.GenTextures(1, &T.tex[a]);
                g.BindTexture(SL_TEXTURE_2D, T.tex[a]);
                g.TexImage2D(SL_TEXTURE_2D, 0, internal, res_x, res_y, 0, SL_RGBA, type, nullptr);
                g.TexParameteri(SL_TEXTURE_2D, SL_TEXTURE_MIN_FILTER, SLGLint(self_filter));
                g.TexParameteri(SL_TEXTURE_2D, SL_TEXTURE_MAG_FILTER, SLGLint(self_filter));
                g.TexParameteri(SL_TEXTURE_2D, SL_TEXTURE_WRAP_S, SLGLint(self_wrap));
                g.TexParameteri(SL_TEXTURE_2D, SL_TEXTURE_WRAP_T, SLGLint(self_wrap));
                g.FramebufferTexture2D(SL_FRAMEBUFFER, SL_COLOR_ATTACHMENT0 + a,
                                       SL_TEXTURE_2D, T.tex[a], 0);
            } else if (T.tex[a]) {           // shrinking : drop stale attachments
                g.DeleteTextures(1, &T.tex[a]);
                T.tex[a] = 0;
                g.FramebufferTexture2D(SL_FRAMEBUFFER, SL_COLOR_ATTACHMENT0 + a,
                                       SL_TEXTURE_2D, 0, 0);
            }
        }
        // this FBO writes to all nt attachments (state stored on the FBO)
        g.DrawBuffers(nt, draw_bufs);
        if (g.CheckFramebufferStatus(SL_FRAMEBUFFER) != SL_FRAMEBUFFER_COMPLETE)
            spdlog::error("[shader] framebuffer incomplete");
        // start every target cleared, so a feedback loop begins from zero
        g.Viewport(0, 0, res_x, res_y);
        g.ClearColor(0.f, 0.f, 0.f, 0.f);
        g.Clear(SL_COLOR_BUFFER_BIT);
    }

    g.BindTexture(SL_TEXTURE_2D, SLGLuint(prev_tex));
    g.BindFramebuffer(SL_FRAMEBUFFER, SLGLuint(prev_fbo));
    g.Viewport(prev_vp[0], prev_vp[1], prev_vp[2], prev_vp[3]);
    g.ClearColor(prev_clear[0], prev_clear[1], prev_clear[2], prev_clear[3]);

    cur = 0;
    gl_ready = true;
}

void Shader::recompile()
{
    needs_recompile = false;
    auto& g = gl();
    if (!g.ok)
        return;

    // pull in whatever the source #include's; the files reached are recorded
    // so hot reload watches them too, and the index -> file table lets a
    // failed compile be traced back past the inclusion.
    const std::string self = from_file ? source_file.string() : "<inline>";
    const path base = from_file ? source_file.parent_path()
                                : path(Options::ProjectDataPath);
    auto X = expandIncludes(fragment_src, base, self);
    include_deps = std::move(X.deps);
    source_units = std::move(X.units);

    // Using the depth API is the declaration : scanning the shader's own text
    // (not the expanded source) means a call hidden inside your own included
    // helper is missed, and has to call useSceneDepth() from C++ instead.
    useSceneDepth(referencesSceneDepth(fragment_src));

    // a source with its own #version is complete; otherwise prepend the
    // built-in prelude. Looks for a real directive, not a substring, so a
    // comment mentioning "#version" does not skip the prelude.
    const bool complete = hasVersionDirective(fragment_src);
    // keyframe defines come after the prelude (they may reference nothing) and
    // before the #line reset, so user line numbers in compile errors stay right
    const std::string frag = complete
        ? X.source
        : (versionLine() + std::string(kFragPreludeBody) + keyframeDefines()
           + kLineReset + X.source);

    // an SSBO shader on a context that cannot offer 430 would fail to compile
    // with a confusing "unexpected buffer" error : name the real cause first
    if (!complete && std::string(versionLine()).find("430") == std::string::npos &&
        fragment_src.find("std430") != std::string::npos)
        spdlog::error("[shader] this shader uses SSBOs (std430) but the GL context "
                      "is only {}.{} : shader storage buffers need 4.3", g.major, g.minor);

    SLGLuint vs = compileStage(SL_VERTEX_SHADER,
                               versionLine() + std::string(kVertexBody), "vertex");
    SLGLuint fs = compileStage(SL_FRAGMENT_SHADER, frag, "fragment");
    // GLSL reports "<source string>:<line>", so name the strings when there is
    // more than one of them
    if (!fs && source_units.size() > 1) {
        std::string table;
        for (size_t u = 0; u < source_units.size(); ++u)
            table += "\n  [" + std::to_string(u) + "] " + source_units[u];
        spdlog::error("[shader] source strings:{}", table);
    }
    if (!vs || !fs) {
        if (vs) g.DeleteShader(vs);
        if (fs) g.DeleteShader(fs);
        return; // keep the previous program, if any, so the slide stays up
    }

    SLGLuint prog = g.CreateProgram();
    g.AttachShader(prog, vs);
    g.AttachShader(prog, fs);
    g.LinkProgram(prog);
    g.DeleteShader(vs);
    g.DeleteShader(fs);

    SLGLint ok = 0;
    g.GetProgramiv(prog, SL_LINK_STATUS, &ok);
    if (!ok) {
        SLGLint len = 0;
        g.GetProgramiv(prog, SL_INFO_LOG_LENGTH, &len);
        std::string log(std::max(len, 1), '\0');
        g.GetProgramInfoLog(prog, len, nullptr, log.data());
        spdlog::error("[shader] link failed:\n{}", log);
        g.DeleteProgram(prog);
        return;
    }

    if (program)
        g.DeleteProgram(program);
    program = prog;
    compiled = true;
    cacheUniformLocations();
}

// resolve every built-in name once, here, rather than once per frame. Only
// called after a link succeeds : a failed recompile keeps the previous
// program, and must keep its locations with it.
void Shader::cacheUniformLocations()
{
    auto& g = gl();
    if (!g.ok || !program)
        return;
    auto L = [&](const char* n){ return g.GetUniformLocation(program, n); };

    uloc = BuiltinLocs{};
    uloc.iResolution = L("iResolution");
    uloc.iAspect     = L("iAspect");
    uloc.iTime       = L("iTime");
    uloc.iTimeDelta  = L("iTimeDelta");
    uloc.iFrame      = L("iFrame");
    uloc.iFrameRate  = L("iFrameRate");

    uloc.from_begin            = L("from_begin");
    uloc.from_action           = L("from_action");
    uloc.inner_time            = L("inner_time");
    uloc.delta_time            = L("delta_time");
    uloc.absolute_frame_number = L("absolute_frame_number");
    uloc.relative_frame_number = L("relative_frame_number");
    uloc.transition_parameter  = L("transition_parameter");

    uloc.iView       = L("iView");
    uloc.iViewInv    = L("iViewInv");
    uloc.iProj       = L("iProj");
    uloc.iProjInv    = L("iProjInv");
    uloc.iCamPos     = L("iCamPos");
    uloc.iCamFov     = L("iCamFov");
    uloc.iScreenRect = L("iScreenRect");
    uloc.iWindowSize = L("iWindowSize");

    uloc.iSceneDepth      = L("iSceneDepth");
    uloc.iSceneDepthValid = L("iSceneDepthValid");
    uloc.iSceneDepthSize  = L("iSceneDepthSize");

    uloc.iMouse     = L("iMouse");
    uloc.iMouseNorm = L("iMouseNorm");
    uloc.iHovered   = L("iHovered");
    uloc.iDate      = L("iDate");

    for (int i = 0; i < kChannels; ++i) {
        // built here once, so the names can never be truncated at draw time
        const std::string ch  = "iChannel" + std::to_string(i);
        const std::string res = "iChannelResolution[" + std::to_string(i) + "]";
        uloc.iChannel[i]    = L(ch.c_str());
        uloc.iChannelRes[i] = L(res.c_str());
    }

    user_uniform_loc.clear();
}

void Shader::renderToTexture(const TimeObject& t, const StateInSlide& sis)
{
    ensureResources();
    if (needs_recompile)
        recompile();
    auto& g = gl();
    if (!g.ok || !program || !buf[0].fbo)
        return;

    // save the bits of GL state we are about to stomp, so polyscope's own frame
    // resumes untouched
    SLGLint prev_fbo=0, prev_prog=0, prev_vao=0, prev_tex=0, prev_vp[4]={0,0,0,0};
    SLGLfloat prev_clear[4] = {0,0,0,0};
    const bool prev_depth = g.IsEnabled(SL_DEPTH_TEST) != 0;
    g.GetFloatv(SL_COLOR_CLEAR_VALUE, prev_clear);
    g.GetIntegerv(SL_FRAMEBUFFER_BINDING, &prev_fbo);
    g.GetIntegerv(SL_CURRENT_PROGRAM, &prev_prog);
    g.GetIntegerv(SL_VERTEX_ARRAY_BINDING, &prev_vao);
    g.GetIntegerv(SL_TEXTURE_BINDING_2D, &prev_tex);
    g.GetIntegerv(SL_VIEWPORT, prev_vp);

    // ping-pong : read the current output, write the other target. A plain
    // (non-feedback) shader always reads/writes target 0.
    const int read  = cur;
    const int write = feedback ? (1 - cur) : 0;

    g.BindFramebuffer(SL_FRAMEBUFFER, buf[write].fbo);
    g.Viewport(0, 0, res_x, res_y);
    g.Disable(SL_DEPTH_TEST);
    g.UseProgram(program);
    g.BindVertexArray(vao);

    const BuiltinLocs& U = uloc;   // resolved at link time, not per frame

    // ── built-in uniforms ────────────────────────────────────────────────────
    if (int l = U.iResolution; l >= 0) g.Uniform2f(l, float(res_x), float(res_y));
    if (int l = U.iAspect; l >= 0) g.Uniform1f(l, float(res_x) / std::max(float(res_y), 1.f));
    if (int l = U.iTime; l >= 0) g.Uniform1f(l, float(t.inner_time));
    if (int l = U.iTimeDelta; l >= 0) g.Uniform1f(l, float(t.delta_time));
    if (int l = U.iFrame; l >= 0) g.Uniform1i(l, frames_rendered);
    if (int l = U.iFrameRate; l >= 0) g.Uniform1f(l, ImGui::GetIO().Framerate);

    // the TimeObject, field for field. What it calls a "frame" is a slide,
    // which is why iFrame is separate : it counts renders of this shader.
    if (int l = U.from_begin; l >= 0) g.Uniform1f(l, float(t.from_begin));
    if (int l = U.from_action; l >= 0) g.Uniform1f(l, float(t.from_action));
    if (int l = U.inner_time; l >= 0) g.Uniform1f(l, float(t.inner_time));
    if (int l = U.delta_time; l >= 0) g.Uniform1f(l, float(t.delta_time));
    if (int l = U.absolute_frame_number; l >= 0) g.Uniform1i(l, t.absolute_frame_number);
    if (int l = U.relative_frame_number; l >= 0) g.Uniform1i(l, t.relative_frame_number);
    if (int l = U.transition_parameter; l >= 0) g.Uniform1f(l, float(t.transition_parameter));

    // cursor mapped into the shader rect, in rect pixels with y up (ShaderToy).
    // Deferred renders run from an ImGui draw callback with an empty window
    // stack, so the rect is taken from record time, not read here.
    ImVec2 pmin, pmax;
    if (use_pending_rect) { pmin = pending_pmin; pmax = pending_pmax; }
    else                    screenRect(sis, pmin, pmax);
    const float w = pmax.x - pmin.x, h = pmax.y - pmin.y;
    const ImVec2 m = ImGui::GetIO().MousePos;
    const float u = (m.x - pmin.x) / std::max(w, 1.f);           // 0..1, left -> right
    const float v = (m.y - pmin.y) / std::max(h, 1.f);           // 0..1, top -> bottom
    const bool  hovered = (u >= 0.f && u <= 1.f && v >= 0.f && v <= 1.f);
    const float mx = u * res_x;            // rect pixels, y up
    const float my = (1.f - v) * res_y;

    // click tracking : latch the position on the press frame, keep it while held.
    // sign of iMouse.z encodes the pressed state, ShaderToy-style.
    const bool down = hovered && ImGui::IsMouseDown(0);
    if (down && !mouse_was_down) { last_click_x = mx; last_click_y = my; }
    mouse_was_down = down;

    if (int l = U.iMouse; l >= 0)
        g.Uniform4f(l, mx, my,
                    down ? last_click_x : -last_click_x,
                    down ? last_click_y : -last_click_y);
    if (int l = U.iMouseNorm; l >= 0) g.Uniform2f(l, u, 1.f - v);
    if (int l = U.iHovered; l >= 0) g.Uniform1f(l, hovered ? 1.f : 0.f);

    // ── polyscope's camera ───────────────────────────────────────────────────
    // uploaded whole, plus where this shader sits in the window, so a fragment
    // maps back to the screen pixel polyscope draws there. Inverses are done
    // here rather than per fragment.
    {
        const glm::mat4 V = polyscope::view::getCameraViewMatrix();
        const glm::mat4 Pr = polyscope::view::getCameraPerspectiveMatrix();
        auto mat = [&](int l, const glm::mat4& M) {
            if (l >= 0)
                g.UniformMatrix4fv(l, 1, 0 /*no transpose : glm is column-major*/,
                                   &M[0][0]);
        };
        mat(U.iView, V);
        mat(U.iProj, Pr);
        // the inverses cost more than the upload, so only do them if wanted
        if (U.iViewInv >= 0) mat(U.iViewInv, glm::inverse(V));
        if (U.iProjInv >= 0) mat(U.iProjInv, glm::inverse(Pr));
        if (int l = U.iCamPos; l >= 0) {
            glm::vec3 c = polyscope::view::getCameraWorldPosition();
            g.Uniform3f(l, c.x, c.y, c.z);
        }
        if (int l = U.iCamFov; l >= 0)
            g.Uniform1f(l, float(polyscope::view::fov));
        if (int l = U.iScreenRect; l >= 0)
            g.Uniform4f(l, pmin.x, pmin.y, w, h);
        if (int l = U.iWindowSize; l >= 0) {
            const ImVec2 ds = ImGui::GetIO().DisplaySize;
            g.Uniform2f(l, ds.x, ds.y);
        }

        // polyscope's depth buffer, so a shader can be occluded by the 3D
        // scene instead of always drawing over it. Re-asserted every frame,
        // since the options panel can toggle it live.
        SLGLuint dtex = 0; int dw = 0, dh = 0;
        if (wants_scene_depth) {
            applySceneDepthMode();

            if (polyscope::render::engine && polyscope::render::engine->sceneDepth) {
                auto& d = *polyscope::render::engine->sceneDepth;
                dtex = SLGLuint(d.getNativeBufferID());
                dw = int(d.getSizeX());
                dh = int(d.getSizeY());
            }
        }
        if (int l = U.iSceneDepth; l >= 0) {
            g.ActiveTexture(SL_TEXTURE0 + kSceneDepthUnit);
            g.BindTexture(SL_TEXTURE_2D, dtex);
            g.Uniform1i(l, kSceneDepthUnit);
            g.ActiveTexture(SL_TEXTURE0);
            bound_scene_depth = true;
            // current for a deferred (visible) shader, which runs after the
            // scene pass; a hidden shader can't defer, so its depth is last
            // frame's — keep requesting a redraw so that copy never goes stale
            if (dtex) polyscope::requestRedraw();
        }
        // "valid" means usable, not merely present : mid-peel the texture is
        // bound but holds the last peeled layer, worse than useless to trust
        const bool depth_usable =
            dtex && (polyscope::options::transparencyMode != polyscope::TransparencyMode::Pretty
                     || polyscope::options::transparencyRenderPasses <= 1);
        if (int l = U.iSceneDepthValid; l >= 0) g.Uniform1f(l, depth_usable ? 1.f : 0.f);
        if (int l = U.iSceneDepthSize; l >= 0) g.Uniform2f(l, float(dw), float(dh));
    }

    if (int l = U.iDate; l >= 0) {
        std::time_t tt = std::time(nullptr);
        std::tm lt{};
#if defined(_WIN32)
        localtime_s(&lt, &tt);
#else
        localtime_r(&tt, &lt);
#endif
        const float secs = lt.tm_hour * 3600.f + lt.tm_min * 60.f + lt.tm_sec;
        g.Uniform4f(l, float(lt.tm_year + 1900), float(lt.tm_mon + 1),
                    float(lt.tm_mday), secs);
    }
    // ── texture channels ─────────────────────────────────────────────────────
    // each bound channel resolves to a source texture (image, another shader's
    // output, or our own previous frame) bound to texture unit i.
    for (int i = 0; i < kChannels; ++i) {
        const Channel& c = channel[i];
        if (c.kind == Channel::Kind::Off)
            continue;
        SLGLuint tex = 0; int cw = 0, ch = 0;
        switch (c.kind) {
            case Channel::Kind::Image:
                tex = c.image_tex; cw = c.w; ch = c.h; break;
            case Channel::Kind::ShaderOut:
                // the source may have been dropped since it was bound
                if (auto sp = c.src.lock()) {
                    tex = sp->currentTexture(c.attachment);
                    cw = sp->res_x; ch = sp->res_y;
                }
                break;
            case Channel::Kind::Self:
                tex = buf[read].tex[c.attachment]; cw = res_x; ch = res_y; break;
            default: break;
        }
        if (!tex)
            continue;
        g.ActiveTexture(SL_TEXTURE0 + i);
        g.BindTexture(SL_TEXTURE_2D, tex);
        // names were built and resolved at link time, so there is no longer any
        // string formatting (nor any buffer to size wrongly) on the draw path
        if (U.iChannel[i]    >= 0) g.Uniform1i(U.iChannel[i], i);
        if (U.iChannelRes[i] >= 0) g.Uniform3f(U.iChannelRes[i], float(cw), float(ch), 1.f);
    }
    g.ActiveTexture(SL_TEXTURE0); // leave unit 0 active, as most code expects

    // storage buffers : bound to their explicit std430 binding points
    for (auto& [binding, sb] : ssbos)
        if (sb.id) g.BindBufferBase(SL_SHADER_STORAGE_BUFFER, SLGLuint(binding), sb.id);

    // user uniforms : unknown names resolve to -1 and are skipped, so editing
    // the shader to add/remove a uniform never throws. Names arrive at
    // runtime, so these are resolved on first use and cached (including -1s).
    for (auto& [name, setter] : uniforms) {
        auto it = user_uniform_loc.find(name);
        if (it == user_uniform_loc.end())
            it = user_uniform_loc.emplace(
                name, g.GetUniformLocation(program, name.c_str())).first;
        if (it->second >= 0)
            setter(it->second);
    }

    g.ClearColor(0.f, 0.f, 0.f, 0.f);
    g.Clear(SL_COLOR_BUFFER_BIT);
    g.DrawArrays(SL_TRIANGLES, 0, 3);

    // make SSBO writes from this pass visible to later readBuffer() / next frame,
    // then release the binding points so we don't leak state into other draws
    if (!ssbos.empty()) {
        if (g.MemoryBarrier) g.MemoryBarrier(SL_SHADER_STORAGE_BARRIER_BIT);
        for (auto& [binding, sb] : ssbos)
            g.BindBufferBase(SL_SHADER_STORAGE_BUFFER, SLGLuint(binding), 0);
    }

    // the freshly written target becomes the current output
    cur = write;
    ++frames_rendered;

    // restore. Units 1..3 are cleared explicitly : only unit 0's binding was
    // saved, so anything we left on the others would leak into later draws.
    if (bound_scene_depth) {
        g.ActiveTexture(SL_TEXTURE0 + kSceneDepthUnit);
        g.BindTexture(SL_TEXTURE_2D, 0);
        bound_scene_depth = false;
    }
    for (int i = kChannels - 1; i >= 1; --i) {
        if (channel[i].kind == Channel::Kind::Off)
            continue;
        g.ActiveTexture(SL_TEXTURE0 + i);
        g.BindTexture(SL_TEXTURE_2D, 0);
    }
    g.ActiveTexture(SL_TEXTURE0);
    g.UseProgram(SLGLuint(prev_prog));
    g.BindVertexArray(SLGLuint(prev_vao));
    g.BindTexture(SL_TEXTURE_2D, SLGLuint(prev_tex));
    g.BindFramebuffer(SL_FRAMEBUFFER, SLGLuint(prev_fbo));
    g.Viewport(prev_vp[0], prev_vp[1], prev_vp[2], prev_vp[3]);
    g.ClearColor(prev_clear[0], prev_clear[1], prev_clear[2], prev_clear[3]);
    // depth test is restored to what it was, not unconditionally enabled : the
    // slide is drawn inside an ImGui pass, which usually runs with it off
    if (prev_depth) g.Enable(SL_DEPTH_TEST);
    else            g.Disable(SL_DEPTH_TEST);
}

void Shader::screenRect(const StateInSlide& sis, ImVec2& pmin, ImVec2& pmax) const
{
    const float scale = float(sis.getScale());
    const vec2 base = getSize();
    const ImVec2 size(float(base(0))*scale, float(base(1))*scale);
    const auto P = sis.getAbsolutePosition();
    pmin = ImVec2(P.x - size.x*0.5f, P.y - size.y*0.5f);
    pmax = ImVec2(pmin.x + size.x, pmin.y + size.y);
}

void Shader::display(const StateInSlide& sis, float global_alpha)
{
    // hidden passes only feed other shaders : they render but never blit
    if (hidden || !compiled || !buf[cur].tex[0])
        return;
    ImVec2 pmin, pmax;
    screenRect(sis, pmin, pmax);
    const ImVec2 size(pmax.x - pmin.x, pmax.y - pmin.y);

    const ImU32 col = ImColor(1.f, 1.f, 1.f, global_alpha);
    // V flipped (0,1)->(1,0) : the FBO texture is bottom-left origin, ImGui is
    // top-left, so this puts the ShaderToy image upright on screen
    ImGui::GetWindowDrawList()->AddImage((ImTextureID)(intptr_t)buf[cur].tex[0],
                                         pmin, pmax, ImVec2(0, 1), ImVec2(1, 0), col);
}

vec2 Shader::getSize() const
{
    // at the screen-size default, res_x/res_y already are the window's
    // resolution : show it 1:1, rather than through the 1920x1080-relative
    // scale below (which would double-apply it)
    if (!explicit_resolution)
        return vec2(res_x, res_y);

    double sx = 1, sy = 1;
    bool notFullHD = (Options::ScreenResolutionWidth != 1920) ||
                     (Options::ScreenResolutionHeight != 1080);
    if (notFullHD) {
        sx = Options::ScreenResolutionWidth / 1920.;
        sy = Options::ScreenResolutionHeight / 1080.;
    }
    return vec2(sx * res_x, sy * res_y);
}

// Deferred rendering, so a shader can see *this* frame's scene depth : ImGui
// only records draw commands here, executing them later at ImGuiRender(),
// which runs after polyscope's scene pass. Only shaders using scene depth
// defer this way; everything else renders inline.
void Shader::ImGuiRenderCallback(const ImDrawList*, const ImDrawCmd* cmd)
{
    Shader* self = static_cast<Shader*>(cmd->UserCallbackData);
    // the draw list outlives the frame's C++ scope, so a Shader freed between
    // recording and execution would leave a dangling pointer here
    if (std::find(all_shaders.begin(), all_shaders.end(), self) == all_shaders.end())
        return;
    self->runNextPendingRender();
}

// Render one recorded placement. A queue, not a single slot, because every
// placement is recorded before any callback runs : with one slot a second
// placement would overwrite the first's state.
void Shader::runNextPendingRender()
{
    if (pending_next >= pending.size())
        return;                                  // more callbacks than records
    const PendingRender job = pending[pending_next++];

    pending_pmin = job.pmin;
    pending_pmax = job.pmax;
    use_pending_rect = true;

    // this runs inside ImGui's C render loop, where an escaping exception is
    // undefined behaviour rather than a stack trace; renderToTexture can throw
    try {
        renderToTexture(job.time, job.sis);
    } catch (const std::exception& e) {
        reportRenderError(e.what());
    } catch (...) {
        reportRenderError("unknown exception");
    }

    use_pending_rect = false;
}

void Shader::reportRenderError(const std::string& what)
{
    // once per shader : this would otherwise fire every frame for the rest of
    // the talk, and drown the log that explains the cause
    if (render_error_reported)
        return;
    render_error_reported = true;
    spdlog::error("[shader] '{}' threw while rendering from the ImGui draw callback : {}"
                  " (further occurrences on this shader are not reported)",
                  from_file ? source_file.string() : "<inline>", what);
}

void Shader::drawWith(const TimeObject& t, const StateInSlide& sis, float alpha)
{
    const bool defer = wants_scene_depth && !hidden;

    if (defer) {
        // one record list per frame : anything left over from a frame whose
        // callbacks never ran must not be carried forward
        const int now = ImGui::GetFrameCount();
        if (now != record_frame) {
            pending.clear();
            pending_next = 0;
            record_frame = now;
        }

        // anything needing ImGui's window state has to be read now, not from
        // the callback, where there is no current window
        ImVec2 pmin, pmax;
        screenRect(sis, pmin, pmax);
        pending.push_back(PendingRender{t, sis, pmin, pmax});

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddCallback(&Shader::ImGuiRenderCallback, this);
        // our GL work clobbers the state ImGui set up; this makes the backend
        // restore it before the AddImage below is drawn
        dl->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
    } else {
        renderToTexture(t, sis);
    }

    // recorded after the callback, so by the time it draws, the texture holds
    // this frame's render
    display(sis, alpha);
}

void Shader::draw(const TimeObject& t, const StateInSlide& sis)
{
    drawWith(t, sis, float(sis.alpha));
}

void Shader::playIntro(const TimeObject& t, const StateInSlide& sis)
{
    drawWith(t, sis, float(sis.alpha) * smoothstep(t.transition_parameter));
}

void Shader::playOutro(const TimeObject& t, const StateInSlide& sis)
{
    drawWith(t, sis, float(sis.alpha) * (1.f - smoothstep(t.transition_parameter)));
}

void Shader::HotReloadIfModified()
{
    static auto last_refresh = Time::now();
    if (TimeFrom(last_refresh) < 0.2)
        return;
    last_refresh = Time::now();

    // a deck hot reload can move any keyframe's slide index, and those are
    // baked into every program as #defines, so a change invalidates all of
    // them, not just the file-backed ones
    {
        static std::string last_defines;
        static bool have_defines = false;
        std::string defines = keyframeDefines();
        if (have_defines && defines != last_defines) {
            spdlog::info("[shader] keyframes changed, recompiling {} shader(s)",
                         all_shaders.size());
            for (auto* s : all_shaders)
                s->needs_recompile = true;
        }
        last_defines = std::move(defines);
        have_defines = true;
    }

    for (auto* s : all_shaders) {
        std::error_code ec;
        if (s->from_file) {
            auto t = std::filesystem::last_write_time(s->source_file, ec);
            if (!ec && t != s->last_modified) {
                spdlog::info("[shader] reloading {}", s->source_file.string());
                s->reloadFromFile();
                continue;   // the re-expansion will refresh its includes
            }
        }
        // a shared header changed : the shader's own file is untouched, but
        // the program built from it is stale
        for (const auto& [file, stamp] : s->include_deps) {
            auto t = std::filesystem::last_write_time(file, ec);
            if (ec || t == stamp)
                continue;
            spdlog::info("[shader] {} changed, recompiling {}", file,
                         s->from_file ? s->source_file.string() : std::string("<inline>"));
            s->needs_recompile = true;
            break;
        }
    }
}

}
