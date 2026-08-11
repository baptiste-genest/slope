#include "Video.h"

#include "../Options.h"
#include "../io.h"

#include "imgui.h"
#include "polyscope/screenshot.h"
#include "../../slides/DragEditor.h"
#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <deque>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

#include <cerrno>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

namespace slope {

size_t Video::MemoryBudget = 96u << 20;

namespace {

// ─────────────────────────────────────────────────────────────────────────────
// running ffmpeg without a shell
// ─────────────────────────────────────────────────────────────────────────────
// popen() hands back a FILE* and no pid, leaving no way to kill a decoder
// stalled in a read. fork/exec gives both the pid and a literal argv.
struct Child {
    pid_t pid = -1;
    int   fd  = -1;
};

bool spawn(const std::vector<std::string>& args, Child& c)
{
    // built before the fork, allocating between fork and exec is not legal
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);

    int p[2];
    if (pipe(p) != 0) return false;

    pid_t child = fork();
    if (child < 0) { close(p[0]); close(p[1]); return false; }

    if (child == 0) {
        dup2(p[1], STDOUT_FILENO);
        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            dup2(devnull, STDERR_FILENO);   // ffmpeg cries "broken pipe" on every seek
            if (devnull > 2) close(devnull);
        }
        close(p[0]);
        if (p[1] > 2) close(p[1]);
        execvp(argv[0], argv.data());
        _exit(127);
    }

    close(p[1]);
    c.pid = child;
    c.fd  = p[0];
    return true;
}

// a pipe read returns whatever is buffered, which is never a whole 4K frame
bool readFull(int fd, uint8_t* dst, size_t n)
{
    size_t got = 0;
    while (got < n) {
        ssize_t r = ::read(fd, dst + got, n - got);
        if (r > 0) { got += size_t(r); continue; }
        if (r == 0) return false;              // ffmpeg closed its end
        if (errno == EINTR) continue;
        return false;
    }
    return true;
}

std::string runCapture(const std::vector<std::string>& args)
{
    Child c;
    if (!spawn(args, c)) return {};

    std::string out;
    char buf[512];
    while (true) {
        ssize_t r = ::read(c.fd, buf, sizeof(buf));
        if (r > 0) { out.append(buf, size_t(r)); continue; }
        if (r < 0 && errno == EINTR) continue;
        break;
    }
    close(c.fd);
    int st;
    while (::waitpid(c.pid, &st, 0) < 0 && errno == EINTR) {}
    return out;
}

double parseRate(const std::string& s)
{
    auto slash = s.find('/');
    if (slash == std::string::npos) return std::atof(s.c_str());
    double num = std::atof(s.substr(0, slash).c_str());
    double den = std::atof(s.substr(slash + 1).c_str());
    return (den > 0) ? num / den : 0;
}

