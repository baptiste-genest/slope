#ifndef VIDEO_H
#define VIDEO_H

#include "content/screen_primitives/media/Image.h"

#include <chrono>
#include <functional>
#include <vector>
#include <memory>
#include <string>

namespace slope {

/// Size, rate and length of a video stream, as reported by ffprobe.
struct VideoInfo {
    int width = 0;
    int height = 0;
    double fps = 0;
    // Duration in seconds, 0 when the container does not give it.
    double duration = 0;
    // Number of frames, 0 when unknown.
    int64_t nb_frames = 0;
    // Frame rate as a fraction written like ffprobe does, for example "30000/1001". It is given to ffmpeg unchanged.
    std::string fps_str;
    // True when the stream has a size and a rate.
    bool valid() const { return width > 0 && height > 0 && fps > 0; }
};

// Reads the properties of a video file with ffprobe.
VideoInfo probeVideo(const std::string& file);

class Video;
using VideoPtr = std::shared_ptr<Video>;

/// A video read from disk while it plays. A Gif, by contrast, keeps every frame as a texture.
/// One ffmpeg process sends raw RGBA frames, a decoding thread fills a short queue and one texture is refilled in place.
/// The memory used depends on the size of the queue and not on the length of the clip.
///
/// The decoder starts at the first draw and stops when the slide ends.
/// There is no audio. A video with a variable frame rate is converted to a constant rate by ffmpeg.
class Video : public ScreenPrimitive {
public:
    /// Builds a video from a file.
    /// A decode_width of 0 or less decodes at the width of the window.
    /// A smaller width lowers the cost of decoding and of the transfer to the GPU, so use it for a clip shown small.
    ///
    /// With autoplay false, the first frame is shown. A click plays or pauses in both cases.
    /// A file that ffprobe cannot read gives a video that does nothing, and no exception.
    static VideoPtr Add(const std::string& file, int decode_width = 0,
                        bool loop = true, bool autoplay = true);

    Video(const std::string& path, const VideoInfo& info,
          int w, int h, bool loop, bool autoplay);
    ~Video();

    // Copying is not allowed because a video owns a texture and a running decoder.
    Video(const Video&) = delete;
    Video& operator=(const Video&) = delete;

    // True when the file was probed and the texture exists.
    bool isValid() const { return info_.valid() && tex_.texture != 0; }

    /// True once a clip that does not loop has shown its last frame.
    bool hasFinished() const { return finished_; }

    // Properties of the stream.
    const VideoInfo& info() const { return info_; }

    /// Playback speed, read every frame so a Params handle can change it while playing.
    /// It scales the elapsed time and not the clock, so a change continues from the frame on screen.
    std::function<scalar()> speed = [] { return scalar(1); };

    // Playback controls.
    void play() { playing_ = true; }
    void pause() { playing_ = false; }
    void togglePlay() { playing_ = !playing_; }
    bool isPlaying() const { return playing_; }

    /// Default size, multiplied by the scale of the slide state.
    scalar scale = 1;

    /// Draws a play symbol over a paused clip, so a slide that waits for a click shows it.
    bool show_play_overlay = true;

    /// Draws the decoding counters and writes them to the log. Look at them first when the playback stutters.
    bool show_stats = false;

    /// Pixels of the frame in the texture, as RGBA with the first row at the top.
    std::vector<unsigned char> framePixels() const;
    /// Writes that frame to a png file. Returns false on failure.
    bool saveFrame(const std::string& file) const;

    /// Size in pixels of the decoded frame. It is not the size at which the frame is drawn.
    int decodedWidth() const { return w_; }
    int decodedHeight() const { return h_; }

    /// Number of bytes that the queue of one video can hold.
    /// The queue length follows from it, between 2 and 8 frames, so 96 MB gives 8 frames at 720p and 2 frames at 4K.
    static size_t MemoryBudget;

    // Primitive interface
    void draw(const TimeObject& t, const StateInSlide& sis) override;
    void playIntro(const TimeObject& t, const StateInSlide& sis) override;
    void playOutro(const TimeObject& t, const StateInSlide& sis) override;
    // Size in pixels.
    Size getSize() const override;
    bool canRotate() const override { return true; }

protected:
    // Stops the decoder, which runs only while the primitive is on screen.
    void forceDisable() override;

    // A live source such as a camera differs from a file in the three points below.
    // Everything else is shared.

