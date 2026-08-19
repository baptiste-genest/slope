#include "LiveVec.h"
#include "Snippet.h"

namespace slope {

vec LiveVec::value() const
{
    return live() ? Snippet::get(snippet).v3() : fixed;
}

std::string LiveVec::key() const
{
    if (live())
        return snippet;
    return std::to_string(fixed(0)) + "," + std::to_string(fixed(1))
         + "," + std::to_string(fixed(2));
}

}