bool sane(const std::string& v) { return !v.empty() && v != "N/A" && v != "0/0"; }

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// probing
// ─────────────────────────────────────────────────────────────────────────────
// key=value and not csv, ffprobe emits the fields in its own order. Both
// durations are asked for, matroska leaves the stream one at N/A and without
// one total_frames_ is 0, which disables looping and seeking.
VideoInfo probeVideo(const std::string& file)
{
    VideoInfo info;
    std::string raw = runCapture({
        Options::PathToFFPROBE, "-v", "error", "-select_streams", "v:0",
        "-show_entries", "stream=width,height,r_frame_rate,avg_frame_rate,duration,nb_frames",
        "-show_entries", "format=duration",
        "-of", "default=nw=1", file});

    if (raw.empty()) {
        spdlog::error("[video] {} could not read {}, missing file or no video stream?",
                      Options::PathToFFPROBE, file);
        return info;
    }

    double stream_duration = 0, format_duration = 0;
    std::string avg_rate, r_rate;
    std::istringstream in(raw);
    std::string line;
    while (std::getline(in, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq), val = line.substr(eq + 1);
        while (!val.empty() && (val.back() == '\r' || val.back() == ' ')) val.pop_back();
        if (!sane(val)) continue;

        if      (key == "width")          info.width     = std::atoi(val.c_str());
        else if (key == "height")         info.height    = std::atoi(val.c_str());
        else if (key == "nb_frames")      info.nb_frames = std::atoll(val.c_str());
        else if (key == "r_frame_rate")   r_rate   = val;
        else if (key == "avg_frame_rate") avg_rate = val;
        // ffprobe prints the stream section first, so the first duration seen
        // is the stream's and the second the container's
        else if (key == "duration") {
            if (stream_duration == 0) stream_duration = std::atof(val.c_str());
            else                      format_duration = std::atof(val.c_str());
        }
    }

    // avg_frame_rate is the honest average; r_frame_rate is a base rate that a
    // variable frame rate file can report as something absurd like 1000/1
    for (const std::string& cand : {avg_rate, r_rate}) {
        double f = parseRate(cand);
        if (f > 0 && f < 1000) { info.fps = f; info.fps_str = cand; break; }
    }

    info.duration = (stream_duration > 0) ? stream_duration : format_duration;
    if (info.duration == 0 && info.nb_frames > 0 && info.fps > 0)
        info.duration = double(info.nb_frames) / info.fps;
    return info;
}

// ─────────────────────────────────────────────────────────────────────────────
// the queue
// ─────────────────────────────────────────────────────────────────────────────
struct Video::Frame {
    int64_t              index = -1;
    std::vector<uint8_t> rgba;
};

// Held by a shared_ptr the decode thread captures, so a seek abandons a stream
// without ever joining it. The orphaned thread reaps itself and takes it along.
struct Video::Stream {
    std::mutex                        m;
    std::condition_variable           room;    // decoder waits for queue space
    std::deque<Frame>                 ready;
    std::vector<std::vector<uint8_t>> pool;    // recycled frame buffers
    bool   stop = false;
    bool   eof  = false;
    size_t  frame_bytes = 0;
    size_t  max_queued  = 4;
    int64_t stride      = 1;   // native frames between two frames it emits
    pid_t  pid = -1;
    int    fd  = -1;

    // frame the render thread wants, the decoder drops what is older instead
    // of queueing it, a queue of stale frames would block it for good
    int64_t target    = -1;
    int64_t discarded = 0;
    int64_t produced  = 0;   // tells a working stream from a stillborn one
};

// ─────────────────────────────────────────────────────────────────────────────
// construction
// ─────────────────────────────────────────────────────────────────────────────
VideoPtr Video::Add(const std::string& file, int decode_width, bool loop, bool autoplay)
{
    std::string path = formatPath(file);
    VideoInfo   info = probeVideo(path);

    if (!info.valid()) {
        spdlog::error("[video] {} is not playable, the slide will show nothing", path);
        return NewPrimitive<Video>(path, info, 0, 0, loop, autoplay);
    }

    // never decode more pixels than the screen shows, a 4K frame is 33 MB
    // through the pipe and over the bus for pixels thrown away on arrival
    int cap = (decode_width > 0) ? decode_width : int(Options::ScreenResolutionWidth);
    int w = std::min(cap, info.width);
    int h = int(std::lround(double(w) * info.height / info.width));
    w -= w & 1; h -= h & 1;   // keep it even, scale filters prefer it
    w = std::max(w, 2); h = std::max(h, 2);

    spdlog::info("[video] {}, {}x{} @ {:.3f} fps, {:.2f} s -> decoding {}x{}",
                 path, info.width, info.height, info.fps, info.duration, w, h);

    return NewPrimitive<Video>(path, info, w, h, loop, autoplay);
}

