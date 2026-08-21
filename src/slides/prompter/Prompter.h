#pragma once

#include "libslope.h"
#include <map>
#include <string>

struct GLFWwindow;
struct ImGuiContext;

namespace slope {

using promptTag = std::string;

class Prompter
{
public:
    Prompter(std::string script_file);
    ~Prompter();

    void write(promptTag tag, TimeStamp fromBegin);
    void erase(TimeStamp fromBegin);
    void loadScript();

private:
    void initWindow();
    void render(const std::string& text, TimeStamp fromBegin);

    GLFWwindow*   window = nullptr;
    ImGuiContext* ctx    = nullptr;

    std::string script_file;
    std::string current_tag;
    std::map<std::string, std::string> scripts;
};

} // namespace slope
