#ifndef VIDEO_H
#define VIDEO_H

#include "Image.h"

#include <chrono>
#include <functional>
#include <vector>
#include <memory>
#include <string>

namespace slope {

/// Geometry of a video stream, as reported by ffprobe.
struct VideoInfo {
    int     width     = 0;
    int     height    = 0;
    double  fps       = 0;
    double  duration  = 0;   // seconds, 0 when no container field gives it
    int64_t nb_frames = 0;   // 0 when unknown
    // rational as ffprobe words it ("30000/1001"), handed to ffmpeg verbatim
    std::string fps_str;
    bool valid() const { return width > 0 && height > 0 && fps > 0; }
};

VideoInfo probeVideo(const std::string& file);

class Video;
using VideoPtr = std::shared_ptr<Video>;

/// A video streamed from disk, unlike Gif which holds every frame as a texture.
/// One ffmpeg pipes raw RGBA, a decode thread fills a short queue, one texture
/// is refilled in place, so memory is O(queue) and not O(clip length).
///
/// The decoder starts on the first draw and dies with the slide. No audio, and
/// variable frame rate sources are resampled to a constant rate by ffmpeg.
class Video : public ScreenPrimitive {
public:
    /// decode_width <= 0 decodes at the window width. It is the main lever on
    /// decode cost and bus traffic, pass a smaller one for a clip shown small.
    ///
    /// autoplay false holds the first frame, a click plays or pauses either way.
    /// An unprobeable file gives an inert primitive rather than throwing.
    static VideoPtr Add(const std::string& file, int decode_width = 0,
                        bool loop = true, bool autoplay = true);

    Video(const std::string& path, const VideoInfo& info,
          int w, int h, bool loop, bool autoplay);
    ~Video();

    // owns a GL texture and a live decoder, neither survives a memberwise copy
    Video(const Video&)            = delete;
    Video& operator=(const Video&) = delete;

    bool isValid() const { return info_.valid() && tex_.texture != 0; }

    /// true once a non-looping clip has shown its last frame
    bool hasFinished() const { return finished_; }

    const VideoInfo& info() const { return info_; }

    /// Playback speed, read every frame so a Params handle can drive it live.
    /// It scales elapsed time, not the clock, so a change carries on from the
    /// frame on screen.
    std::function<scalar()> speed = [] { return scalar(1); };

    // ── playback ───────────────────────────────────────────────────────────
    void play()       { playing_ = true; }
    void pause()      { playing_ = false; }
    void togglePlay() { playing_ = !playing_; }
    bool isPlaying() const { return playing_; }

    /// default size, multiplied by whatever scale the slide state carries
    scalar scale = 1;

    /// play glyph over a paused clip, so a slide that waits for a click says so
    bool show_play_overlay = true;

    /// overlay and log the decode counters, first thing to look at on a stutter
    bool show_stats = false;

    /// the frame currently in the texture, RGBA, first row at the top
    std::vector<unsigned char> framePixels() const;
    /// writes that frame to a png
    bool saveFrame(const std::string& file) const;

    /// pixel size of the decoded frame, which is not the size it is drawn at
    int decodedWidth()  const { return w_; }
    int decodedHeight() const { return h_; }

    /// Bytes one video's queue may hold. The length follows from it, clamped
    /// to [2, 8] frames, so 96 MB is 8 frames at 720p and 2 at 4K.
    static size_t MemoryBudget;

    // ── Primitive interface ────────────────────────────────────────────────
    void draw(const TimeObject& t, const StateInSlide& sis) override;
    void playIntro(const TimeObject& t, const StateInSlide& sis) override;
    void playOutro(const TimeObject& t, const StateInSlide& sis) override;
    Size getSize() const override;

protected:
    // the decoder lives exactly as long as the primitive is on screen
    void forceDisable() override;

    // ── what a source has to answer ────────────────────────────────────────
    // A live source (a camera) differs from a file on three points only, so
    // these are the whole of it, everything else below is shared.

    // the ffmpeg argv up to and including -i <path>
    virtual std::vector<std::string> inputArgs(int64_t start_frame) const;
    // the frame the clock is asking for, a live source always wants the newest
    virtual int64_t wantedFrame(const TimeObject& t);
    // a live source has no timeline, so no seeking, no speed and no pausing
    virtual bool    hasTimeline() const { return true; }
    // a queued frame is buffer for a file and latency for a camera
    virtual size_t  queueLimit(size_t frame_bytes) const;

    std::string path_;
    VideoInfo   info_;
    int         w_ = 0, h_ = 0;

private:
    struct Frame;
    struct Stream;   // one ffmpeg process plus its queue, defined in the .cpp

    void step(const TimeObject& t, const StateInSlide& sis);
    void sync(const TimeObject& t);
    void uploadWhenAvailable(int64_t want, bool blocking);
    void display(const StateInSlide& sis);
    bool rect(const StateInSlide& sis, ImVec2& pmin, ImVec2& pmax) const;
    void handleClick(const StateInSlide& sis);
    void drawPlayOverlay(const StateInSlide& sis) const;
    void drawStats() const;
    void logStats(const TimeObject& t);

    void seek(int64_t start_frame);
    std::shared_ptr<Stream> startStream(int64_t start_frame);
    void promoteWarmStream(int64_t want);
    static void killStream(const std::shared_ptr<Stream>& s);
    void closeStream();
    int64_t wrap(int64_t frame) const;

    // at 4x, decoding one frame in 4 keeps the pipe at 1x instead of 4x
    static int64_t strideFor(double speed);
    std::string dividedRate(int64_t stride) const;

    // a seek costs an ffmpeg startup, one a second is all that can ever help
    bool cooledDown() const;

    static void decodeLoop(std::shared_ptr<Stream> s, int64_t base);

    bool        loop_ = true;
    int64_t     total_frames_ = 0;

    ImageData   tex_;
    std::shared_ptr<Stream> stream_;
    // a seek starts here and takes over once it has a frame, so the ~250 ms
    // ffmpeg needs to get going is not a freeze
    std::shared_ptr<Stream> warm_;
    int64_t                 warm_base_ = 0;

    bool        autoplay_ = true;
    bool        playing_  = true;
    double      media_time_ = 0;   // integrated, speed * inner_time jumps back
    double      last_inner_ = -1;
    bool        press_inside_ = false;

    int64_t shown_      = -1;   // frame index currently in the texture
    int64_t next_index_ = 0;    // next index the decoder will emit
    int64_t stream_base_ = 0;   // first index the current stream can ever emit
    int     queue_depth_  = 0;
    bool    seek_pending_ = false;
    bool    restart_now_  = false;   // a working stream ended, skip the cooldown
    bool    warned_no_duration_ = false;
    bool    finished_     = false;
    double  last_log_     = 0;
    int64_t steps_        = 0;
    std::chrono::steady_clock::time_point last_seek_{};
    // dropped_ was skipped by the render thread, discarded_ by the decoder
    int64_t uploaded_ = 0, dropped_ = 0, discarded_ = 0, starves_ = 0, seeks_ = 0;
};

}

#endif // VIDEO_H
