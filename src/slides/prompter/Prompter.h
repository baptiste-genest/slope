#pragma once

#include "libslope.h"
#include <map>
#include <string>

struct GLFWwindow;
struct ImGuiContext;

namespace slope {

// Name of a section of the script.
using promptTag = std::string;

// A second window that shows the speaker notes of the current slide.
class Prompter {
public:
    // Prompter for a script file. The window opens at the first use.
    Prompter(std::string script_file);
    ~Prompter();

    // Shows the text of a tag. An unknown tag clears the window.
    void write(promptTag tag, TimeStamp fromBegin);
    // Clears the window.
    void erase(TimeStamp fromBegin);
    // Reads the script file. Throws if it cannot be opened.
    void loadScript();

private:
    // Creates the window.
    void initWindow();
    // Draws a text in the window.
    void render(const std::string& text, TimeStamp fromBegin);

    GLFWwindow* window = nullptr;
    ImGuiContext* ctx = nullptr;

    std::string script_file;
    std::string current_tag;
    std::map<std::string, std::string> scripts;
};

} // namespace slope
