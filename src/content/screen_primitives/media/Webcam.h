#ifndef WEBCAM_H
#define WEBCAM_H

#include "content/screen_primitives/media/Video.h"

namespace slope {

class Webcam;
using WebcamPtr = std::shared_ptr<Webcam>;

/// A live camera, the pipeline Video already has minus the timeline. The frame
/// shown is the last that arrived, and there is no seeking, speed or pausing.
///
/// It opens when the slide is reached and closes when it is left, so the camera
/// light follows the slide. Named Webcam because slope's Camera is a viewpoint.
class Webcam : public Video {
public:
    /// The device is told, not probed. `v4l2-ctl --list-formats-ext` lists what
    /// it offers, and mjpeg above VGA, raw 720p30 does not fit through USB 2.
    static WebcamPtr Add(const std::string& device = "/dev/video0",
                         int w = 1280, int h = 720, int fps = 30,
                         const std::string& input_format = "mjpeg");

    Webcam(const std::string& device, const VideoInfo& info,
           const std::string& input_format);

protected:
    std::vector<std::string> inputArgs(int64_t start_frame) const override;
    int64_t wantedFrame(const TimeObject& t) override;
    bool    hasTimeline() const override { return false; }
    size_t  queueLimit(size_t frame_bytes) const override { return 2; }

private:
    std::string input_format_;
};

}

#endif // WEBCAM_H