    // Arguments of ffmpeg up to and including -i <path>.
    virtual std::vector<std::string> inputArgs(int64_t start_frame) const;
    // Index of the frame that the clock asks for. A live source always wants the newest one.
    virtual int64_t wantedFrame(const TimeObject& t);
    // False for a live source, which has no timeline, so no seeking, no speed and no pause.
    virtual bool hasTimeline() const { return true; }
    // Maximum number of queued frames. A queued frame is a buffer for a file and a delay for a camera.
    virtual size_t queueLimit(size_t frame_bytes) const;

    std::string path_;
    VideoInfo info_;
    int w_ = 0, h_ = 0;

private:
    struct Frame;
    // One ffmpeg process with its queue. It is defined in the .cpp.
    struct Stream;

    // Handles the click, chooses the frame and draws it.
    void step(const TimeObject& t, const StateInSlide& sis);
    // Starts, restarts or seeks the stream so it provides the frame asked by the clock.
    void sync(const TimeObject& t);
    // Takes frames from the queue up to `want` and uploads the newest. When blocking, it waits for a frame.
    void uploadWhenAvailable(int64_t want, bool blocking);
    // Draws the texture.
    void display(const StateInSlide& sis);
    // Gives the upright box of the video on screen. Returns false when the video is not valid.
    bool rect(const StateInSlide& sis, ImVec2& pmin, ImVec2& pmax) const;
    // Plays or pauses when the video is clicked.
    void handleClick(const StateInSlide& sis);
    // Draws the play symbol.
    void drawPlayOverlay(const StateInSlide& sis) const;
    // Draws the decoding counters.
    void drawStats() const;
    // Writes the decoding counters to the log every two seconds.
    void logStats(const TimeObject& t);

    // Jumps to a frame, keeping the old stream on screen until the new one has a frame.
    void seek(int64_t start_frame);
    // Starts an ffmpeg process at a frame.
    std::shared_ptr<Stream> startStream(int64_t start_frame);
    // Replaces the current stream by the one started by a seek when it has a frame ready.
    void promoteWarmStream(int64_t want);
    // Stops a stream and its process.
    static void killStream(const std::shared_ptr<Stream>& s);
    // Stops both streams.
    void closeStream();
    // Frame of the file for a frame index, when the clip loops.
    int64_t wrap(int64_t frame) const;

    // Number of frames skipped for a speed. At speed 4, decoding one frame out of 4 keeps the rate of the pipe at 1x instead of 4x.
    static int64_t strideFor(double speed);
    // Frame rate of the file divided by the stride, as a fraction.
    std::string dividedRate(int64_t stride) const;

    // True when a second passed since the last seek. A seek costs an ffmpeg startup, so more than one per second cannot help.
    bool cooledDown() const;

    // Function run by the decoding thread.
    static void decodeLoop(std::shared_ptr<Stream> s, int64_t base);

    bool loop_ = true;
    int64_t total_frames_ = 0;

    ImageData tex_;
    std::shared_ptr<Stream> stream_;
    // The stream started by a seek. It takes over once it has a frame,
    // so the 250 ms that ffmpeg needs to start is not seen as a freeze.
    std::shared_ptr<Stream> warm_;
    int64_t warm_base_ = 0;

    bool autoplay_ = true;
    bool playing_ = true;
    // Time in the clip, accumulated over the frames. Computing speed * inner_time would jump when the speed changes.
    double media_time_ = 0;
    double last_inner_ = -1;
    bool press_inside_ = false;

    // Index of the frame in the texture.
    int64_t shown_ = -1;
    // Next index the decoder will produce.
    int64_t next_index_ = 0;
    // First index the current stream can produce.
    int64_t stream_base_ = 0;
    int queue_depth_ = 0;
    bool seek_pending_ = false;
    // Set when a working stream ended, to skip the cooldown.
    bool restart_now_ = false;
    bool warned_no_duration_ = false;
    bool finished_ = false;
    double last_log_ = 0;
    int64_t steps_ = 0;
    std::chrono::steady_clock::time_point last_seek_{};
    // Counters. dropped_ counts frames skipped by the render thread and discarded_ those skipped by the decoder.
    int64_t uploaded_ = 0, dropped_ = 0, discarded_ = 0, starves_ = 0, seeks_ = 0;
};

} // namespace slope

#endif // VIDEO_H
