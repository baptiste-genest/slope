#include "slides/ui/FileEditor.h"

#include "content/screen_primitives/gpu/Shader.h"
#include "content/screen_primitives/text/Code.h"
#include "content/screen_primitives/text/Algorithm.h"
#include "content/authoring/Snippet.h"

#include "imgui.h"
#include "imgui_internal.h"
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <functional>

namespace slope {

namespace {

constexpr float kBasePx = 16.f;   // editor font size at text_scale 1

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

// lets ImGui grow a std::string in place, the trick imgui_stdlib uses
int growString(ImGuiInputTextCallbackData* data)
{
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        auto* s = static_cast<std::string*>(data->UserData);
        s->resize(data->BufTextLen);
        data->Buf = s->data();
    }
    return 0;
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
    add(extras());

    std::sort(all.begin(), all.end(), [](const auto& a, const auto& b) {
        return a.filename() < b.filename();
    });
    files = std::move(all);
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

    // ── left: the file list ────────────────────────────────────────────────
    ImGui::BeginChild("list", ImVec2(260, 0), ImGuiChildFlags_Borders);
    if (files.empty())
        ImGui::TextDisabled("nothing watched yet");
    for (const auto& p : files) {
        bool sel = (p == current);
        if (ImGui::Selectable(shortName(p).c_str(), sel))
            requestOpen(p);
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

    if (current.empty()) {
        ImGui::TextDisabled("select a file");
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

        // The widget renders no text of its own (transparent ink): a coloured glyph
        // drawn over a pale one just muddies through the anti-aliased edges. We draw
        // every character ourselves, in the Dracula palette, over its dark ground.
        const ImU32 ink = ImU32(ImColor(editorStyle().text.getImColor()));
        ImGui::PushStyleColor(ImGuiCol_FrameBg,        IM_COL32(0x28, 0x2A, 0x36, 255));
        ImGui::PushStyleColor(ImGuiCol_Text,           IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, IM_COL32(0x44, 0x47, 0x5A, 255));

        if (ImGui::InputTextMultiline("##body", buffer.data(), buffer.size() + 1,
                                      avail,
                                      ImGuiInputTextFlags_AllowTabInput
                                          | ImGuiInputTextFlags_CallbackResize,
                                      growString, &buffer)) {
            dirty = true;
        }
        ImGui::PopStyleColor(3);

        {
            const ImVec2 p_min = ImGui::GetItemRectMin();
            const ImVec2 p_max = ImGui::GetItemRectMax();

            // the scrollable child InputTextMultiline created, so vertical scroll is
            // read straight from it whether or not the field is focused
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
                const int cpos = std::clamp(st->GetCursorPos(), 0, int(buffer.size()));
                int cl = 0;
                while (cl + 1 < n && int(lines[cl + 1].first) <= cpos) ++cl;
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
    drawPendingPopup();
    ImGui::End();

    // closing keeps the buffer, edits included; quitting asks about them
    if (!open)
        wm.CloseAll();
}

} // namespace slope
