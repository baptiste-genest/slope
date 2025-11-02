#ifndef PROMPTERMODULE_H
#define PROMPTERMODULE_H

#include "Prompter.h"
#include "SlideManager.h"

namespace slope {

class PrompterModule : public SlideManager
{
protected:
    struct prompt_range {
        int begin,end;
        promptTag tag;
        prompt_range(int b, int e,promptTag t) : begin(b),end(e),tag(t) {}
        bool inRange(int c) const {
            if (end == -1)
                return begin <= c;
            return (begin <= c) && (c <= end);
        }
    };
    std::vector<prompt_range> scripts_ranges;
    std::unique_ptr<Prompter> prompter_ptr;
public:
    void setScriptFile(std::string file);

    void setPromptTag(promptTag tag);
    void closePromptTag();
};

inline PrompterModule& operator<<(PrompterModule& PM,promptTag tag) {
    PM.setPromptTag(tag);
    return PM;
}

struct ClosePromptTag {};
inline PrompterModule& operator<<(PrompterModule& PM,ClosePromptTag) {
    PM.closePromptTag();
    return PM;
}

}

#endif // PROMPTERMODULE_H
