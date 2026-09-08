#pragma once

#include "libslope.h"
#include "slides/ui/WindowManager.h"
#include "content/screen_primitives/text/Code.h"

#include <string>
#include <vector>
#include <map>
#include <filesystem>

struct ImFont;

namespace slope {

/*
 * A bare-bones in-app text editor for every file the hot-reload watchers are
 * tracking: GLSL/frag sources and their #included headers, Lua snippets, Code
 * primitive sources, and the deck manifest. Toggled with E.
 *
 * It does not talk to the reload machinery at all: it writes to disk and the
 * existing mtime polls (Shader/Snippet/Code::HotReloadIfModified, the deck
 * loader) pick the change up on the next frame, exactly as an external editor
 * would. A prototype, hence no tabs, no undo, no syntax colouring.
 */
class FileEditor {
public:
    // draws the window when wm has it open; call once per frame from the UI pass
    void draw(WindowManager& wm);

    // files outside the primitive registries (the deck manifest, mainly) that
    // should still show up in the list. Absolute paths, de-duplicated.
    static void registerExtra(const std::filesystem::path& p);

private:
    void refreshFileList();
    void selectFile(const std::filesystem::path& p);
    void loadFromDisk();
    void saveToDisk();
    void rehighlight();   // recompute runs if the buffer moved
    ImFont* fontForScale(float scale);
    void primeFonts();    // bake every snapped face on open, off the hot path
    static const CodeStyle& editorStyle();

    std::vector<std::filesystem::path> files;      // what the list shows
    std::filesystem::path              current;    // selected file, empty if none
    std::string                        buffer;     // editable contents
    std::filesystem::file_time_type    disk_mtime{};
    bool                               dirty = false;     // buffer != disk
    bool                               load_failed = false;
    double                             last_refresh = -1; // seconds, throttle
    float                              text_scale = 1.4f; // editor font multiplier

    // tree-sitter highlight of the current buffer, recomputed when it changes
    std::string                        language;          // CodeLanguage name, "" if none
    std::vector<Code::HighlightRun>    runs;
    std::size_t                        hl_hash = 0;
    std::map<int, ImFont*>             font_cache;   // px size -> atlas font
    bool                               fonts_primed = false;
};

} // namespace slope
