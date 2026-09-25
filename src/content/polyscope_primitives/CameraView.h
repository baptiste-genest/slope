#ifndef CAMERAVIEW_H
#define CAMERAVIEW_H

#include "content/polyscope_primitives/PolyscopePrimitive.h"
#include "content/config/Options.h"
#include <fstream>
#include "extern/json.hpp"

namespace slope {


// Path of a view file. A name that is not absolute is looked up in the views folder, with .json added.
std::string formatCameraFilename(std::string file);
// Removes the window size from the json of a saved camera, so it fits any window.
std::string removeResolutionFromCamfile(std::string json);

class CameraView;
using CameraViewPtr = std::shared_ptr<CameraView>;
// A camera position that a slide can switch to.
class CameraView
{
public:
    // Converts an Eigen vector to a glm vector.
    static glm::vec3 toVec3(const vec& x) {
        return glm::vec3(x(0),x(1),x(2));
    }
    // View looking from `from` to `to`, with `up` as the vertical. With fly, the camera moves smoothly to it.
    CameraView(const glm::vec3 &from, const glm::vec3 &to, const glm::vec3 &up,
               bool fly = false) : from(from),to(to),up(up),flyTo(fly) {
        fromFile = false;
    }

    // View from the json saved by polyscope.
    CameraView(std::string json,bool fly) : jsonContent(json),flyTo(fly) {
        fromFile = true;
    }

    CameraView() {}
    // Builds a view from a position.
    static CameraViewPtr Add(const vec& from,const vec& to,const vec& up = vec(0,1,0),
                             bool flyTo = false);
    // Builds a view from a saved camera file, named as in formatCameraFilename.
    static CameraViewPtr Add(std::string json_file,bool flyTo = false);

public:
    // Moves the camera to the view. With allow_fly false, it jumps there at once, as needed
    // when going through the slide menu or skipping a frame.
    void enable(bool allow_fly = true) {
        // A flight lasts several frames and an export would capture it half way, so it jumps instead.
        bool fly = flyTo && allow_fly && !Options::ExportMode;
        if (fromFile) {
            // Polyscope 2.6.1 never restores "fov" because its key check is inverted.
            // So the saved fov is set first, and both the jump and the flight use it.
            if (saved_fov > 0)
                polyscope::view::fov = saved_fov;
            polyscope::view::setCameraFromJson(jsonContent,fly);
        }
        else
            polyscope::view::lookAt(from,to,up,fly);
    }
    // Returns to the home view of polyscope.
    void disable() {
        polyscope::view::resetCameraToHomeView();
    }

private:
    bool fromFile = false;
    glm::vec3 from,to,up;
    std::string jsonContent;
    bool flyTo;
    float saved_fov = -1;

public:
    // Sets the field of view applied when the view is enabled.
    void setSavedFov(float f) {saved_fov = f;}
};



}

#endif // CAMERAVIEW_H
