#ifndef LIVETRANSFORM_H
#define LIVETRANSFORM_H

#include "content/polyscope_primitives/Transform.h"
#include "content/authoring/Snippet.h"

namespace slope {

// The fields of a Transform, each either fixed or read from a snippet every frame.
struct LiveTransform {
    LiveVec pos = vec(0, 0, 0);
    // A name that gives one number scales all axes the same.
    LiveVec scale = vec(1, 1, 1);
    LiveVec axis = vec(0, 0, 1);
    // In degrees, like the rot of a screen item.
    LiveScalar angle = 0;

    // Current transform.
    Transform value() const;
    // Current transform placed inside the transform of a parent.
    Transform within(const Transform& parent) const;
};

} // namespace slope

#endif // LIVETRANSFORM_H
