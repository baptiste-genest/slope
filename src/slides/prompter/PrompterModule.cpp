#include "slides/prompter/PrompterModule.h"
#include "slides/ui/CLI.h"

void slope::PrompterModule::setScriptFile(std::string file) {
    prompter_ptr = std::make_unique<Prompter>(Options::ProjectPath + file);
    prompter_ptr->loadScript();
}

void slope::PrompterModule::setPromptTag(promptTag tag) {
    if (prompter_ptr == nullptr)
        throw std::runtime_error("[prompter] set a prompt file before a prompt tag");
    if (getNumberSlides() == 0)
        throw std::runtime_error("[prompter] a prompt tag needs a slide to start on");
    if (!scripts_ranges.empty())
        if (scripts_ranges.back().end == -1)
            scripts_ranges.back().end = getNumberSlides()-2;
    scripts_ranges.emplace_back(getNumberSlides()-1,-1,tag);
}

void slope::PrompterModule::closePromptTag() {
    if (prompter_ptr == nullptr)
        throw std::runtime_error("[prompter] set a prompt file before closing a prompt tag");
    // closing before any tag was opened has no range to terminate
    if (scripts_ranges.empty())
        return;
    scripts_ranges.back().end = getNumberSlides()-1;
}
