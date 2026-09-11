#pragma once

#include "libslope.h"
#include "slides/ui/WindowManager.h"
#include "content/screen_primitives/text/Code.h"

#include <string>
#include <vector>
#include <filesystem>

struct ImFont;

namespace slope {

/*
 * An in-app text editor for every file the hot-reload watchers are tracking:
 * GLSL/frag sources and their #included headers, Lua snippets, Code and
 * Algorithm sources, and the deck manifest. Toggled with E.
 *
 * It does not talk to the reload machinery at all: it writes to disk and the
 * existing mtime polls (Shader/Snippet/Code::HotReloadIfModified, the deck
 * loader) pick the change up on the next frame, exactly as an external editor
 * would. One file at a time; unsaved edits are never dropped without asking.
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
    static const CodeStyle& editorStyle();

    std::vector<std::filesystem::path> files;      // what the list shows
    std::filesystem::path              current;    // selected file, empty if none
    std::string                        buffer;     // editable contents
    std::filesystem::file_time_type    disk_mtime{};
    bool                               dirty = false;     // buffer != disk
    bool                               load_failed = false;
    std::string                        save_error;        // last failed save, shown in the toolbar
    bool                               widget_reload = false; // buffer replaced under an active field
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
