#ifndef PRIMITIVEGROUP_H
#define PRIMITIVEGROUP_H
#include "primitive.h"
#include "../slides/Slide.h"

namespace slope {

struct PrimitiveGroup {
    Slide buffer;
    PrimitiveGroup& operator<<(PrimitivePtr ptr) {
        buffer.add(ptr);
        return *this;
    }

    PrimitiveGroup& operator<<(const PrimitiveInSlide& pis) {
        buffer.add(pis.first,pis.second);
        return *this;
    }
};

}

#endif // PRIMITIVEGROUP_H
