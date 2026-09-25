#ifndef PLOT_H
#define PLOT_H

#include "content/screen_primitives/plots/Board.h"

namespace slope {

class Plot;
using PlotPtr = std::shared_ptr<Plot>;

/*
 * One curve on a Board. It is a primitive of its own, drawn only where the line is,
 * with the rest transparent.
 *
 *   show << Board::Add("fig", vec2(-M_PI, M_PI), vec2(-1.2, 1.2))->at("figure")
 *        << inNextFrame
 *        << Plot::Add("sine", "fig", [](scalar x){ return std::sin(x); });
 *
 * It goes on the rectangle of the board it names, and it is drawn progressively during
 * the transition that brings it in. The slide where it is added decides when it appears.
 *
 * Sources
 * All five sources give samples over an interval.
 *
 *   Plot::Add("f",   fig, [](scalar x){ return x*x; });   // a C++ callable
 *   Plot::Add("res", fig, "residuals.csv");               // two columns
 *   Plot::Add("pts", fig, points);                        // vector<vec2>
 *   Plot::Add("y",   fig, ys, vec2(0, 10));               // values on a grid
 *   Plot::FromSnippet("lua", fig, "profile");             // a Lua section
 *
 * A callable and a Lua section are sampled over the x range of the board and follow it.
 * A file and a point set have their own range and are read again when they change.
 *
 * The sampling is done in the coordinates of the board. A log axis is therefore drawn in decades of the source
 * and not as a squeezed picture of it. A value that a log axis cannot show goes out of the rectangle.
 *
 * Settings
 *
 *   sine/color       color of the line                from the palette
 *   sine/width       stroke width in pixels           3
 *   sine/reveal      how much is drawn, 0 to 1        1
 *
 * The curve is cut at the shorter of sine/reveal and its own appearance.
 */
class Plot : public Shader, public Legendable {
public:
    using Fn = std::function<scalar(scalar)>;

    // Builds a plot on a board, given as the board or its name (see BoardRef). The board is looked up when first needed.
    // The source is a function, points, values on a grid covering `span`, a csv file, or a Lua section for FromSnippet.
    static PlotPtr Add(const std::string& name, BoardRef board, const Fn& f);
    static PlotPtr Add(const std::string& name, BoardRef board,
                       const std::vector<vec2>& points);
    static PlotPtr Add(const std::string& name, BoardRef board,
                       const std::vector<scalar>& values, const vec2& span);
    static PlotPtr Add(const std::string& name, BoardRef board, const path& csv);
    static PlotPtr FromSnippet(const std::string& name, BoardRef board,
                               const std::string& section);

    std::string name;
    // Text shown in the legend, used instead of the name when set.
    std::string caption;

    // Names of every setting, in the order of the Tuner.
    static const std::vector<std::string>& settingNames();

    Settings settings;

    // True, because it goes on its board and not where a slide would put it.
    bool placesItself() const override { return true; }

    // Legend entry, see Legendable.
    std::string legendCaption() const override { return caption.empty() ? name : caption; }
    bool legendDrawnAt(int frame) const override { return last_frame >= frame - 1; }
    scalar legendAlpha() const override { return appeared; }
    void legendSwatch(ImDrawList* dl, const ImVec2& a, const ImVec2& b,
                      scalar scale, scalar alpha) const override;

    void draw(const TimeObject& t, const StateInSlide& sis) override;
    void playIntro(const TimeObject& t, const StateInSlide& sis) override;
    void playOutro(const TimeObject& t, const StateInSlide& sis) override;

    // Number of samples of any source before it is sent to the GPU.
    static constexpr int kSamples = 1024;

private:
    // Creates the plot and registers it in the legend of the board.
    static PlotPtr make(const std::string& name, const BoardRef& board);
    // The board of this plot, found once and kept with a weak pointer.
    BoardPtr owner() const;
    // Samples again when the interval moved, or at every frame for a live source.
    void refresh();
    // Finds the board, matches its rectangle and samples again. Returns false when there is no board.
    bool prepare(const StateInSlide& sis, float appeared, StateInSlide& on);

    std::string board;
    mutable std::weak_ptr<Board> cached;
    // Progress of the appearance of this plot.
    float appeared = 1;
    // Color from the palette, used when no setting gives one.
    RGBA default_ink;
    // Last frame where it was drawn, read by a legend.
    int last_frame = -1000;
    // True when the size of the rendering matches the board.
    bool sized = false;
    // What the sampling uses. A callable takes a data x.
    // A point source is interpolated in the board coordinates and takes a drawn x.
    Fn f;
    // A point source as it was given.
    std::vector<vec2> raw;
    // Interval sampled, in the coordinates of the board.
    vec2 span = vec2(0, 1);
    // The same interval in data units.
    vec2 data_span = vec2(0, 1);
    // True when the source brought its own x range.
    bool own_span = false;
    // Scales used for the samples now on the GPU.
    bool log_x = false;
    bool log_y = false;
    // Rectangle under which the floor of the samples was set.
    vec2 view_y = vec2(0, 1);
    // True for a Lua section, sampled again while it is edited.
    bool live = false;
    // A csv file, read again when it is saved.
    path file;
    std::filesystem::file_time_type stamp{};
    std::vector<float> samples;
    bool sampled = false;
};

} // namespace slope

#endif // PLOT_H