Video::Video(const std::string& path, const VideoInfo& info,
             int w, int h, bool loop, bool autoplay)
    : path_(path), info_(info), w_(w), h_(h), loop_(loop),
      autoplay_(autoplay), playing_(autoplay)
{
    if (!info_.valid()) return;

    total_frames_ = (info_.nb_frames > 0)
                        ? info_.nb_frames
                        : int64_t(std::llround(info_.duration * info_.fps));
    if (total_frames_ < 0) total_frames_ = 0;

    // one texture, allocated once, refilled in place for the whole clip
    tex_.width  = w_;
    tex_.height = h_;
    glGenTextures(1, &tex_.texture);
    glBindTexture(GL_TEXTURE_2D, tex_.texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w_, h_, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);

    // no decoder here, a slide the talk never reaches costs nothing
}

Video::~Video()
{
    closeStream();
    // primitives outlive the window, deleting a texture then hits a dead context
    if (tex_.texture && glfwGetCurrentContext())
        glDeleteTextures(1, &tex_.texture);
}

// ─────────────────────────────────────────────────────────────────────────────
// the decode side
// ─────────────────────────────────────────────────────────────────────────────
void Video::killStream(const std::shared_ptr<Stream>& s)
{
    if (!s) return;
    {
        std::lock_guard<std::mutex> lk(s->m);
        s->stop = true;
        // SIGKILL, the decoder is usually blocked reading and a 4K frame can
        // be 100 ms away. Under the reaper's lock, so no recycled pid is hit
        if (s->pid > 0) ::kill(s->pid, SIGKILL);
    }
    s->room.notify_all();   // the decode thread holds the last reference
}

void Video::closeStream()
{
    killStream(stream_);
    killStream(warm_);
    stream_.reset();
    warm_.reset();

    shown_        = -1;
    seek_pending_ = false;
    queue_depth_  = 0;
}

int64_t Video::strideFor(double sp)
{
    return std::clamp<int64_t>(int64_t(std::llround(std::abs(sp))), 1, 16);
}

// the rate is a rational, so dividing it is multiplying its denominator
std::string Video::dividedRate(int64_t stride) const
{
    if (info_.fps_str.empty() || stride <= 1) return info_.fps_str;
    const auto slash = info_.fps_str.find('/');
    const std::string num = (slash == std::string::npos) ? info_.fps_str
                                                         : info_.fps_str.substr(0, slash);
    const long long den = (slash == std::string::npos)
                              ? 1 : std::atoll(info_.fps_str.c_str() + slash + 1);
    if (den <= 0) return info_.fps_str;
    return num + "/" + std::to_string(den * stride);
}

bool Video::cooledDown() const
{
    return (std::chrono::steady_clock::now() - last_seek_) > std::chrono::seconds(1);
}

int64_t Video::wrap(int64_t frame) const
{
    if (total_frames_ <= 0) return frame;
    int64_t f = frame % total_frames_;
    return (f < 0) ? f + total_frames_ : f;
}

// `-ss` before `-i` is the fast form and, since ffmpeg 2.1, an exact one.
// `-stream_loop -1` makes the wrap point free, and its later iterations start
// at 0 rather than at `-ss`, which is what makes file frame = wrap(index) hold.
void Video::seek(int64_t start_frame)
{
    last_seek_   = std::chrono::steady_clock::now();   // set before the attempt, a
    restart_now_ = false;                              // failed spawn must cool down too
    if (!isValid()) return;

    auto s = startStream(start_frame);
    if (!s) return;
    ++seeks_;

    // the old stream keeps feeding while the new one starts, which is the
    // quarter second a seek used to freeze for. An export blocks, so it swaps
    if (stream_ && shown_ >= 0 && !Options::ExportMode) {
        killStream(warm_);
        warm_      = s;
        warm_base_ = start_frame;
        return;
    }

    killStream(stream_);
    killStream(warm_);
    warm_.reset();
    stream_       = s;
    next_index_   = start_frame;
    stream_base_  = start_frame;
    seek_pending_ = true;
    shown_        = -1;
    queue_depth_  = 0;
}

