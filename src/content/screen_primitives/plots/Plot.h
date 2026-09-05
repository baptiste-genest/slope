#ifndef PLOT_H
#define PLOT_H

#include "content/screen_primitives/plots/Board.h"

namespace slope {

class Plot;
using PlotPtr = std::shared_ptr<Plot>;

/*
 * One curve on a Board, a primitive of its own, with ink only where the line
 * is and the rest transparent.
 *
 *   show << Board::Add("fig", vec2(-M_PI, M_PI), vec2(-1.2, 1.2))->at("figure")
 *        << inNextFrame
 *        << Plot::Add("sine", "fig", [](scalar x){ return std::sin(x); });
 *
 * It lands on the rectangle of the board it names, and draws itself on across
 * the transition that brings it in. Where it appears in the slides is the
 * whole of its sequencing.
 *
 * ── sources ───────────────────────────────────────────────────────────────
 * All five end up as samples over an interval.
 *
 *   Plot::Add("f",   fig, [](scalar x){ return x*x; });   // a C++ callable
 *   Plot::Add("res", fig, "residuals.csv");               // two columns
 *   Plot::Add("pts", fig, points);                        // vector<vec2>
 *   Plot::Add("y",   fig, ys, vec2(0, 10));               // values on a grid
 *   Plot::FromSnippet("lua", fig, "profile");             // a Lua section
 *
 * A callable and a Lua section are sampled over the board's x range and
 * follow it. A file and a point set carry their own, and are re-read live.
 *
 * The sampling is done in the referential of the board, so a log axis is drawn
 * in decades of the source rather than a squeezed picture of it, and a value
 * a log axis cannot show dives out of the rectangle.
 *
 * ── the namespace ─────────────────────────────────────────────────────────
 *
 *   sine/color       the ink                          from the palette
 *   sine/width       stroke width in pixels           3
 *   sine/reveal      how much of it is drawn, 0 to 1  1
 *
 * It is cut at whichever is shorter, sine/reveal or its own arrival.
 */
class Plot : public Shader, public Legendable {
public:
    using Fn = std::function<scalar(scalar)>;

    // the board, or its name (see BoardRef), looked up when first needed
    static PlotPtr Add(const std::string& name, BoardRef board, const Fn& f);
    static PlotPtr Add(const std::string& name, BoardRef board,
                        const std::vector<vec2>& points);
    static PlotPtr Add(const std::string& name, BoardRef board,
                        const std::vector<scalar>& values, const vec2& span);
    static PlotPtr Add(const std::string& name, BoardRef board, const path& csv);
    static PlotPtr FromSnippet(const std::string& name, BoardRef board,
                                const std::string& section);

    std::string name;
    // what a legend calls it, when the name is not what a reader wants
    std::string caption;

    // every setting a plot publishes, in Tuner order
    static const std::vector<std::string>& settingNames();

    Settings settings;

    // it lands on its board rather than where a slide would put it
    bool placesItself() const override {return true;}

    // ── a legend entry (see Legendable) ─────────────────────────────────
    std::string legendCaption() const override {return caption.empty() ? name : caption;}
    bool legendDrawnAt(int frame) const override {return last_frame >= frame - 1;}
    scalar legendAlpha() const override {return appeared;}
    void legendSwatch(ImDrawList* dl, const ImVec2& a, const ImVec2& b,
                      scalar scale, scalar alpha) const override;

    void draw(const TimeObject& t, const StateInSlide& sis) override;
    void playIntro(const TimeObject& t, const StateInSlide& sis) override;
    void playOutro(const TimeObject& t, const StateInSlide& sis) override;

    // how finely any source is sampled before it reaches the GPU
    static constexpr int kSamples = 1024;

private:
    static PlotPtr make(const std::string& name, const BoardRef& board);
    // the board this plot is drawn in, resolved once and held weakly
    BoardPtr owner() const;
    // resamples when the interval moved, or on every frame for a live source
    void refresh();
    // find the board, match its rectangle, resample. False when it has none.
    bool prepare(const StateInSlide& sis, float appeared, StateInSlide& on);

    std::string board;
    mutable std::weak_ptr<Board> cached;
    float appeared = 1;      // how far this plot's own arrival has got
    RGBA default_ink;        // its colour of the palette, if nothing names one
    int last_frame = -1000;  // when it was last drawn, read by a legend
    bool sized = false;      // the render size has been matched to the board's
    // what the sampling reads. A callable takes a data x, a point source is
    // interpolated in the referential and takes a drawn one.
    Fn f;
    std::vector<vec2> raw;       // a point source, as it was given
    vec2 span = vec2(0, 1);      // sampled over this, in the board's referential
    vec2 data_span = vec2(0, 1); // the same interval in data units
    bool own_span = false;   // the source brought its own x range
    bool log_x = false;      // the scales the samples on the GPU were taken in
    bool log_y = false;
    vec2 view_y = vec2(0, 1);// and the rectangle their floor was set under
    bool live = false;       // a Lua section, re-sampled while it is edited
    path file;               // a csv, re-read when it is saved
    std::filesystem::file_time_type stamp{};
    std::vector<float> samples;
    bool sampled = false;
};

}

#endif // PLOT_H
