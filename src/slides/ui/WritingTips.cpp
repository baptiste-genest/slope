#include "slides/ui/WritingTips.h"
#include "slides/ui/EditorTheme.h"

#include "content/screen_primitives/gpu/Shader.h"
#include "content/authoring/Snippet.h"
#include "slides/deck/items/DeckItem.h"
#include "content/config/Options.h"

#include "imgui.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <regex>
#include <cctype>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace slope {

namespace {

enum class Kind { None, Deck, Shader, Snippet };

const ImVec4 kKey     = theme::Cyan;      // keys and signatures
const ImVec4 kValue   = theme::Orange;    // what a key takes
const ImVec4 kComment = theme::Comment;   // comments in the examples

std::string lowerExt(const std::filesystem::path& p)
{
    std::string e = p.extension().string();
    std::transform(e.begin(), e.end(), e.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return e;
}

bool listed(const std::vector<std::filesystem::path>& v, const std::filesystem::path& p)
{
    return std::find(v.begin(), v.end(), p) != v.end();
}

Kind classify(const std::filesystem::path& file)
{
    const std::string e = lowerExt(file);
    if (e == ".yaml" || e == ".yml")
        return Kind::Deck;
    if (e == ".frag" || e == ".glsl" || e == ".fs" || listed(Shader::WatchedFiles(), file))
        return Kind::Shader;
    if (e == ".lua" || listed(Snippet::WatchedFiles(), file))
        return Kind::Snippet;
    return Kind::None;
}

// the watcher lists touch the disk, so a file is classified once
Kind kindOf(const std::filesystem::path& file)
{
    static std::filesystem::path last;
    static Kind kind = Kind::None;
    if (file != last) {
        last = file;
        kind = classify(file);
    }
    return kind;
}

ImGuiTextFilter& filter()
{
    static ImGuiTextFilter f;
    return f;
}

void filterBox()
{
    ImGuiTextFilter& f = filter();
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::InputTextWithHint("##tips-filter", "filter", f.InputBuf, IM_ARRAYSIZE(f.InputBuf)))
        f.Build();
}

// code lines in mono, "//" or "--" comments dimmed, only the ones the filter keeps
void codeBlock(const std::string& text, ImFont* mono, float px, const char* comment)
{
    ImGui::PushFont(mono, px);
    ImGui::PushTextWrapPos(0.f);
    std::istringstream in(text);
    std::string line;
    const size_t cl = std::strlen(comment);
    const float avail = ImGui::GetContentRegionAvail().x;
    while (std::getline(in, line)) {
        if (filter().IsActive() && !filter().PassFilter(line.c_str()))
            continue;
        const size_t c = cl ? line.find(comment) : std::string::npos;
        const size_t first = line.find_first_not_of(' ');
        if (c == std::string::npos) {
            ImGui::TextUnformatted(line.c_str());
        } else if (c == first) {
            ImGui::TextColored(kComment, "%s", line.c_str());
        } else if (ImGui::CalcTextSize(line.c_str()).x > avail) {
            // too wide for both, so the comment goes above its code, at the same indent
            std::string code = line.substr(0, c);
            code.erase(code.find_last_not_of(' ') + 1);
            ImGui::TextColored(kComment, "%s", (line.substr(0, first) + line.substr(c)).c_str());
            ImGui::TextUnformatted(code.c_str());
        } else {
            ImGui::TextUnformatted(line.c_str(), line.c_str() + c);
            ImGui::SameLine(0, 0);
            ImGui::TextColored(kComment, "%s", line.c_str() + c);
        }
    }
    ImGui::PopTextWrapPos();
    ImGui::PopFont();
}

std::string joined(const std::set<std::string>& keys, const std::set<std::string>& skip = {})
{
    std::string out;
    for (const auto& k : keys)
        if (!skip.count(k))
            out += (out.empty() ? "" : "  ") + k;
    return out;
}

// a keyword and what goes with it, hidden when the filter matches neither
void keyRow(const std::string& key, const std::string& fields, ImFont* mono, float px)
{
    if (filter().IsActive() && !filter().PassFilter(key.c_str()) && !filter().PassFilter(fields.c_str()))
        return;
    ImGui::PushFont(mono, px);
    ImGui::PushTextWrapPos(0.f);
    ImGui::TextColored(kKey, "%s", key.c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopFont();
    if (!fields.empty()) {
        ImGui::Indent();
        ImGui::PushTextWrapPos(0.f);
        ImGui::TextDisabled("%s", fields.c_str());
        ImGui::PopTextWrapPos();
        ImGui::Unindent();
    }
}

// "formula: tex formula", then the item's other fields
void itemRow(const std::string& key, const std::string& value, const std::string& fields,
             ImFont* mono, float px)
{
    if (filter().IsActive() && !filter().PassFilter(key.c_str())
        && !filter().PassFilter(value.c_str()) && !filter().PassFilter(fields.c_str()))
        return;
    ImGui::PushFont(mono, px);
    ImGui::PushTextWrapPos(0.f);
    ImGui::TextColored(kKey, "%s:", key.c_str());
    if (!value.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(kValue, "%s", value.c_str());
    }
    ImGui::PopTextWrapPos();
    ImGui::PopFont();
    if (!fields.empty()) {
        ImGui::Indent();
        ImGui::PushTextWrapPos(0.f);
        ImGui::TextDisabled("%s", fields.c_str());
        ImGui::PopTextWrapPos();
        ImGui::Unindent();
    }
}

// a signature in mono and what it gives, below it
void apiRow(const char* signature, const char* what, ImFont* mono, float px)
{
    if (filter().IsActive() && !filter().PassFilter(signature) && !filter().PassFilter(what))
        return;
    ImGui::PushFont(mono, px);
    ImGui::PushTextWrapPos(0.f);
    ImGui::TextColored(kKey, "%s", signature);
    ImGui::PopTextWrapPos();
    ImGui::PopFont();
    if (*what == 0)
        return;
    ImGui::Indent();
    ImGui::PushTextWrapPos(0.f);
    ImGui::TextDisabled("%s", what);
    ImGui::PopTextWrapPos();
    ImGui::Unindent();
}

void prose(const char* text)
{
    if (filter().IsActive())
        return;
    ImGui::PushTextWrapPos(0.f);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
}

void deckTips(ImFont* mono, float px)
{
    if (ImGui::CollapsingHeader("Shape", ImGuiTreeNodeFlags_DefaultOpen)) {
        codeBlock(R"(snippets: anim.lua
template:          # every frame
  - image: logo.png
    at: logo
figure:            # a group
  params: {file: plot.png}
  items:
    - image: $file
      at: fig
slides:
  - frame:
      - title: A title
      - latex: some text
        at: intro  # views/intro.pos
      - step       # next slide
      - formula: e^{i\pi} = -1
        below: intro
  - frame:
      - title: Results
      - figure: results
        file: other.png)", mono, px, "#");
    }

    auto docRows = [&](const KeyDoc& keys) {
        for (const auto& [key, doc] : keys)
            apiRow((key + ":").c_str(), doc.c_str(), mono, px);
    };
    if (ImGui::CollapsingHeader("Top level", ImGuiTreeNodeFlags_DefaultOpen)) {
        docRows(deckTopLevelKeys());
        apiRow("<name>:", "any other list is a group of items, used as \"- name\" in a frame", mono, px);
        apiRow("  params:", "a map of name: default, empty when required", mono, px);
        apiRow("  items:", "the group's items, steps included, reading $name or ${name} inside a string", mono, px);
    }
    if (ImGui::CollapsingHeader("In slides", ImGuiTreeNodeFlags_DefaultOpen)) {
        docRows(frameKeys());
        apiRow("- step", "what follows shows on the next slide", mono, px);
        apiRow("- <name>", "puts a top-level group here", mono, px);
        apiRow("- <name>: id", "the same with args as the other keys, remove: id takes it off", mono, px);
    }
    if (ImGui::CollapsingHeader("config:", ImGuiTreeNodeFlags_DefaultOpen))
        docRows(deckConfigKeys());

    if (ImGui::CollapsingHeader("Placement (every screen item)", ImGuiTreeNodeFlags_DefaultOpen))
        keyRow("keys", joined(placementFields()), mono, px);

    const std::pair<ItemSpec::Kind, const char*> families[] = {
        {ItemSpec::Kind::Screen, "Screen items"},
        {ItemSpec::Kind::Scene,  "Scene items"},
        {ItemSpec::Kind::Custom, "Slide items"},
    };
    for (const auto& [kind, label] : families) {
        if (!ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen))
            continue;
        for (const auto& spec : itemSpecs()) {
            if (spec.kind != kind)
                continue;
            // placement keys are listed once above
            auto skip = kind == ItemSpec::Kind::Screen ? placementFields()
                                                       : std::set<std::string>{};
            skip.insert(spec.type);
            itemRow(spec.type, itemValueHint(spec.type), joined(spec.fields, skip), mono, px);
            if (spec.type == "arrow")
                keyRow("  inside arrow:", joined(arrowFields()), mono, px);
        }
    }
}

// ── shader stdlib ────────────────────────────────────────────────────────────
// signatures are read from the files, so a new function shows up by itself;
// only the descriptions are kept here

const std::map<std::string, std::string>& stdlibFileDocs()
{
    static const std::map<std::string, std::string> d = {
        {"camera.glsl",   "rays for a 3D shader, and depth against polyscope's scene"},
        {"colormap.glsl", "color maps and value remapping"},
        {"complex.glsl",  "complex numbers on vec2 (x real, y imaginary), domain coloring; includes colormap"},
        {"noise.glsl",    "hashes and procedural noise, no textures"},
        {"plot.glsl",     "curves, grids and axes in the data space set by \"view:\", widths in pixels"},
        {"raymarch.glsl", "sphere tracing a float sceneSDF(vec3 p) you define; includes camera"},
        {"sdf.glsl",      "signed distances in 2D and 3D, and ways to combine them"},
        {"slide.glsl",    "fades and stages that follow the talk"},
    };
    return d;
}

const std::map<std::string, std::string>& stdlibFnDocs()
{
    static const std::map<std::string, std::string> d = {
        // camera
        {"lookAtRay", "ray from ro towards target, lens is the focal length"},
        {"orbitRayAt", "camera around target, orbit = (yaw, pitch) in 0..1"},
        {"orbitRayTarget", "orbit follows the cursor, a 3/4 view otherwise"},
        {"orbitRay", "the same, around the origin"},
        {"screenPoint", "this pixel in the window, 0..1, y up"},
        {"screenToLocal", "a window point in this shader's uv"},
        {"screenAspect", "the window's width / height"},
        {"polyscopeNDC", "this pixel in polyscope's device coordinates"},
        {"polyscopeRay", "ray from polyscope's camera through this pixel"},
        {"polyscopeDepth", "a world point's depth as polyscope stores it"},
        {"sceneDepthHere", "polyscope's depth here, 1 where nothing is drawn"},
        {"visibleOverScene", "true when the point is in front of the 3D scene"},
        {"sceneEyeDistance", "distance to the 3D scene along the view axis"},
        {"sceneClearance", "negative in front of the 3D scene, positive behind"},
        {"sceneOcclusion", "0 visible to 1 hidden, eased over fade"},
        {"sceneWorldPos", "world position of the 3D scene's surface here"},
        // colormap
        {"viridis", "perceptual map, t in 0..1"}, {"magma", "perceptual map, t in 0..1"},
        {"inferno", "perceptual map, t in 0..1"}, {"plasma", "perceptual map, t in 0..1"},
        {"turbo", "rainbow map, most contrast, not uniform"},
        {"grayscale", "black to white"},
        {"coolwarm", "diverging map, zero at t = 0.5"},
        {"cosinePalette", "custom palette from offset, amplitude, frequency, phase"},
        {"remap", "v to 0..1 across [lo, hi]"},
        {"signedRemap", "signed v to 0..1, zero at 0.5"},
        {"hsv2rgb", "hue, saturation, value to rgb"},
        {"isoline", "1 on the isolines of v, grad = length of its screen gradient"},
        // complex
        {"I", "the imaginary unit"}, {"ONE", "the real unit"},
        {"cinv", "1 / a"}, {"cconj", "conjugate"}, {"carg", "argument"}, {"cabs", "modulus"},
        {"clog", "principal logarithm"}, {"cpow", "power"},
        {"mobius", "(az + b) / (cz + d)"},
        {"domainColor", "hue = argument, brightness bands = modulus"},
        {"domainColorGrid", "the same, with spokes argument lines per turn"},
        // noise
        {"hash11", "random 0..1 from a point"}, {"hash12", "random 0..1 from a point"},
        {"hash13", "random 0..1 from a point"}, {"hash22", "random vec2 from a point"},
        {"hash33", "random vec3 from a point"},
        {"valueNoise", "smooth noise, 0..1"},
        {"gradientNoise", "Perlin-style noise, -1..1"},
        {"fbm", "octaves of noise, natural detail"},
        {"ridgedFbm", "fbm with creases"},
        {"domainWarp", "fbm warped by fbm, a flowing look"},
        {"worley", "(nearest, second nearest) distance to cell points"},
        {"curlNoise", "divergence-free 2D flow"},
        // plot
        {"inkClear", "an empty stack of layers"},
        {"inkOver", "adds a layer under the previous ones"},
        {"inkResolve", "the stack as the output color"},
        {"sdGraph", "pixel distance to y = f(x), dfx = f'(x), px = iPixelXY()"},
        {"stroke", "1 within width_px of the distance"},
        {"dashMask", "dashes along x, d = (mark, gap) in pixels"},
        {"gridAxis", "grid lines of one axis"},
        {"gridMask", "grid lines every step"},
        {"axesMask", "the lines x = 0 and y = 0"},
        {"frameMask", "a border inside the rectangle lo..hi"},
        {"dataAt", "a 1D data texture read over span"},
        {"dataSlope", "its slope"},
        {"inSpan", "1 inside span, soft at the ends"},
        {"revealMask", "draws a curve on as u goes 0 -> 1"},
        // raymarch
        {"sceneSDF", "you define it, the scene's distance"},
        {"MARCH_STEPS", "#define before the include to change"},
        {"MARCH_MAX_DIST", "#define before the include to change"},
        {"MARCH_EPS", "#define before the include to change"},
        {"marchScene", "true on a hit, writes the hit point"},
        {"marchDistance", "distance to the hit, MARCH_MAX_DIST if none"},
        {"sceneNormal", "surface normal"},
        {"softShadow", "0 shadowed to 1 lit, sharpness sets the penumbra"},
        {"ambientOcclusion", "0 enclosed to 1 open"},
        {"fresnel", "rim term, strongest at grazing angles"},
        {"checker", "0 or 1 checkerboard"},
        {"shadeDefault", "ready-made lighting with shadow and rim"},
        // sdf
        {"sdBox2", "half_size is half the width and height"},
        {"sdNgon", "regular n-gon of circumradius r"},
        {"sdTriangle", "from its three corners"},
        {"sdArc", "ring wedge, sc = (sin, cos) of the half angle"},
        {"sdPie", "pie slice, c = (sin, cos) of the half angle"},
        {"sdPlane", "plane of normal n at offset h"},
        {"sdTorus", "t = (major radius, minor radius)"},
        {"sdCylinder", "along y, half height h"},
        {"sdCappedCone", "from a (radius ra) to b (radius rb)"},
        {"sdRoundCone", "capsule tapering from r1 to r2"},
        {"sdEllipsoid", "semi-axes r, approximate"},
        {"opSubtract", "b minus a"},
        {"opSmoothUnion", "union with a fillet of size k"},
        {"opShell", "hollow, thickness 2t"},
        {"opRound", "rounds the edges by r"},
        {"opRepeat", "tiles space with period c"}, {"opRepeat2", "tiles space with period c"},
        {"opMirrorX", "mirror across x = 0"},
        {"opRotateY", "rotate around y by a radians"},
        {"opRotate2", "rotate by a radians"},
        // slide
        {"fadeIn", "0 -> 1 over the slide's first seconds"},
        {"fadeInSmooth", "the same, eased"},
        {"fadeOut", "1 -> 0 over the slide's first seconds"},
        {"pulse", "rises over attack, falls over release"},
        {"fadeInAt", "0 -> 1 over seconds once the keyframe is reached"},
        {"fadeInAtSmooth", "the same, eased"},
        {"onceAt", "0 before the keyframe, 1 from it on"},
        {"betweenKeyframes", "1 from the first keyframe until the second"},
        {"stageAfter", "slides since the keyframe, clamped to 0..count"},
        {"stageAfterSmooth", "the same, blended over seconds"},
        {"slideAlpha", "the deck's transition, multiply your color by it"},
        {"shaderTime", "seconds since the shader appeared"},
    };
    return d;
}

struct StdlibEntry { std::string name, signature; };
struct StdlibFile  { std::string file; std::vector<StdlibEntry> entries; };

// superseded headers, kept for old shaders but not offered
bool stdlibHidden(const std::string& file) { return file == "plot2d.glsl"; }

const std::vector<StdlibFile>& stdlib()
{
    static const std::vector<StdlibFile> files = [] {
        std::vector<StdlibFile> out;
        std::error_code ec;
        std::vector<std::filesystem::path> paths;
        for (const auto& e : std::filesystem::directory_iterator(Options::ShaderPath, ec))
            if (e.path().extension() == ".glsl" && !stdlibHidden(e.path().filename().string()))
                paths.push_back(e.path());
        std::sort(paths.begin(), paths.end());

        static const std::regex fn(R"(^(void|bool|int|float|[iu]?vec[234]|mat[234]|Ink)\s+(\w+)\s*\(([^)]*)\))");
        static const std::regex def(R"(^#define\s+([A-Z_][A-Z0-9_]*)\s+(.*\S))");
        static const std::regex space(R"(\s+)");
        // keyframe arguments are written as names, which the compile replaces
        static const std::pair<std::regex, const char*> kf[] = {
            {std::regex(R"(\bint from_kf\b)"), "\"from\""},
            {std::regex(R"(\bint to_kf\b)"), "\"to\""},
            {std::regex(R"(\bint kf\b)"), "\"keyframe\""},
        };
        for (const auto& p : paths) {
            StdlibFile f{p.filename().string(), {}};
            std::ifstream in(p);
            std::string line;
            std::set<std::string> seen;
            while (std::getline(in, line)) {
                std::smatch m;
                StdlibEntry e;
                if (std::regex_search(line, m, fn)) {
                    std::string args = std::regex_replace(m[3].str(), space, " ");
                    for (const auto& [re, name] : kf)
                        args = std::regex_replace(args, re, name);
                    e = {m[2].str(), m[1].str() + " " + m[2].str() + "(" + args + ")"};
                } else if (std::regex_search(line, m, def)) {
                    std::string value = m[2].str();
                    value = value.substr(0, value.find("//"));
                    value.erase(value.find_last_not_of(' ') + 1);
                    e = {m[1].str(), m[1].str() + " " + value};
                } else {
                    continue;
                }
                if (seen.insert(e.signature).second)
                    f.entries.push_back(std::move(e));
            }
            if (!f.entries.empty())
                out.push_back(std::move(f));
        }
        return out;
    }();
    return files;
}

void stdlibTips(ImFont* mono, float px)
{
    if (!ImGui::CollapsingHeader("Standard library", ImGuiTreeNodeFlags_DefaultOpen))
        return;
    prose("#include <file.glsl> for slope's own, #include \"file.glsl\" for the project's.");
    if (stdlib().empty())
        prose("The stdlib folder could not be read.");
    ImGui::Indent();
    for (const auto& f : stdlib()) {
        // searching looks inside every file, open or not
        if (filter().IsActive())
            ImGui::SetNextItemOpen(true);
        const std::string header = "<" + f.file + ">";
        if (!ImGui::CollapsingHeader(header.c_str()))
            continue;
        if (auto it = stdlibFileDocs().find(f.file); it != stdlibFileDocs().end())
            prose(it->second.c_str());
        for (const auto& e : f.entries) {
            auto it = stdlibFnDocs().find(e.name);
            apiRow(e.signature.c_str(), it == stdlibFnDocs().end() ? "" : it->second.c_str(), mono, px);
        }
    }
    ImGui::Unindent();
}

void shaderTips(ImFont* mono, float px)
{
    if (ImGui::CollapsingHeader("Inputs", ImGuiTreeNodeFlags_DefaultOpen)) {
        apiRow("out vec4 fragColor", "write the pixel's color here", mono, px);
        apiRow("vec2 iResolution", "render size, in pixels", mono, px);
        apiRow("float iAspect", "width / height", mono, px);
        apiRow("float iTime", "seconds since the shader appeared", mono, px);
        apiRow("float iTimeDelta", "seconds since the last frame", mono, px);
        apiRow("int iFrame", "frames rendered so far", mono, px);
        apiRow("vec4 iMouse", "xy cursor in pixels, zw last click", mono, px);
        apiRow("vec2 iMouseNorm", "cursor in 0..1 across the shader, y up", mono, px);
        apiRow("float iHovered", "1 while the cursor is over the shader", mono, px);
        apiRow("vec4 iDate", "year, month, day, seconds since midnight", mono, px);
        apiRow("sampler2D iChannel0..3", "textures from the deck's \"textures:\"", mono, px);
        apiRow("vec3 iChannelResolution[4]", "their sizes, in pixels", mono, px);
    }
    if (ImGui::CollapsingHeader("Coordinates", ImGuiTreeNodeFlags_DefaultOpen)) {
        apiRow("vec2 iUV()", "0..1 across the shader, y up", mono, px);
        apiRow("vec2 iUVc()", "centred, y in -1..1, x scaled by the aspect", mono, px);
        apiRow("vec2 iWorld()", "the pixel in the world set by \"view:\"", mono, px);
        apiRow("float iPixel()", "one pixel, in world units", mono, px);
        apiRow("vec2 iPixelXY()", "one pixel, per axis", mono, px);
    }
    if (ImGui::CollapsingHeader("Slide time", ImGuiTreeNodeFlags_DefaultOpen)) {
        apiRow("float from_begin", "seconds since the show started", mono, px);
        apiRow("float from_action", "seconds since the last slide change", mono, px);
        apiRow("float slide_progress", "0 -> 1 across a slide change", mono, px);
        apiRow("float transition_parameter", "0 -> 1 across this shader's intro or outro", mono, px);
        apiRow("int absolute_frame_number", "current slide index", mono, px);
        apiRow("int relative_frame_number", "slides since this shader appeared", mono, px);
        apiRow("float slidePosition()", "continuous slide index", mono, px);
    }
    if (ImGui::CollapsingHeader("Keyframes", ImGuiTreeNodeFlags_DefaultOpen)) {
        apiRow("bool afterKeyframe(\"keyframe\")", "reached, and stays true", mono, px);
        apiRow("bool beforeKeyframe(\"keyframe\")", "not reached yet", mono, px);
        apiRow("bool atKeyframe(\"keyframe\")", "on that very slide", mono, px);
        apiRow("float sinceKeyframe(\"keyframe\")", "eases 0 -> 1 as it is reached", mono, px);
        apiRow("float duringKeyframe(\"keyframe\")", "eases 0 -> 1 -> 0 around it", mono, px);
        apiRow("float duringKeyframe(\"from\", \"to\")", "the same, spanning both", mono, px);
        apiRow("int slidesSinceKeyframe(\"keyframe\")", "slides since it was reached", mono, px);
        apiRow("float secondsSinceKeyframe(\"keyframe\")", "0 until reached", mono, px);
    }
    stdlibTips(mono, px);
    if (ImGui::CollapsingHeader("Notes", ImGuiTreeNodeFlags_DefaultOpen)) {
        prose("Inputs, coordinates, slide time and keyframes are declared for you, unless the file starts with its own "
              "\"#version\" line.");
    }
}

void snippetTips(ImFont* mono, float px)
{
    if (ImGui::CollapsingHeader("Sections", ImGuiTreeNodeFlags_DefaultOpen)) {
        codeBlock(R"(--- envelope
-- a value
return math.sin(t.from_begin)

--- lattice
-- a table, one name per key
return { z1 = complex(1, 0.5) }

--- wobble
-- a function
return function(p, i)
  return p + vec3(0, 0, envelope)
end

--- fig/xrange
-- a name grouped with "/")", mono, px, "--");
        prose("Sections read each other by name, in any order.");
    }
    if (ImGui::CollapsingHeader("Slide time", ImGuiTreeNodeFlags_DefaultOpen)) {
        apiRow("t.from_begin", "seconds since the show started", mono, px);
        apiRow("t.from_action", "seconds since the last slide change", mono, px);
        apiRow("t.delta_time", "seconds since the last frame", mono, px);
        apiRow("t.slide_progress", "0 -> 1 across a slide change", mono, px);
        apiRow("t.transition_parameter", "the same, 0 -> 1 across a slide change", mono, px);
        apiRow("t.absolute_frame_number", "current slide index", mono, px);
        apiRow("t:slidePosition()", "continuous slide index", mono, px);
    }
    if (ImGui::CollapsingHeader("Keyframes", ImGuiTreeNodeFlags_DefaultOpen)) {
        apiRow("t:afterKeyframe(\"keyframe\")", "reached, and stays true", mono, px);
        apiRow("t:beforeKeyframe(\"keyframe\")", "not reached yet", mono, px);
        apiRow("t:atKeyframe(\"keyframe\")", "on that very slide", mono, px);
        apiRow("t:sinceKeyframe(\"keyframe\")", "eases 0 -> 1 as it is reached", mono, px);
        apiRow("t:duringKeyframe(\"keyframe\")", "eases 0 -> 1 -> 0 around it", mono, px);
        apiRow("t:slidesSinceKeyframe(\"keyframe\")", "slides since it was reached", mono, px);
        apiRow("t:secondsSinceKeyframe(\"keyframe\")", "0 until reached", mono, px);
    }
    if (ImGui::CollapsingHeader("Built-ins", ImGuiTreeNodeFlags_DefaultOpen)) {
        apiRow("param(\"name\", default, min, max)", "a slider in the Tuner, saved to params.json", mono, px);
        apiRow("vec2(x, y)  vec3(x, y, z)", "with :norm() :dot() :cross()", mono, px);
        apiRow("complex(re, im)  cis(angle)", "with :abs() :arg() :conj()", mono, px);
        apiRow("smoothstep, math, string", "plus Lua's own libraries", mono, px);
    }
}

} // namespace

bool WritingTips::available(const std::filesystem::path& file)
{
    return kindOf(file) != Kind::None;
}

void WritingTips::draw(const std::filesystem::path& file, ImFont* mono, float px)
{
    filterBox();
    switch (kindOf(file)) {
    case Kind::Deck:    deckTips(mono, px);    break;
    case Kind::Shader:  shaderTips(mono, px);  break;
    case Kind::Snippet: snippetTips(mono, px); break;
    case Kind::None:    break;
    }
}

} // namespace slope
