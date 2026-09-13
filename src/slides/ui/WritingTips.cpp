#include "slides/ui/WritingTips.h"

#include "content/screen_primitives/gpu/Shader.h"
#include "content/authoring/Snippet.h"
#include "slides/deck/items/DeckItem.h"

#include "imgui.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace slope {

namespace {

enum class Kind { None, Deck, Shader, Snippet };

const ImVec4 kKey    (0.545f, 0.914f, 0.992f, 1.f);   // Dracula cyan
const ImVec4 kComment(0.384f, 0.447f, 0.643f, 1.f);   // Dracula comment

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
        ImGui::TextUnformatted(value.c_str());
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
figures:           # a group
  - image: plot.png
    at: fig
slides:
  - frame:
      - title: A title
      - latex: some text
        at: intro  # views/intro.pos
      - step       # next click
      - formula: e^{i\pi} = -1
        below: intro
  - frame:
      - title: Results
      - figures)", mono, px, "#");
    }

    auto docRows = [&](const KeyDoc& keys) {
        for (const auto& [key, doc] : keys)
            apiRow((key + ":").c_str(), doc.c_str(), mono, px);
    };
    if (ImGui::CollapsingHeader("Top level", ImGuiTreeNodeFlags_DefaultOpen)) {
        docRows(deckTopLevelKeys());
        apiRow("<name>:", "any other list is a group of items, used as \"- name\" in a frame", mono, px);
    }
    if (ImGui::CollapsingHeader("In slides", ImGuiTreeNodeFlags_DefaultOpen)) {
        docRows(frameKeys());
        apiRow("- step", "what follows shows on the next click", mono, px);
        apiRow("- <name>", "puts a top-level group here", mono, px);
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
    if (ImGui::CollapsingHeader("Notes", ImGuiTreeNodeFlags_DefaultOpen)) {
        prose("All of the above is declared for you, unless the file starts with its own "
              "\"#version\" line.");
        prose("#include \"file.glsl\" is resolved next to this file.");
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
