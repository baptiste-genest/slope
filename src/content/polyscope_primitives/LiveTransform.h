#ifndef LIVETRANSFORM_H
#define LIVETRANSFORM_H

#include "content/polyscope_primitives/Transform.h"
#include "content/authoring/Snippet.h"

namespace slope {

// the fields of a Transform, each given outright or read from a snippet every frame
struct LiveTransform {
    LiveVec pos = vec(0,0,0);
    // a name giving one number scales uniformly
    LiveVec scale = vec(1,1,1);
    LiveVec axis = vec(0,0,1);
    // degrees, like the rot of a screen item
    LiveScalar angle = 0;

    Transform value() const;
    Transform within(const Transform& parent) const;
};

}

#endif // LIVETRANSFORM_H
