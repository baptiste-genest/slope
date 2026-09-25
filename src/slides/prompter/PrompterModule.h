#ifndef PROMPTERMODULE_H
#define PROMPTERMODULE_H

#include "slides/prompter/Prompter.h"
#include "slides/core/SlideManager.h"

namespace slope {

// A SlideManager that attaches speaker notes to ranges of slides.
class PrompterModule : public SlideManager
{
protected:
    // Range of slides, from begin to end included, where a tag is shown. An end of -1 means no end.
    struct prompt_range {
        int begin,end;
        promptTag tag;
        prompt_range(int b, int e,promptTag t) : begin(b),end(e),tag(t) {}
        // True when slide c is in the range.
        bool inRange(int c) const {
            if (end == -1)
                return begin <= c;
            return (begin <= c) && (c <= end);
        }
    };
    std::vector<prompt_range> scripts_ranges;
    std::unique_ptr<Prompter> prompter_ptr;
public:
    // Sets the script file, named from the project folder, and reads it.
    void setScriptFile(std::string file);

    // Starts a range with a tag on the last slide, and ends the previous range.
    void setPromptTag(promptTag tag);
    // Ends the current range on the last slide.
    void closePromptTag();
};

// Starts a range with a tag, as in show << "intro".
inline PrompterModule& operator<<(PrompterModule& PM,promptTag tag) {
    PM.setPromptTag(tag);
    return PM;
}

}

#endif // PROMPTERMODULE_H
