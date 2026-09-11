#pragma once

#include "libslope.h"
#include "slides/ui/WindowManager.h"
#include "content/screen_primitives/text/Code.h"

#include <string>
#include <vector>
#include <set>
#include <filesystem>

struct ImFont;
struct ImGuiInputTextCallbackData;

namespace slope {

/*
 * An in-app text editor for every file the hot-reload watchers track, GLSL
 * sources and their includes, Lua snippets, Code and Algorithm sources, the
 * latex files and the deck. Toggled with E.
 *
 * It only writes to disk, and the existing mtime polls pick the change up on
 * the next frame, as they would for an external editor. One file at a time,
 * and unsaved edits are never dropped without asking.
 */
class FileEditor {
public:
    // draws the window when wm has it open; call once per frame from the UI pass
    void draw(WindowManager& wm);

    // files outside the primitive registries (the deck manifest, mainly) that
    // should still show up in the list. Absolute paths, de-duplicated.
    static void registerExtra(const std::filesystem::path& p);

    bool hasUnsaved() const { return dirty; }
    // saves the open file if it has edits; false if that save failed
    bool saveUnsaved() { return !dirty || saveToDisk(); }
    const std::filesystem::path& currentFile() const { return current; }
    // shows p, asking first when the open file has edits
    void open(const std::filesystem::path& p) { requestOpen(p); }

private:
    enum class Pending { None, Switch, Reload, Overwrite };

    void refreshFileList();
    void requestOpen(const std::filesystem::path& p);
    void openFile(const std::filesystem::path& p);
    void loadFromDisk();
    bool saveToDisk();
    bool changedOnDisk() const;
    void drawPendingPopup();
    void rehighlight();   // recompute runs if the buffer moved
    static int inputCallback(ImGuiInputTextCallbackData* data);
    std::string indentAfter(const char* buf, int cursor) const;   // for the line Enter just ended
    bool isYaml() const;
    std::string commentMarker() const;   // "" when the language has no line comment
    void toggleComment(ImGuiInputTextCallbackData* data) const;
    bool createFile();
    static const CodeStyle& editorStyle();

    std::vector<std::filesystem::path> files;      // what the list shows
    std::filesystem::path              current;    // selected file, empty if none
    std::string                        buffer;     // editable contents
    std::filesystem::file_time_type    disk_mtime{};
    bool                               dirty = false;     // buffer != disk
    bool                               load_failed = false;
    std::string                        save_error;        // last failed save, shown in the toolbar
    bool                               widget_reload = false; // buffer replaced under an active field
    bool                               indent_pending = false; // Enter was typed, indent at the next callback
    std::string                        pending_insert;        // what a filtered key stands for, spaces for a yaml tab
    bool                               comment_pending = false; // Ctrl+/ was pressed, applied at the next callback
    bool                               enter_raw = false;      // Enter pressed this frame in the active field
    bool                               enter_handled = false;  // and ImGui turned it into a newline itself
    int                                logged_mods = -1;       // modifiers of the last Enter inserted by hand
    std::set<std::filesystem::path>    missing;              // listed files not on disk, refreshed with the list
    double                             last_refresh = -1; // seconds, throttle
    float                              text_scale = 1.4f; // editor font multiplier

    // a confirmation waiting on the user, and the file a Switch goes to
    Pending                            pending = Pending::None;
    std::filesystem::path              pending_file;

    // tree-sitter highlight of the current buffer, recomputed when it changes
    std::string                        language;          // CodeLanguage name, "" if none
    std::vector<Code::HighlightRun>    runs;
    std::size_t                        hl_hash = 0;
    ImFont*                            mono = nullptr;    // one face, sized per frame
    bool                               font_tried = false;
};

} // namespace slope
