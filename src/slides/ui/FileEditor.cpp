#include "slides/ui/FileEditor.h"

#include "content/screen_primitives/gpu/Shader.h"
#include "content/screen_primitives/text/Code.h"
#include "content/screen_primitives/text/Algorithm.h"
#include "content/authoring/Snippet.h"
#include "content/screen_primitives/text/LateX.h"
#include "content/config/ReloadErrors.h"
#include "content/screen_primitives/layout/Anchor.h"

#include "imgui.h"
#include "imgui_internal.h"
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <climits>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <functional>
#include <string_view>

namespace slope {

namespace {

constexpr float kBasePx = 16.f;   // editor font size at text_scale 1
constexpr int kBgAlpha = 235;    // the slide stays faintly visible behind the text

std::vector<std::filesystem::path>& extras()
{
    static std::vector<std::filesystem::path> v;
    return v;
}

std::filesystem::path normalized(const std::filesystem::path& p)
{
    std::error_code ec;
    auto v = std::filesystem::weakly_canonical(p, ec);
    return ec ? p : v;
}

std::string modNames(int mods)
{
    std::string out;
    auto add = [&](int bit, const char* name) {
        if (mods & bit) out += (out.empty() ? "" : "+") + std::string(name);
    };
    add(ImGuiMod_Ctrl, "Ctrl");
    add(ImGuiMod_Shift, "Shift");
    add(ImGuiMod_Alt, "Alt");
    add(ImGuiMod_Super, "Super");
    return out.empty() ? "no modifier" : out;
}

std::string shortName(const std::filesystem::path& p)
{
    auto parent = p.parent_path().filename();
    return parent.empty() ? p.filename().string()
                          : (parent / p.filename()).string();
}

} // namespace

void FileEditor::registerExtra(const std::filesystem::path& p)
{
    auto v = normalized(p);
    auto& e = extras();
    if (std::find(e.begin(), e.end(), v) == e.end())
        e.push_back(v);
}

void FileEditor::refreshFileList()
{
    std::vector<std::filesystem::path> all;
    // normalised so every registry compares equal to `current`
    auto add = [&](const std::vector<std::filesystem::path>& v) {
        for (const auto& raw : v) {
            auto p = normalized(raw);
            if (std::find(all.begin(), all.end(), p) == all.end())
                all.push_back(p);
        }
    };
    add(Shader::WatchedFiles());
    add(Snippet::WatchedFiles());
    add(Code::WatchedFiles());
    add(Algorithm::WatchedFiles());
    add(Latex::WatchedFiles());
    add(extras());

    std::sort(all.begin(), all.end(), [](const auto& a, const auto& b) {
        return a.filename() < b.filename();
    });
    files = std::move(all);
    missing.clear();
    for (const auto& p : files) {
        std::error_code ec;
        if (!std::filesystem::exists(p, ec))
            missing.insert(p);
    }
}

void FileEditor::requestOpen(const std::filesystem::path& p)
{
    if (p == current)
        return;
    if (dirty) {
        pending = Pending::Switch;
        pending_file = p;
        return;
    }
    openFile(p);
}

void FileEditor::openFile(const std::filesystem::path& p)
{
    current = p;
    loadFromDisk();
}

void FileEditor::loadFromDisk()
{
    load_failed = false;
    dirty = false;
    save_error.clear();
    buffer.clear();
    widget_reload = true;
    indent_pending = false;
    pending_insert.clear();
    comment_pending = false;
    hl_hash = 0;
    runs.clear();
    if (current.empty())
        return;

    std::ifstream f(current, std::ios::binary);
    if (!f) {
        load_failed = true;
        return;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    buffer = ss.str();

    std::error_code ec;
    disk_mtime = std::filesystem::last_write_time(current, ec);

    std::string ext = current.extension().string();
    if (!ext.empty() && ext.front() == '.') ext.erase(0, 1);
    const auto& lang = CodeLanguage::ForExtension(ext);
    language = lang.valid() ? lang.name : std::string();
}

bool FileEditor::changedOnDisk() const
{
    if (current.empty())
        return false;
    std::error_code ec;
    auto t = std::filesystem::last_write_time(current, ec);
    return !ec && t != disk_mtime;
}

// Dracula palette for the editor: Code's CodeStyle is tuned for a light slide,
// so its inks would vanish on the dark editor background
const CodeStyle& FileEditor::editorStyle()
{
    static const CodeStyle s = [] {
        auto hex = [](int r, int g, int b) {
            return Color(r / 255.f, g / 255.f, b / 255.f);
        };
        CodeStyle c;
        c.text     = hex(0xF8, 0xF8, 0xF2);   // foreground
        c.keyword  = hex(0xFF, 0x79, 0xC6);   // pink
        c.type     = hex(0x8B, 0xE9, 0xFD);   // cyan
        c.comment  = hex(0x82, 0x8F, 0xBE);   // muted blue, lifted off Dracula's #6272A4
        c.literal  = hex(0xF1, 0xFA, 0x8C);   // yellow (strings)
        c.preproc  = hex(0xFF, 0x79, 0xC6);   // pink
        c.function = hex(0x50, 0xFA, 0x7B);   // green
        c.constant = hex(0xBD, 0x93, 0xF9);   // purple (numbers, constants)
        c.variable = hex(0xF8, 0xF8, 0xF2);   // foreground
        c.op       = hex(0xFF, 0x79, 0xC6);   // pink
        return c;
    }();
    return s;
}

void FileEditor::rehighlight()
{
    if (language.empty()) { runs.clear(); return; }
    std::size_t h = std::hash<std::string>{}(buffer);
    if (h == hl_hash) return;
    hl_hash = h;
    runs = Code::HighlightRuns(buffer, CodeLanguage::ForName(language), editorStyle());
}

// written aside then renamed, so a failed write never truncates the original
bool FileEditor::saveToDisk()
{
    namespace fs = std::filesystem;
    if (current.empty())
        return false;
    save_error.clear();

    fs::path tmp = current;
    tmp += ".slope-save";
    std::error_code ec;
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (f) {
            f.write(buffer.data(), std::streamsize(buffer.size()));
            f.close();
        }
        if (!f) {
            save_error = "could not write " + tmp.string();
            fs::remove(tmp, ec);
            spdlog::error("[file-editor] {}", save_error);
            return false;
        }
    }

    const auto st = fs::status(current, ec);
    if (!ec && fs::exists(st))
        fs::permissions(tmp, st.permissions(), ec);
    fs::rename(tmp, current, ec);
    if (ec) {
        save_error = "could not replace " + current.string() + ": " + ec.message();
        std::error_code ignored;
        fs::remove(tmp, ignored);
        spdlog::error("[file-editor] {}", save_error);
        return false;
    }

    disk_mtime = fs::last_write_time(current, ec);
    dirty = false;
    spdlog::info("[file-editor] saved {}", current.string());
    return true;
}

void FileEditor::drawPendingPopup()
{
    const char* title = "Unsaved changes##file-editor";
    if (pending == Pending::None)
        return;
    if (!ImGui::IsPopupOpen(title))
        ImGui::OpenPopup(title);
    if (!ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    const std::string name = current.filename().string();
    auto done = [&] { pending = Pending::None; ImGui::CloseCurrentPopup(); };

    switch (pending) {
    case Pending::Switch:
        ImGui::Text("%s has unsaved changes.", name.c_str());
        if (changedOnDisk())
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.f, 1.f), "It also changed on disk; saving overwrites that.");
        if (ImGui::Button("Save")) {
            if (saveToDisk()) openFile(pending_file);
            done();
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard")) { openFile(pending_file); done(); }
        break;
    case Pending::Reload:
        ImGui::Text("Discard your changes to %s and reload it from disk?", name.c_str());
        if (ImGui::Button("Reload")) { loadFromDisk(); done(); }
        break;
    case Pending::Overwrite:
        ImGui::Text("%s changed on disk since it was loaded.", name.c_str());
        if (ImGui::Button("Overwrite")) { saveToDisk(); done(); }
        ImGui::SameLine();
        if (ImGui::Button("Reload from disk")) { loadFromDisk(); done(); }
        break;
    case Pending::None:
        break;
    }
    if (pending != Pending::None) {
        ImGui::SameLine();
        if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            done();
    }
    ImGui::EndPopup();
}

bool FileEditor::isYaml() const
{
    const auto ext = current.extension();
    return ext == ".yaml" || ext == ".yml";
}

// the previous line's indentation, one level deeper in yaml after "- " or a trailing ":"
std::string FileEditor::indentAfter(const char* buf, int cursor) const
{
    const int end = std::max(0, cursor - 1);
    int begin = end;
    while (begin > 0 && buf[begin - 1] != '\n')
        --begin;
    const std::string_view line(buf + begin, size_t(end - begin));
    const size_t n = line.find_first_not_of(" \t");
    std::string indent(line.substr(0, n == std::string_view::npos ? line.size() : n));
    if (!isYaml() || n == std::string_view::npos)
        return indent;

    std::string_view rest = line.substr(n);
    while (!rest.empty() && (rest.back() == ' ' || rest.back() == '\t' || rest.back() == '\r'))
        rest.remove_suffix(1);
    if (rest == "-" || rest.starts_with("- "))
        indent += "  ";
    if (rest.ends_with(":") || rest.ends_with(": |") || rest.ends_with(": >"))
        indent += "  ";
    return indent;
}

std::string FileEditor::commentMarker() const
{
    std::string ext = current.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    if (ext == ".yaml" || ext == ".yml" || ext == ".py" || ext == ".sh" || ext == ".cmake")
        return "#";
    if (ext == ".lua")
        return "--";
    if (ext == ".tex" || ext == ".sty")
        return "%";
    if (ext == ".glsl" || ext == ".frag" || ext == ".vert" || ext == ".comp" || ext == ".geom"
        || ext == ".c" || ext == ".h" || ext == ".cpp" || ext == ".hpp" || ext == ".js")
        return "//";
    return "";
}

// every line the selection touches, or the caret's; uncommented when all already are
void FileEditor::toggleComment(ImGuiInputTextCallbackData* d) const
{
    const std::string m = commentMarker();
    if (m.empty())
        return;
    const bool had_selection = d->SelectionStart != d->SelectionEnd;
    int a = std::min(d->SelectionStart, d->SelectionEnd);
    int b = std::max(d->SelectionStart, d->SelectionEnd);
    if (!had_selection)
        a = b = d->CursorPos;
    // a selection ending at a line start leaves that line alone
    if (b > a && d->Buf[b - 1] == '\n')
        --b;

    auto lineStart = [&](int p) { while (p > 0 && d->Buf[p - 1] != '\n') --p; return p; };
    auto lineEnd = [&](int p) { while (p < d->BufTextLen && d->Buf[p] != '\n') ++p; return p; };
    auto indentOf = [&](int s) { int i = s; while (i < d->BufTextLen && (d->Buf[i] == ' ' || d->Buf[i] == '\t')) ++i; return i - s; };

    std::vector<int> starts;
    for (int s = lineStart(a);;) {
        starts.push_back(s);
        const int e = lineEnd(s);
        if (e >= b || e >= d->BufTextLen)
            break;
        s = e + 1;
    }

    bool all = true, any = false;
    int min_indent = INT_MAX;
    for (int s : starts) {
        const int ind = indentOf(s);
        if (s + ind == lineEnd(s))
            continue;   // blank lines are left as they are
        any = true;
        min_indent = std::min(min_indent, ind);
        if (std::string_view(d->Buf + s + ind, size_t(lineEnd(s) - s - ind)).substr(0, m.size()) != m)
            all = false;
    }
    if (!any)
        return;

    const int first = starts.front();
    const int last_end = lineEnd(starts.back());
    const int caret = d->CursorPos;
    int delta = 0;
    // bottom up, so the lines still to edit keep their offsets
    for (auto it = starts.rbegin(); it != starts.rend(); ++it) {
        const int s = *it;
        const int ind = indentOf(s);
        if (s + ind == lineEnd(s))
            continue;
        if (all) {
            int n = int(m.size());
            if (d->Buf[s + ind + n] == ' ')
                ++n;
            d->DeleteChars(s + ind, n);
            delta -= n;
        } else {
            const std::string ins = m + " ";
            d->InsertChars(s + min_indent, ins.c_str());
            delta += int(ins.size());
        }
    }
    if (had_selection) {
        d->SelectionStart = first;
        d->SelectionEnd = d->CursorPos = last_end + delta;
    } else {
        d->CursorPos = std::clamp(caret + delta, first, d->BufTextLen);
        d->SelectionStart = d->SelectionEnd = d->CursorPos;
    }
}

// a shader gets the smallest body that compiles, so creating it is not an error
bool FileEditor::createFile()
{
    namespace fs = std::filesystem;
    save_error.clear();
    std::error_code ec;
    fs::create_directories(current.parent_path(), ec);
    std::string ext = current.extension().string();
    const std::string starter = ext == ".frag" ? "void main() {\n    fragColor = vec4(0.0);\n}\n"
                              : ext == ".json" ? "{}\n"
                              : "";
    {
        std::ofstream f(current, std::ios::binary);
        if (f) {
            f << starter;
            f.close();
        }
        if (!f) {
            save_error = "could not create " + current.string();
            spdlog::error("[file-editor] {}", save_error);
            return false;
        }
    }
    missing.erase(current);
    spdlog::info("[file-editor] created {}", current.string());
    loadFromDisk();
    return true;
}

int FileEditor::inputCallback(ImGuiInputTextCallbackData* data)
{
    auto* self = static_cast<FileEditor*>(data->UserData);
    switch (data->EventFlag) {
    case ImGuiInputTextFlags_CallbackResize:
        // lets ImGui grow the std::string in place, the trick imgui_stdlib uses
        self->buffer.resize(data->BufTextLen);
        data->Buf = self->buffer.data();
        break;
    case ImGuiInputTextFlags_CallbackCharFilter:
        // Enter only, a pasted newline keeps the text as it came
        if (data->EventChar == '\n'
            && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))) {
            self->indent_pending = true;
            self->enter_handled = true;
        }
        else if (data->EventChar == '\t' && self->isYaml()) {
            self->pending_insert = "  ";   // a tab is not yaml indentation
            return 1;
        }
        break;
    case ImGuiInputTextFlags_CallbackAlways:
        // ImGui refuses Enter while it believes a modifier is held, so the newline is made here
        if (self->enter_raw && !self->enter_handled) {
            self->enter_raw = false;
            if (data->SelectionStart != data->SelectionEnd) {
                const int a = std::min(data->SelectionStart, data->SelectionEnd);
                data->DeleteChars(a, std::abs(data->SelectionEnd - data->SelectionStart));
                data->CursorPos = a;
            }
            const int at = data->CursorPos;
            data->InsertChars(at, "\n");
            data->CursorPos = data->SelectionStart = data->SelectionEnd = at + 1;
            self->indent_pending = true;
            const int mods = ImGui::GetIO().KeyMods;
            if (mods != self->logged_mods) {
                self->logged_mods = mods;
                spdlog::warn("[file-editor] ImGui dropped Enter ({} held for it), newline inserted by hand",
                             modNames(mods));
            }
        }
        if (self->comment_pending) {
            self->comment_pending = false;
            self->toggleComment(data);
        }
        if (self->indent_pending) {
            self->indent_pending = false;
            const std::string indent = self->indentAfter(data->Buf, data->CursorPos);
            if (!indent.empty()) {
                const int at = data->CursorPos;
                data->InsertChars(at, indent.c_str());
                data->CursorPos = data->SelectionStart = data->SelectionEnd = at + int(indent.size());
            }
        }
        if (!self->pending_insert.empty()) {
            const int at = data->CursorPos;
            data->InsertChars(at, self->pending_insert.c_str());
            data->CursorPos = data->SelectionStart = data->SelectionEnd = at + int(self->pending_insert.size());
            self->pending_insert.clear();
        }
        break;
    default:
        break;
    }
    return 0;
}