// its first frame is the signal, until then the old stream is what is shown
void Video::promoteWarmStream(int64_t want)
{
    bool ready = false, dead = false;
    {
        std::lock_guard<std::mutex> lk(warm_->m);
        ready = !warm_->ready.empty() && warm_->ready.front().index <= want;
        dead  = warm_->eof && warm_->ready.empty();
        if (ready) warm_->target = want;
    }
    if (dead)   { killStream(warm_); warm_.reset(); return; }
    if (!ready) return;

    killStream(stream_);
    stream_       = warm_;
    warm_.reset();
    stream_base_  = warm_base_;
    next_index_   = warm_base_;
    seek_pending_ = true;
    queue_depth_  = 0;
}

// budget bytes and derive the length, 8 frames of 4K would be 264 MB
size_t Video::queueLimit(size_t frame_bytes) const
{
    return std::clamp<size_t>(MemoryBudget / frame_bytes, 2, 8);
}

std::vector<std::string> Video::inputArgs(int64_t start_frame) const
{
    double t0 = 0;
    if (total_frames_ > 0)
        t0 = double(wrap(start_frame)) / info_.fps;
    else if (info_.duration > 0)
        t0 = std::min(double(start_frame) / info_.fps, info_.duration);
    // with neither, -ss could land past the end, so start from the top

    std::vector<std::string> args{Options::PathToFFMPEG, "-nostdin", "-loglevel", "error"};
    if (loop_) { args.push_back("-stream_loop"); args.push_back("-1"); }
    if (t0 > 0.001) { args.push_back("-ss"); args.push_back(std::to_string(t0)); }
    args.insert(args.end(), {"-i", path_, "-an", "-sn", "-dn"});
    return args;
}

std::shared_ptr<Video::Stream> Video::startStream(int64_t start_frame)
{
    if (hasTimeline() && total_frames_ == 0 && !warned_no_duration_) {
        warned_no_duration_ = true;
        spdlog::warn("[video] {} reports no duration, playback is fine but seeking "
                     "inside it can only restart the clip", path_);
    }

    auto s = std::make_shared<Stream>();
    s->stride      = hasTimeline() ? strideFor(double(speed())) : 1;
    s->frame_bytes = size_t(w_) * size_t(h_) * 4;
    s->max_queued  = queueLimit(s->frame_bytes);

    std::vector<std::string> args = inputArgs(start_frame);
    if (w_ != info_.width || h_ != info_.height)
        args.insert(args.end(), {"-vf", "scale=" + std::to_string(w_) + ":" + std::to_string(h_)});
    // constant output rate, a variable one breaks index -> index/fps
    const std::string rate = dividedRate(s->stride);
    if (!rate.empty()) { args.push_back("-r"); args.push_back(rate); }
    args.insert(args.end(), {"-f", "rawvideo", "-pix_fmt", "rgba", "pipe:1"});

    Child c;
    if (!spawn(args, c)) {
        spdlog::error("[video] could not start {}", Options::PathToFFMPEG);
        return nullptr;
    }
    s->pid    = c.pid;
    s->fd     = c.fd;
    s->target = start_frame;

    int64_t base = start_frame;
    std::thread([s, base] { decodeLoop(s, base); }).detach();
    return s;
}

