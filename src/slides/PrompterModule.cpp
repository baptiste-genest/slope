#include "PrompterModule.h"

void slope::PrompterModule::setScriptFile(std::string file) {
    prompter_ptr = std::make_unique<Prompter>(file);
}

void slope::PrompterModule::setPromptTag(promptTag tag) {
    if (prompter_ptr == nullptr){
        return;
        std::cerr << " [ Must set prompt file ]" << std::endl;
        assert(0);
    }
    if (getNumberSlides() == 0){
        std::cerr << "[ NO CURRENT SLIDE ]" << std::endl;
        assert(0);
    }
    if (!scripts_ranges.empty())
        if (scripts_ranges.back().end == -1)
            scripts_ranges.back().end = getNumberSlides()-2;
    scripts_ranges.emplace_back(getNumberSlides()-1,-1,tag);
}

void slope::PrompterModule::closePromptTag() {
    if (prompter_ptr == nullptr){
        std::cerr << " [ Must set prompt file ]" << std::endl;
        assert(0);
    }
    scripts_ranges.back().end = getNumberSlides()-1;
}
