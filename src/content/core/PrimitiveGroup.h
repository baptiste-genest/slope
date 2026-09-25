#ifndef PRIMITIVEGROUP_H
#define PRIMITIVEGROUP_H
#include "content/core/primitive.h"
#include "slides/core/Slide.h"

namespace slope {

// A set of primitives with their states, reusable in several slides.
struct PrimitiveGroup {
    Slide buffer;
    // Adds a primitive with the default state.
    PrimitiveGroup& operator<<(PrimitivePtr ptr) {
        buffer.add(ptr);
        return *this;
    }

    // Adds a primitive with the given state.
    PrimitiveGroup& operator<<(const PrimitiveInSlide& pis) {
        buffer.add(pis.first,pis.second);
        return *this;
    }
};

}

#endif // PRIMITIVEGROUP_H
