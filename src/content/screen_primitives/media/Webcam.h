#ifndef WEBCAM_H
#define WEBCAM_H

#include "content/screen_primitives/media/Video.h"

namespace slope {

class Webcam;
using WebcamPtr = std::shared_ptr<Webcam>;

/// A live camera. It uses the pipeline of Video without the timeline.
/// The frame shown is the last one received, and there is no seeking, speed or pause.
///
/// It opens when the slide is reached and closes when the slide is left, so the light of the camera follows the slide.
/// It is named Webcam because Camera in slope is a viewpoint.
class Webcam : public Video {
public:
    /// Opens a camera device. The size, rate and format are given and not probed.
    /// `v4l2-ctl --list-formats-ext` lists what the device offers.
    /// Use mjpeg above VGA, because raw 720p at 30 fps does not fit through USB 2.
    static WebcamPtr Add(const std::string& device = "/dev/video0",
                         int w = 1280, int h = 720, int fps = 30,
                         const std::string& input_format = "mjpeg");

    // Same as Add, from a device name and its properties.
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