void Video::decodeLoop(std::shared_ptr<Stream> s, int64_t base)
{
    const int     fd     = s->fd;
    const int64_t stride = s->stride;
    int64_t index = base;

    while (true) {
        std::vector<uint8_t> buf;
        {
            std::unique_lock<std::mutex> lk(s->m);
            // a full queue only holds it back while it is not behind
            s->room.wait(lk, [&] {
                return s->stop || s->ready.size() < s->max_queued || index < s->target;
            });
            if (s->stop) break;
            if (!s->pool.empty()) { buf = std::move(s->pool.back()); s->pool.pop_back(); }
        }
        buf.resize(s->frame_bytes);

        // outside the lock, the render thread keeps draining the queue
        if (!readFull(fd, buf.data(), s->frame_bytes)) {
            std::lock_guard<std::mutex> lk(s->m);
            s->eof = true;
            break;
        }

        std::lock_guard<std::mutex> lk(s->m);
        if (s->stop) break;
        // too late, and something newer is queued. The queue check matters, a
        // decoder running at playback speed is always a little late and
        // discarding on lateness alone would show nothing at all
        if (index < s->target && !s->ready.empty()) {
            index += stride;
            ++s->discarded;
            s->pool.push_back(std::move(buf));
            continue;
        }
        ++s->produced;
        s->ready.push_back(Frame{index, std::move(buf)});
        index += stride;
    }

    pid_t pid;
    {
        std::lock_guard<std::mutex> lk(s->m);
        pid   = s->pid;
        s->pid = -1;        // from here on closeStream() will not signal it
        s->fd  = -1;
    }
    ::close(fd);
    if (pid > 0) {
        ::kill(pid, SIGKILL);   // no-op once it has exited on its own
        int st;
        while (::waitpid(pid, &st, 0) < 0 && errno == EINTR) {}
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// the presentation side
// ─────────────────────────────────────────────────────────────────────────────
void Video::draw(const TimeObject& t, const StateInSlide& sis)      { step(t, sis); }
void Video::playIntro(const TimeObject& t, const StateInSlide& sis) { step(t, sis); }
void Video::playOutro(const TimeObject& t, const StateInSlide& sis) { step(t, sis); }

// an unplayable file has no texture, and its -1 size would poison every box
Primitive::Size Video::getSize() const
{
    return isValid() ? Image::getScaledSize(tex_, 1) : Size::Zero();
}

// leaving the slide kills the decoder, the next appearance starts from zero
void Video::forceDisable()
{
    closeStream();
    finished_    = false;
    playing_     = autoplay_;
    media_time_  = 0;
    last_inner_  = -1;
    restart_now_ = false;
}

void Video::step(const TimeObject& t, const StateInSlide& sis)
{
    if (!isValid()) return;
    handleClick(sis);
    sync(t);
    display(sis);
    if (!playing_ && show_play_overlay && !Options::ExportMode) drawPlayOverlay(sis);
    if (show_stats) { drawStats(); logStats(t); }
}

// integrated, not inner_time * speed, which jumps back on a slowdown
int64_t Video::wantedFrame(const TimeObject& t)
{
    const double inner = double(t.inner_time);
    if (last_inner_ < 0 || inner < last_inner_) last_inner_ = inner;
    if (playing_) media_time_ += (inner - last_inner_) * double(speed());
    last_inner_ = inner;
    if (media_time_ < 0) media_time_ = 0;

    int64_t want = int64_t(std::floor(media_time_ * info_.fps));
    if (want < 0) want = 0;
    if (!loop_ && total_frames_ > 0) want = std::min(want, total_frames_ - 1);
    return want;
}

void Video::sync(const TimeObject& t)
{
    const int64_t want  = wantedFrame(t);
    // a live source is started from its first frame, there is nothing to seek to
    const int64_t start = hasTimeline() ? want : 0;

    // first draw, or a stream that died. The cooldown keeps a broken file from
    // spawning one ffmpeg per rendered frame
    if (!stream_) {
        if (finished_) return;
        if (!restart_now_ && !cooledDown()) return;
        seek(start);
        if (!stream_) return;
    }
    if (warm_) promoteWarmStream(want);
    if (want == shown_) return;

    if (hasTimeline()) {
        const double sp = double(speed());
        // the speed moved enough that the stream decodes the wrong share of frames
        if (stream_->stride != strideFor(sp) && cooledDown()) {
            seek(want);
            if (!stream_) return;
        }

        // where the clock is, told before draining so stale frames are dropped
        {
            std::lock_guard<std::mutex> lk(stream_->m);
            stream_->target = want;
        }
        stream_->room.notify_all();

        // Seeking is for jumps, not for lateness. Being behind is normal on a heavy
        // slide and re-seeking on it only throws the queue away for another startup.
        // Five wall seconds, or a fast playback would count as jumping right away
        const int64_t jump = int64_t(5.0 * info_.fps * std::max(1.0, std::abs(sp)));
        // nothing before the stream's first frame can arrive, pending seek or not
        const bool unreachable = want < stream_base_;
        const bool wandered    = !seek_pending_ && (want < shown_ || want > next_index_ + jump);
        if (cooledDown() && (unreachable || wandered)) {
            seek(want);
            if (!stream_) return;
        }
    }

    uploadWhenAvailable(want, Options::ExportMode);
}

// Pops up to `want` and uploads the newest. An empty queue leaves the previous
// frame up rather than stalling the slideshow, which is how a loaded machine
// degrades. Exports block instead, so a PNG never catches the pre-roll.
void Video::uploadWhenAvailable(int64_t want, bool blocking)
{
    auto s = stream_;
    std::vector<uint8_t> chosen;
    int64_t chosen_index = -1;
    int64_t produced     = 0;
    bool    at_eof       = false;

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (true) {
        {
            std::lock_guard<std::mutex> lk(s->m);
            while (!s->ready.empty() && s->ready.front().index <= want) {
                if (chosen_index >= 0) { s->pool.push_back(std::move(chosen)); ++dropped_; }
                chosen_index = s->ready.front().index;
                chosen       = std::move(s->ready.front().rgba);
                s->ready.pop_front();
            }
            queue_depth_ = int(s->ready.size());
            discarded_   = s->discarded;
            produced     = s->produced;
            at_eof       = s->eof && s->ready.empty();
            if (chosen_index >= 0 || at_eof) break;
        }
        s->room.notify_all();
        if (!blocking) break;
        if (std::chrono::steady_clock::now() > deadline) {
            spdlog::warn("[video] {} has no frame after 10 s, exporting the previous one", path_);
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    if (chosen_index < 0) {
        if (at_eof) {
            if (loop_) {
                // ffmpeg stopped honouring -stream_loop. One that had been
                // feeding restarts at once, the cooldown would show as a freeze,
                // one that died young is a broken file and must wait it out
                const bool worked = produced > int64_t(info_.fps);
                closeStream();
                restart_now_ = worked;
            }
            // the clip is over, hold the last frame and stop asking
            else finished_ = true;
            return;
        }
        ++starves_;
        s->room.notify_all();
        return;
    }

    glBindTexture(GL_TEXTURE_2D, tex_.texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w_, h_, GL_RGBA, GL_UNSIGNED_BYTE,
                    chosen.data());
    ++uploaded_;
    seek_pending_ = false;
    shown_        = chosen_index;
    next_index_   = std::max(next_index_, chosen_index + 1);

    {
        std::lock_guard<std::mutex> lk(s->m);
        s->pool.push_back(std::move(chosen));
    }
    s->room.notify_all();
}

// the texture holds what is on screen, so a snapshot is a read back and not
// another decode, and it is the displayed frame and not a nearby one
std::vector<unsigned char> Video::framePixels() const
{
    if (!isValid() || shown_ < 0) return {};
    std::vector<unsigned char> px(size_t(w_) * size_t(h_) * 4);
    glBindTexture(GL_TEXTURE_2D, tex_.texture);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    return px;
}

bool Video::saveFrame(const std::string& file) const
{
    auto px = framePixels();
    if (px.empty()) return false;

    // saveImage flips, GL framebuffers being bottom up, and this one is not
    const size_t row = size_t(w_) * 4;
    for (int y = 0; y < h_ / 2; ++y)
        std::swap_ranges(px.begin() + y * row, px.begin() + (y + 1) * row,
                         px.begin() + (h_ - 1 - y) * row);

    polyscope::saveImage(file, px.data(), w_, h_, 4);
    spdlog::info("[video] frame {} written to {}", shown_, file);
    return true;
}

void Video::display(const StateInSlide& sis)
{
    anchor->updatePos(sis.getPosition());
    DisplayImage(tex_, sis, sis.getScale());
}

// upright box, so a tilted clip is picked by its bounds
bool Video::rect(const StateInSlide& sis, ImVec2& pmin, ImVec2& pmax) const
{
    if (!isValid()) return false;
    const Size   s = Image::getScaledSize(tex_, sis.getScale());
    const ImVec2 P = sis.getAbsolutePosition();
    pmin = ImVec2(float(P.x - s(0) * 0.5), float(P.y - s(1) * 0.5));
    pmax = ImVec2(float(pmin.x + s(0)),    float(pmin.y + s(1)));
    return true;
}

void Video::handleClick(const StateInSlide& sis)
{
    // nothing to pause on a live source, and an export has no mouse
    if (Options::ExportMode || !hasTimeline()) return;
    // the click that drops a primitive is not a click on what it lands on
    if (DragEditor::isPlacing()) { press_inside_ = false; return; }

    ImVec2 pmin, pmax;
    if (!rect(sis, pmin, pmax)) return;

    const ImGuiIO& io = ImGui::GetIO();
    // a modifier means the click is aimed at the placement tools, dragging a
    // primitive or picking a group, and never at the clip
    if (io.KeyCtrl || io.KeyShift || io.KeyAlt) { press_inside_ = false; return; }

    ImVec2 m = io.MousePos;
    const ImVec2 w = ImGui::GetWindowPos();
    m.x -= w.x; m.y -= w.y;
    const bool over = m.x >= pmin.x && m.x <= pmax.x && m.y >= pmin.y && m.y <= pmax.y;

    if (ImGui::IsMouseClicked(0)) press_inside_ = over;
    if (ImGui::IsMouseReleased(0)) {
        // a drag on the clip is the camera being turned, not a click
        const ImVec2 d = ImGui::GetMouseDragDelta(0);
        if (press_inside_ && over && std::abs(d.x) + std::abs(d.y) < 4.f)
            togglePlay();
        press_inside_ = false;
    }
}

void Video::drawPlayOverlay(const StateInSlide& sis) const
{
    ImVec2 pmin, pmax;
    if (!rect(sis, pmin, pmax)) return;

    const ImVec2 c((pmin.x + pmax.x) * 0.5f, (pmin.y + pmax.y) * 0.5f);
    const float  r = std::min(pmax.x - pmin.x, pmax.y - pmin.y) * 0.12f;
    const float  a = float(sis.alpha);

    auto* dl = ImGui::GetWindowDrawList();
    dl->AddCircleFilled(c, r, IM_COL32(0, 0, 0, int(120 * a)), 48);
    dl->AddTriangleFilled(ImVec2(c.x - r * 0.35f, c.y - r * 0.5f),
                          ImVec2(c.x - r * 0.35f, c.y + r * 0.5f),
                          ImVec2(c.x + r * 0.55f, c.y),
                          IM_COL32(255, 255, 255, int(230 * a)));
}

void Video::drawStats() const
{
    char buf[256];
    std::snprintf(buf, sizeof(buf),
                  "%dx%d  frame %lld  queue %d  uploaded %lld  dropped %lld/%lld  "
                  "starved %lld  seeks %lld",
                  w_, h_, (long long)shown_, queue_depth_, (long long)uploaded_,
                  (long long)dropped_, (long long)discarded_, (long long)starves_,
                  (long long)seeks_);
    ImGui::GetForegroundDrawList()->AddText(ImVec2(20, 20), IM_COL32(255, 220, 80, 255), buf);
}

// the on-screen overlay is invisible in a headless export run
void Video::logStats(const TimeObject& t)
{
    ++steps_;
    if (t.from_begin - last_log_ < 2.0) return;
    // how often the slideshow asked for a frame, when playback stutters this
    // separates "the decoder cannot keep up" from "the whole app is at 5 fps"
    double draw_fps = double(steps_) / (t.from_begin - last_log_);
    last_log_ = t.from_begin;
    steps_    = 0;
    spdlog::info("[video] frame {} queue {} uploaded {} dropped {}/{} starved {} seeks {} "
                 "({:.0f} draws/s)",
                 shown_, queue_depth_, uploaded_, dropped_, discarded_, starves_, seeks_,
                 draw_fps);
}

}
