#pragma once

#include "libslope.h"
#include "slides/ui/WindowManager.h"
#include "content/screen_primitives/text/Code.h"

#include <string>
#include <vector>
#include <set>
#include <filesystem>
#include <functional>

struct ImFont;
struct ImGuiInputTextCallbackData;

namespace slope {

/*
 * A text editor inside the app for every file that the reload watchers follow.
 * These are GLSL sources and their includes, Lua snippets, Code and Algorithm sources, the LaTeX files and the deck.
 * It is toggled with E.
 *
 * It only writes to disk, and the existing polling of modification times detects the change at the next frame,
 * as it would for an external editor.
 * One file is open at a time, and unsaved edits are never dropped without asking.
 */
class FileEditor {
public:
    // Draws the window when wm has it open. Call it once per frame.
    void draw(WindowManager& wm);

    // Adds a file to the list that is not found through the primitives, such as the deck file.
    // The path must be absolute, and duplicates are ignored.
    static void registerExtra(const std::filesystem::path& p);

    // True when the open file has edits that are not saved.
    bool hasUnsaved() const { return dirty; }
    // Saves the open file if it has edits. Returns false if the save failed.
    bool saveUnsaved() { return !dirty || saveToDisk(); }
    // The open file.
    const std::filesystem::path& currentFile() const { return current; }
    // Opens p, after asking when the open file has edits.
    void open(const std::filesystem::path& p) { requestOpen(p); }

    // Scrolls to the frame that the show is on.
    void jumpToCurrentFrame();

    // Called when a frame of the outline is clicked, with its index in the file counted from 0.
    std::function<void(const std::filesystem::path& file, int frame)> onFrameJump;

    // Gives the frame of that file that the show is on, or -1 when the show uses another deck.
    std::function<int(const std::filesystem::path& file)> currentFrameOf;

private:
    enum class Pending { None, Switch, Reload, Overwrite };

    // Builds the list of files.
    void refreshFileList();
    // Opens a file, or asks before if the open file has edits.
    void requestOpen(const std::filesystem::path& p);
    // Opens a file with no question.
    void openFile(const std::filesystem::path& p);
    // Reads the open file again from disk.
    void loadFromDisk();
    // Writes the buffer to disk. Returns false on failure.
    bool saveToDisk();
    // True when the file was modified on disk after it was read.
    bool changedOnDisk() const;
    // Draws the question that waits for an answer.
    void drawPendingPopup();
    // Computes the highlight again if the buffer changed.
    void rehighlight();
    // Called by ImGui for each key in the text field.
    static int inputCallback(ImGuiInputTextCallbackData* data);
    // Indentation for the line that follows the line that Enter just ended.
    std::string indentAfter(const char* buf, int cursor) const;
    // True when the open file is a yaml file.
    bool isYaml() const;
    // Marker of a line comment, or an empty string when the language has none.
    std::string commentMarker() const;
    // Adds or removes the comment marker on the selected lines.
    void toggleComment(ImGuiInputTextCallbackData* data) const;
    // Tab indents and Shift+Tab removes an indent. A selection applies to every line it touches.
    void indentSelection(ImGuiInputTextCallbackData* data, bool dedent) const;
    // Creates the missing file that is selected.
    bool createFile();
    // Style used to draw the text.
    static const CodeStyle& editorStyle();

    // Files shown in the list.
    std::vector<std::filesystem::path> files;
    // Selected file, empty if none.
    std::filesystem::path              current;
    // Text being edited.
    std::string                        buffer;
    std::filesystem::file_time_type    disk_mtime{};
    // True when the buffer differs from the file.
    bool                               dirty = false;
    bool                               load_failed = false;
    // Message of the last failed save, shown in the toolbar.
    std::string                        save_error;
    // True when the buffer was replaced while a text field was active.
    bool                               widget_reload = false;
    // Set when Enter was typed. The indentation is added at the next callback.
    bool                               indent_pending = false;
    // Text that a filtered key stands for, such as spaces for a Tab in yaml.
    std::string                        pending_insert;
    // Set when Ctrl+/ was pressed. It is applied at the next callback.
    bool                               comment_pending = false;
    // Set when Tab or Shift+Tab was pressed. It is applied at the next callback.
    bool                               tab_pending = false;
    bool                               shift_tab_pending = false;
    // Cursor position set by a click in the outline, applied at the next callback.
    int                                jump_to = -1;
    // Line that the jump puts at the top.
    int                                scroll_line = -1;
    // Set when Enter was pressed in this frame in the active field.
    bool                               enter_raw = false;
    // Set when ImGui already turned that Enter into a new line.
    bool                               enter_handled = false;
    // Modifiers of the last Enter inserted by hand.
    int                                logged_mods = -1;
    // Listed files that are not on disk. It is refreshed with the list.
    std::set<std::filesystem::path>    missing;
    // Time in seconds of the last refresh, used to limit how often it happens.
    double                             last_refresh = -1;
    // Multiplier of the font size.
    float                              text_scale = 1.4f;
    // True when the documentation panel is shown, for files that have one.
    bool                               show_tips = true;

    // A confirmation that waits for an answer, and the file that a Switch opens.
    Pending                            pending = Pending::None;
    std::filesystem::path              pending_file;

    // Tree-sitter highlight of the current buffer, computed again when it changes.
    // Name of the CodeLanguage, or an empty string if none.
    std::string                        language;
    std::vector<Code::HighlightRun>    runs;
    std::size_t                        hl_hash = 0;
    // Monospace font, one face whose size is set at every frame.
    ImFont*                            mono = nullptr;
    bool                               font_tried = false;
};

} // namespace slope