void FileEditor::draw(WindowManager& wm)
{
    if (!wm.isOpen(WindowType::FileEditor))
        return;

    // loaded on open so the lookup hitch never eats a keystroke
    if (!font_tried) {
        font_tried = true;
        mono = Code::LoadFont("Fira Code", kBasePx);
    }

    // the file set changes as slides come and go; twice a second is plenty
    {
        static auto stamp = Time::now();
        if (last_refresh < 0 || TimeFrom(stamp) > 0.5) {
            refreshFileList();
            stamp = Time::now();
            last_refresh = 0;
        }
    }

    bool open = true;
    ImGui::SetNextWindowSize(ImVec2(900, 560), ImGuiCond_FirstUseEver);
    const bool visible = ImGui::Begin("Hot-reload files (E)", &open);
    if (!visible) {
        ImGui::End();
        if (!open) wm.CloseAll();
        return;
    }
    const bool win_focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    const auto errors = ReloadErrors::all();

    // ── left: the file list ────────────────────────────────────────────────
    ImGui::BeginChild("list", ImVec2(260, 0), ImGuiChildFlags_Borders);
    if (files.empty())
        ImGui::TextDisabled("nothing watched yet");
    for (const auto& p : files) {
        bool sel = (p == current);
        const bool broken = errors.count(p) > 0;
        const bool absent = missing.count(p) > 0;
        if (broken || absent)
            ImGui::PushStyleColor(ImGuiCol_Text, broken ? ImVec4(1.f, 0.4f, 0.35f, 1.f)
                                                        : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        if (ImGui::Selectable(shortName(p).c_str(), sel))
            requestOpen(p);
        if (broken || absent)
            ImGui::PopStyleColor();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", p.string().c_str());
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // ── right: the editor ──────────────────────────────────────────────────
    ImGui::BeginChild("edit", ImVec2(0, 0));
    const ImGuiID body_id = ImGui::GetID("##body");

    // the widget's own Escape reverts every edit, so it never sees one
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && pending == Pending::None) {
        if (ImGui::GetActiveID() == body_id)
            ImGui::ClearActiveID();
        else if (win_focused)
            open = false;
    }

    // Enter as pressed, Ctrl+Enter excepted since ImGui makes it leave the field
    enter_raw = ImGui::GetActiveID() == body_id && !ImGui::GetIO().KeyCtrl
             && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter));
    enter_handled = false;

    // Ctrl+/, and the key that types '/' on an AZERTY layout, which ImGui sees as Period
    if (ImGui::GetActiveID() == body_id && ImGui::GetIO().KeyCtrl
        && (ImGui::IsKeyPressed(ImGuiKey_Slash, false) || ImGui::IsKeyPressed(ImGuiKey_Period, false)))
        comment_pending = true;

    // N types a letter here, so Ctrl+N puts the free label in at the cursor
    if (ImGui::GetActiveID() == body_id && ImGui::GetIO().KeyCtrl
        && ImGui::IsKeyPressed(ImGuiKey_N, false)) {
        const std::string l = LabelAnchor::suggestLabel();
        if (l.empty())
            spdlog::warn("[file-editor] could not find a free label to suggest");
        else
            pending_insert += l;
    }

    std::error_code exists_ec;
    if (current.empty()) {
        ImGui::TextDisabled("select a file");
    } else if (!dirty && !std::filesystem::exists(current, exists_ec)) {
        ImGui::TextUnformatted(current.filename().string().c_str());
        ImGui::TextDisabled("%s does not exist yet", current.string().c_str());
        if (ImGui::Button("Create file"))
            createFile();
        if (!save_error.empty())
            ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "%s", save_error.c_str());
    } else {
        // a clean buffer follows the disk; a dirty one is flagged instead
        bool stale = changedOnDisk();
        if (stale && !dirty) {
            loadFromDisk();
            stale = false;
        }

        ImGui::TextUnformatted(current.filename().string().c_str());
        ImGui::SameLine();
        if (dirty && stale)
            ImGui::TextColored(ImVec4(1.f, 0.4f, 0.3f, 1.f), "[modified, also changed on disk]");
        else if (dirty)
            ImGui::TextColored(ImVec4(1.f, 0.7f, 0.2f, 1.f), "[modified]");

        const bool ctrl_s = ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false);
        if (ImGui::Button("Save") || (ctrl_s && dirty)) {
            if (stale) pending = Pending::Overwrite;
            else saveToDisk();
        }
        ImGui::SameLine();
        if (ImGui::Button("Reload from disk")) {
            if (dirty) pending = Pending::Reload;
            else loadFromDisk();
        }
        if (load_failed) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "could not read file");
        }

        ImGui::SameLine();
        if (language.empty())
            ImGui::TextDisabled("no grammar");
        else
            ImGui::TextDisabled("%s", language.c_str());

        ImGui::SameLine();
        ImGui::SetNextItemWidth(140);
        // quarter steps, so only a handful of sizes get baked
        if (ImGui::SliderFloat("size", &text_scale, 0.75f, 3.0f, "%.2fx"))
            text_scale = std::round(text_scale * 4.f) / 4.f;

        if (!save_error.empty())
            ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "%s", save_error.c_str());

        // what the last reload of this file said, at most 8 lines before it scrolls
        if (auto it = errors.find(current); it != errors.end()) {
            const int n = 1 + int(std::count(it->second.begin(), it->second.end(), '\n'));
            const float h = float(std::min(n, 8)) * ImGui::GetTextLineHeightWithSpacing()
                          + 2.f * ImGui::GetStyle().WindowPadding.y;
            ImGui::BeginChild("errors", ImVec2(0, h), ImGuiChildFlags_Borders);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.45f, 0.4f, 1.f));
            ImGui::TextUnformatted(it->second.c_str());
            ImGui::PopStyleColor();
            ImGui::EndChild();
        }

        ImGui::Separator();

        rehighlight();

        // an active field keeps its own copy of the text
        if (widget_reload) {
            widget_reload = false;
            if (ImGui::GetActiveID() == body_id)
                if (ImGuiInputTextState* st = ImGui::GetInputTextState(body_id))
                    st->ReloadUserBufAndKeepSelection();
        }

        ImGuiWindow* edit_win = ImGui::GetCurrentWindow();
        ImVec2 avail = ImGui::GetContentRegionAvail();
        // a null face keeps the current one, so the size still applies
        ImGui::PushFont(mono, kBasePx * text_scale);

        // a gutter for the line numbers, as wide as the largest one
        const int n_lines = 1 + int(std::count(buffer.begin(), buffer.end(), '\n'));
        const float digit_w = ImGui::CalcTextSize("0").x;
        const float gutter = (float(std::max<size_t>(std::to_string(n_lines).size(), 2)) + 1.5f) * digit_w;
        const float gutter_x = ImGui::GetCursorScreenPos().x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + gutter);
        avail.x -= gutter;

        // the widget's own ink is transparent, every glyph is drawn below in the Dracula palette
        const ImU32 ink = ImU32(ImColor(editorStyle().text.getImColor()));
        ImGui::PushStyleColor(ImGuiCol_FrameBg,        IM_COL32(0x28, 0x2A, 0x36, kBgAlpha));
        ImGui::PushStyleColor(ImGuiCol_Text,           IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, IM_COL32(0x44, 0x47, 0x5A, 255));

        if (ImGui::InputTextMultiline("##body", buffer.data(), buffer.size() + 1,
                                      avail,
                                      ImGuiInputTextFlags_AllowTabInput
                                          | ImGuiInputTextFlags_CallbackResize
                                          | ImGuiInputTextFlags_CallbackCharFilter
                                          | ImGuiInputTextFlags_CallbackAlways,
                                      inputCallback, this)) {
            dirty = true;
        }
        ImGui::PopStyleColor(3);

        {
            const ImVec2 p_min = ImGui::GetItemRectMin();
            const ImVec2 p_max = ImGui::GetItemRectMax();

            // vertical scroll lives on the multiline's own child window, focused or not
            char child_name[256];
            ImFormatString(child_name, sizeof(child_name), "%s/##body_%08X",
                           edit_win->Name, body_id);
            ImGuiWindow* body_win = ImGui::FindWindowByName(child_name);
            ImVec2 scroll = body_win ? body_win->Scroll : ImVec2(0.f, 0.f);
            // the state outlives the field's focus, and may describe another file
            const bool active = ImGui::GetActiveID() == body_id;
            ImGuiInputTextState* st = active ? ImGui::GetInputTextState(body_id) : nullptr;
            if (st) scroll.x = st->Scroll.x;   // horizontal is tracked on the state

            const ImGuiStyle& gs = ImGui::GetStyle();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImFont* font = ImGui::GetFont();
            const float fs = ImGui::GetFontSize();
            const ImVec2 origin(p_min.x + gs.FramePadding.x - scroll.x,
                                p_min.y + gs.FramePadding.y - scroll.y);
            const char* base = buffer.c_str();
            auto measure = [&](size_t a, size_t b) {
                return font->CalcTextSizeA(fs, FLT_MAX, 0.f, base + a, base + b).x;
            };

            // [begin,end) of every line, trailing '\n' excluded
            std::vector<std::pair<size_t, size_t>> lines;
            for (size_t i = 0, b = 0; i <= buffer.size(); ++i)
                if (i == buffer.size() || buffer[i] == '\n') { lines.push_back({b, i}); b = i + 1; }

            const int n = int(lines.size());
            const int first = std::max(0, int(scroll.y / fs) - 1);
            const int lastl = std::min(n, first + int((p_max.y - p_min.y) / fs) + 3);

            int cpos = 0, cl = -1;
            if (st) {
                cpos = std::clamp(st->GetCursorPos(), 0, int(buffer.size()));
                cl = 0;
                while (cl + 1 < n && int(lines[cl + 1].first) <= cpos) ++cl;
            }

            // right aligned, on the text's own line positions so they scroll with it
            const ImVec2 g_min(gutter_x, p_min.y), g_max(p_min.x, p_max.y);
            dl->AddRectFilled(g_min, g_max, IM_COL32(0x21, 0x22, 0x2C, kBgAlpha));
            dl->PushClipRect(g_min, g_max, true);
            const ImU32 muted = IM_COL32(0x62, 0x72, 0xA4, 255);
            char num[16];
            for (int li = first; li < lastl; ++li) {
                const int len = std::snprintf(num, sizeof(num), "%d", li + 1);
                const float w = font->CalcTextSizeA(fs, FLT_MAX, 0.f, num, num + len).x;
                dl->AddText(font, fs, ImVec2(p_min.x - 0.75f * digit_w - w, origin.y + float(li) * fs),
                            li == cl ? ink : muted, num, num + len);
            }
            dl->PopClipRect();

            dl->PushClipRect(p_min, p_max, true);
            for (int li = first; li < lastl; ++li) {
                const auto [b, e] = lines[li];
                const float y = origin.y + float(li) * fs;
                float x = origin.x;
                size_t seg = b;
                // first run that reaches into this line
                size_t k = 0;
                while (k < runs.size() && runs[k].end <= seg) ++k;
                while (seg < e) {
                    while (k < runs.size() && runs[k].end <= seg) ++k;
                    ImU32 col;
                    size_t seg_end;
                    if (k < runs.size() && runs[k].begin <= seg) {
                        col = runs[k].color;
                        seg_end = std::min(e, runs[k].end);
                    } else {
                        col = ink;
                        seg_end = (k < runs.size()) ? std::min(e, runs[k].begin) : e;
                    }
                    if (seg_end <= seg) break;
                    dl->AddText(font, fs, ImVec2(x, y), col, base + seg, base + seg_end);
                    x += measure(seg, seg_end);
                    seg = seg_end;
                }
            }

            // our own caret, the widget's is transparent too
            if (st) {
                const float cx = origin.x + measure(lines[cl].first, size_t(cpos));
                const float cy = origin.y + float(cl) * fs;
                if (std::fmod(st->CursorAnim, 1.2f) <= 0.8f)
                    dl->AddLine(ImVec2(cx, cy + 1.f), ImVec2(cx, cy + fs - 1.f), ink, 1.f);
            }
            dl->PopClipRect();
        }

        ImGui::PopFont();
    }

    ImGui::EndChild();

    // the fullscreen slide window hands every uncaptured click to polyscope's camera
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows
                               | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem
                               | ImGuiHoveredFlags_AllowWhenBlockedByPopup)
        || pending != Pending::None
        || (win_focused && ImGui::IsAnyItemActive()))
        ImGui::SetNextFrameWantCaptureMouse(true);

    drawPendingPopup();
    ImGui::End();

    // closing keeps the buffer, edits included; quitting asks about them
    if (!open)
        wm.CloseAll();
}

} // namespace slope
